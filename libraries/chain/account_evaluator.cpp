/*
 * Copyright (c) 2015 Cryptonomex, Inc., and contributors.
 *
 * The MIT License
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */

#include <graphene/chain/account_evaluator.hpp>
#include <graphene/chain/buyback.hpp>
#include <graphene/chain/buyback_object.hpp>
#include <graphene/chain/database.hpp>
#include <graphene/chain/exceptions.hpp>
#include <graphene/chain/hardfork.hpp>
#include <graphene/chain/internal_exceptions.hpp>
#include <graphene/chain/special_authority.hpp>
#include <graphene/chain/special_authority_object.hpp>
#include <graphene/chain/worker_object.hpp>

#include <algorithm>

namespace graphene { namespace chain {

void verify_authority_accounts( const database& db, const authority& a )
{
   const auto& chain_params = db.get_global_properties().parameters;
   GRAPHENE_ASSERT(
      a.num_auths() <= chain_params.maximum_authority_membership,
      internal_verify_auth_max_auth_exceeded,
      "Maximum authority membership exceeded" );
   for( const auto& acnt : a.account_auths )
   {
      GRAPHENE_ASSERT( db.find_object( acnt.first ) != nullptr,
         internal_verify_auth_account_not_found,
         "Account ${a} specified in authority does not exist",
         ("a", acnt.first) );
   }
}

// Overwrites the num_son values from the origin to the destination for those sidechains which are found in the origin.
// Keeps the values of num_son for the sidechains which are found in the destination, but not in the origin.
// Returns false if an error is detected.
bool merge_num_sons( flat_map<sidechain_type, uint16_t>& destination,
                     const flat_map<sidechain_type, uint16_t>& origin,
                     fc::optional<time_point_sec> head_block_time = {})
{
   const auto active_sidechains = head_block_time.valid() ? active_sidechain_types(*head_block_time) : all_sidechain_types;
   bool success = true;

   for (const auto &ns : origin)
   {
      destination[ns.first] = ns.second;
      if (active_sidechains.find(ns.first) == active_sidechains.end())
      {
         success = false;
      }
   }

   return success;
}

flat_map<sidechain_type, uint16_t> count_SON_votes_per_sidechain( const flat_set<vote_id_type>& votes )
{
   flat_map<sidechain_type, uint16_t> SON_votes_per_sidechain = account_options::ext::empty_num_son();

   for (const auto &vote : votes)
   {
      switch (vote.type())
     {
      case vote_id_type::son_bitcoin:
         SON_votes_per_sidechain[sidechain_type::bitcoin]++;
         break;
      case vote_id_type::son_hive:
         SON_votes_per_sidechain[sidechain_type::hive]++;
         break;
      case vote_id_type::son_ethereum:
         SON_votes_per_sidechain[sidechain_type::ethereum]++;
         break;
      default:
         break;
      }
   }

   return SON_votes_per_sidechain;
}

void verify_account_votes( const database& db, const account_options& options, fc::optional<account_object> account = {} )
{
   // ensure account's votes satisfy requirements
   // NB only the part of vote checking that requires chain state is here,
   // the rest occurs in account_options::validate()

   const auto& gpo = db.get_global_properties();
   const auto& chain_params = gpo.parameters;

   FC_ASSERT( db.find_object(options.voting_account), "Invalid proxy account specified." );

   FC_ASSERT( options.num_witness <= chain_params.maximum_witness_count,
              "Voted for more witnesses than currently allowed (${c})", ("c", chain_params.maximum_witness_count) );
   FC_ASSERT( options.num_committee <= chain_params.maximum_committee_count,
              "Voted for more committee members than currently allowed (${c})", ("c", chain_params.maximum_committee_count) );
   FC_ASSERT( chain_params.extensions.value.maximum_son_count.valid() , "Invalid maximum son count" );

   flat_map<sidechain_type, uint16_t> merged_num_sons = account_options::ext::empty_num_son();

   // Merge with existing account if exists
   if ( account.valid() && account->options.extensions.value.num_son.valid())
   {
      merge_num_sons( merged_num_sons, *account->options.extensions.value.num_son, db.head_block_time() );
   }

   // Apply update operation on top
   if ( options.extensions.value.num_son.valid() )
   {
      merge_num_sons( merged_num_sons, *options.extensions.value.num_son, db.head_block_time() );
   }

   for(const auto& num_sons : merged_num_sons)
   {
      FC_ASSERT( num_sons.second <= *chain_params.extensions.value.maximum_son_count,
                "Voted for more sons than currently allowed (${c})", ("c", *chain_params.extensions.value.maximum_son_count) );
   }

   // Count the votes for SONs and confirm that the account did not vote for less SONs than num_son
   flat_map<sidechain_type, uint16_t> SON_votes_per_sidechain = count_SON_votes_per_sidechain(options.votes);

   for (const auto& number_of_votes : SON_votes_per_sidechain)
   {
      // Number of votes of account_options are also checked in account_options::do_evaluate,
      // but there we are checking the value before merging num_sons, so the values should be checked again
      const auto sidechain = number_of_votes.first;
      FC_ASSERT( number_of_votes.second >= merged_num_sons[sidechain],
                "Voted for less sons than specified in num_son (votes ${v} < num_son ${ns}) for sidechain ${s}",
                ("v", number_of_votes.second) ("ns", merged_num_sons[sidechain]) ("s", sidechain) );
   }

   FC_ASSERT( db.find_object(options.voting_account), "Invalid proxy account specified." );

   uint32_t max_vote_id = gpo.next_available_vote_id;
   bool has_worker_votes = false;
   for( auto id : options.votes )
   {
      FC_ASSERT( id < max_vote_id );
      has_worker_votes |= (id.type() == vote_id_type::worker);
   }

   if( has_worker_votes && (db.head_block_time() >= HARDFORK_607_TIME) )
   {
      const auto& against_worker_idx = db.get_index_type<worker_index>().indices().get<by_vote_against>();
      for( auto id : options.votes )
      {
         if( id.type() == vote_id_type::worker )
         {
            FC_ASSERT( against_worker_idx.find( id ) == against_worker_idx.end() );
         }
      }
   }

}


void_result account_create_evaluator::do_evaluate( const account_create_operation& op )
{ try {
   database& d = db();
   if( d.head_block_time() < HARDFORK_516_TIME )
   {
      FC_ASSERT( !op.extensions.value.owner_special_authority.valid() );
      FC_ASSERT( !op.extensions.value.active_special_authority.valid() );
   }
   if( d.head_block_time() < HARDFORK_599_TIME )
   {
      FC_ASSERT( !op.extensions.value.null_ext.valid() );
      FC_ASSERT( !op.extensions.value.owner_special_authority.valid() );
      FC_ASSERT( !op.extensions.value.active_special_authority.valid() );
      FC_ASSERT( !op.extensions.value.buyback_options.valid() );
   }
   if( d.head_block_time() < HARDFORK_999_TIME )
      FC_ASSERT( !op.extensions.value.affiliate_distributions.valid(), "Affiliate reward distributions not allowed yet" );
   if (d.head_block_time() < HARDFORK_SON_TIME)
      FC_ASSERT(op.name != "son-account", "Son account creation before SON hardfork");

   FC_ASSERT( fee_paying_account->is_lifetime_member(), "Only Lifetime members may register an account." );
   FC_ASSERT( op.referrer(d).is_member(d.head_block_time()), "The referrer must be either a lifetime or annual subscriber." );

   try
   {
      verify_authority_accounts( d, op.owner );
      verify_authority_accounts( d, op.active );
   }
   GRAPHENE_RECODE_EXC( internal_verify_auth_max_auth_exceeded, account_create_max_auth_exceeded )
   GRAPHENE_RECODE_EXC( internal_verify_auth_account_not_found, account_create_auth_account_not_found )

   if( op.extensions.value.owner_special_authority.valid() )
      evaluate_special_authority( d, *op.extensions.value.owner_special_authority );
   if( op.extensions.value.active_special_authority.valid() )
      evaluate_special_authority( d, *op.extensions.value.active_special_authority );
   if( op.extensions.value.buyback_options.valid() )
      evaluate_buyback_account_options( d, *op.extensions.value.buyback_options );
   verify_account_votes( d, op.options );

   auto& acnt_indx = d.get_index_type<account_index>();
   if( op.name.size() )
   {
      auto current_account_itr = acnt_indx.indices().get<by_name>().find( op.name );
      FC_ASSERT( current_account_itr == acnt_indx.indices().get<by_name>().end() );
   }

   return void_result();
} FC_CAPTURE_AND_RETHROW( (op) ) }

object_id_type account_create_evaluator::do_apply( const account_create_operation& o )
{ try {

   database& d = db();
   uint16_t referrer_percent = o.referrer_percent;
   bool has_small_percent = (
         (db().head_block_time() <= HARDFORK_453_TIME)
      && (o.referrer != o.registrar  )
      && (o.referrer_percent != 0    )
      && (o.referrer_percent <= 0x100)
      );

   if( has_small_percent )
   {
      if( referrer_percent >= 100 )
      {
         wlog( "between 100% and 0x100%:  ${o}", ("o", o) );
      }
      referrer_percent = referrer_percent*100;
      if( referrer_percent > GRAPHENE_100_PERCENT )
         referrer_percent = GRAPHENE_100_PERCENT;
   }
   const auto& global_properties = d.get_global_properties();

   const auto& new_acnt_object = d.create<account_object>( [&o,&d,&global_properties,referrer_percent]( account_object& obj )
   {
      obj.registrar = o.registrar;
      obj.referrer = o.referrer;
      obj.lifetime_referrer = o.referrer(d).lifetime_referrer;

      const auto& params = global_properties.parameters;
      obj.network_fee_percentage = params.network_percent_of_fee;
      obj.lifetime_referrer_fee_percentage = params.lifetime_referrer_percent_of_fee;
      obj.referrer_rewards_percentage = referrer_percent;

      obj.name             = o.name;
      obj.owner            = o.owner;
      obj.active           = o.active;
      obj.options          = o.options;

      obj.options.extensions.value.num_son = account_options::ext::empty_num_son();
      if ( o.options.extensions.value.num_son.valid() )
      {
         merge_num_sons( *obj.options.extensions.value.num_son, *o.options.extensions.value.num_son );
      }

      obj.statistics = d.create<account_statistics_object>([&obj](account_statistics_object& s){
                           s.owner = obj.id;
                           s.name = obj.name;
                           s.is_voting = obj.options.is_voting();
                        }).id;

      if( o.extensions.value.owner_special_authority.valid() )
         obj.owner_special_authority = *(o.extensions.value.owner_special_authority);
      if( o.extensions.value.active_special_authority.valid() )
         obj.active_special_authority = *(o.extensions.value.active_special_authority);
      if( o.extensions.value.buyback_options.valid() )
      {
         obj.allowed_assets = o.extensions.value.buyback_options->markets;
         obj.allowed_assets->emplace( o.extensions.value.buyback_options->asset_to_buy );
      }
      obj.affiliate_distributions = o.extensions.value.affiliate_distributions;
   });

   if( has_small_percent )
   {
      wlog( "Account affected by #453 registered in block ${n}:  ${na} reg=${reg} ref=${ref}:${refp} ltr=${ltr}:${ltrp}",
         ("n", db().head_block_num()) ("na", new_acnt_object.id)
         ("reg", o.registrar) ("ref", o.referrer) ("ltr", new_acnt_object.lifetime_referrer)
         ("refp", new_acnt_object.referrer_rewards_percentage) ("ltrp", new_acnt_object.lifetime_referrer_fee_percentage) );
      wlog( "Affected account object is ${o}", ("o", new_acnt_object) );
   }

   const auto& dynamic_properties = d.get_dynamic_global_properties();
   d.modify(dynamic_properties, [](dynamic_global_property_object& p) {
      ++p.accounts_registered_this_interval;
   });

   if( dynamic_properties.accounts_registered_this_interval % global_properties.parameters.accounts_per_fee_scale == 0
         && global_properties.parameters.account_fee_scale_bitshifts != 0 )
   {
      d.modify(global_properties, [&dynamic_properties](global_property_object& p) {
         p.parameters.current_fees->get<account_create_operation>().basic_fee <<= p.parameters.account_fee_scale_bitshifts;
      });
   }

   if(    o.extensions.value.owner_special_authority.valid()
       || o.extensions.value.active_special_authority.valid() )
   {
      db().create< special_authority_object >( [&]( special_authority_object& sa )
      {
         sa.account = new_acnt_object.id;
      } );
   }

   if( o.extensions.value.buyback_options.valid() )
   {
      asset_id_type asset_to_buy = o.extensions.value.buyback_options->asset_to_buy;

      d.create< buyback_object >( [&]( buyback_object& bo )
      {
         bo.asset_to_buy = asset_to_buy;
      } );

      d.modify( asset_to_buy(d), [&]( asset_object& a )
      {
         a.buyback_account = new_acnt_object.id;
      } );
   }

   return new_acnt_object.id;
} FC_CAPTURE_AND_RETHROW((o)) }


void_result account_update_evaluator::do_evaluate( const account_update_operation& o )
{ try {
   database& d = db();
   if( d.head_block_time() < HARDFORK_516_TIME )
   {
      FC_ASSERT( !o.extensions.value.owner_special_authority.valid() );
      FC_ASSERT( !o.extensions.value.active_special_authority.valid() );
   }
   if( d.head_block_time() < HARDFORK_599_TIME )
   {
      FC_ASSERT( !o.extensions.value.null_ext.valid() );
      FC_ASSERT( !o.extensions.value.owner_special_authority.valid() );
      FC_ASSERT( !o.extensions.value.active_special_authority.valid() );
   }

   try
   {
      if( o.owner )  verify_authority_accounts( d, *o.owner );
      if( o.active ) verify_authority_accounts( d, *o.active );
   }
   GRAPHENE_RECODE_EXC( internal_verify_auth_max_auth_exceeded, account_update_max_auth_exceeded )
   GRAPHENE_RECODE_EXC( internal_verify_auth_account_not_found, account_update_auth_account_not_found )

   if( o.extensions.value.owner_special_authority.valid() )
      evaluate_special_authority( d, *o.extensions.value.owner_special_authority );
   if( o.extensions.value.active_special_authority.valid() )
      evaluate_special_authority( d, *o.extensions.value.active_special_authority );

   acnt = &o.account(d);

   if( o.new_options.valid() )
      verify_account_votes( d, *o.new_options, *acnt );

   return void_result();
} FC_CAPTURE_AND_RETHROW( (o) ) }

void_result account_update_evaluator::do_apply( const account_update_operation& o )
{ try {
   database& d = db();

   bool sa_before = acnt->has_special_authority();

   // update account statistics
   if( o.new_options.valid() )
   {
      d.modify( acnt->statistics( d ), [&]( account_statistics_object& aso )
      {
         fc::optional< bool > flag = o.extensions.value.update_last_voting_time;
         if((o.new_options->votes != acnt->options.votes ||
             o.new_options->voting_account != acnt->options.voting_account) ||
             (flag.valid() && *flag))
            aso.last_vote_time = d.head_block_time();
        
         if(o.new_options->is_voting() != acnt->options.is_voting())
            aso.is_voting = !aso.is_voting;
      } );
   }

   // update account object
   d.modify( *acnt, [&o](account_object& a){
      if( o.owner )
      {
         a.owner = *o.owner;
         a.top_n_control_flags = 0;
      }
      if( o.active )
      {
         a.active = *o.active;
         a.top_n_control_flags = 0;
      }

      // New num_son structure initialized to 0
      flat_map<sidechain_type, uint16_t> new_num_son = account_options::ext::empty_num_son();

      // If num_son of existing object is valid, we should merge the existing data
      if ( a.options.extensions.value.num_son.valid() )
      {
         merge_num_sons( new_num_son, *a.options.extensions.value.num_son );
      }

      // If num_son of the operation are valid, they should merge the existing data
      if ( o.new_options )
      {
         const auto new_options = *o.new_options;

         if ( new_options.extensions.value.num_son.valid() )
         {
            merge_num_sons( new_num_son, *new_options.extensions.value.num_son );
         }

         a.options = *o.new_options;
      }

      a.options.extensions.value.num_son = new_num_son;

      if( o.extensions.value.owner_special_authority.valid() )
      {
         a.owner_special_authority = *(o.extensions.value.owner_special_authority);
         a.top_n_control_flags = 0;
      }
      if( o.extensions.value.active_special_authority.valid() )
      {
         a.active_special_authority = *(o.extensions.value.active_special_authority);
         a.top_n_control_flags = 0;
      }
   });

   bool sa_after = acnt->has_special_authority();

   if( sa_before & (!sa_after) )
   {
      const auto& sa_idx = d.get_index_type< special_authority_index >().indices().get<by_account>();
      auto sa_it = sa_idx.find( o.account );
      assert( sa_it != sa_idx.end() );
      d.remove( *sa_it );
   }
   else if( (!sa_before) & sa_after )
   {
      d.create< special_authority_object >( [&]( special_authority_object& sa )
      {
         sa.account = o.account;
      } );
   }
   return void_result();
} FC_CAPTURE_AND_RETHROW( (o) ) }

void_result account_whitelist_evaluator::do_evaluate(const account_whitelist_operation& o)
{ try {
   database& d = db();

   listed_account = &o.account_to_list(d);
   if( !d.get_global_properties().parameters.allow_non_member_whitelists )
      FC_ASSERT(o.authorizing_account(d).is_lifetime_member());

   return void_result();
} FC_CAPTURE_AND_RETHROW( (o) ) }

void_result account_whitelist_evaluator::do_apply(const account_whitelist_operation& o)
{ try {
   database& d = db();

   d.modify(*listed_account, [&o](account_object& a) {
      if( o.new_listing & o.white_listed )
         a.whitelisting_accounts.insert(o.authorizing_account);
      else
         a.whitelisting_accounts.erase(o.authorizing_account);

      if( o.new_listing & o.black_listed )
         a.blacklisting_accounts.insert(o.authorizing_account);
      else
         a.blacklisting_accounts.erase(o.authorizing_account);
   });

   /** for tracking purposes only, this state is not needed to evaluate */
   d.modify( o.authorizing_account(d), [&]( account_object& a ) {
     if( o.new_listing & o.white_listed )
        a.whitelisted_accounts.insert( o.account_to_list );
     else
        a.whitelisted_accounts.erase( o.account_to_list );

     if( o.new_listing & o.black_listed )
        a.blacklisted_accounts.insert( o.account_to_list );
     else
        a.blacklisted_accounts.erase( o.account_to_list );
   });

   return void_result();
} FC_CAPTURE_AND_RETHROW( (o) ) }

void_result account_upgrade_evaluator::do_evaluate(const account_upgrade_evaluator::operation_type& o)
{ try {
   database& d = db();

   account = &d.get(o.account_to_upgrade);
   FC_ASSERT(!account->is_lifetime_member());

   return {};
//} FC_CAPTURE_AND_RETHROW( (o) ) }
} FC_RETHROW_EXCEPTIONS( error, "Unable to upgrade account '${a}'", ("a",o.account_to_upgrade(db()).name) ) }

void_result account_upgrade_evaluator::do_apply(const account_upgrade_evaluator::operation_type& o)
{ try {
   database& d = db();

   d.modify(*account, [&](account_object& a) {
      if( o.upgrade_to_lifetime_member )
      {
         // Upgrade to lifetime member. I don't care what the account was before.
         a.statistics(d).process_fees(a, d);
         a.membership_expiration_date = time_point_sec::maximum();
         a.referrer = a.registrar = a.lifetime_referrer = a.get_id();
         a.lifetime_referrer_fee_percentage = GRAPHENE_100_PERCENT - a.network_fee_percentage;
      } else if( a.is_annual_member(d.head_block_time()) ) {
         // Renew an annual subscription that's still in effect.
         FC_ASSERT( d.head_block_time() <= HARDFORK_613_TIME );
         FC_ASSERT(a.membership_expiration_date - d.head_block_time() < fc::days(3650),
                   "May not extend annual membership more than a decade into the future.");
         a.membership_expiration_date += fc::days(365);
      } else {
         // Upgrade from basic account.
         FC_ASSERT( d.head_block_time() <= HARDFORK_613_TIME );
         a.statistics(d).process_fees(a, d);
         assert(a.is_basic_account(d.head_block_time()));
         a.referrer = a.get_id();
         a.membership_expiration_date = d.head_block_time() + fc::days(365);
      }
   });

   return {};
} FC_RETHROW_EXCEPTIONS( error, "Unable to upgrade account '${a}'", ("a",o.account_to_upgrade(db()).name) ) }

} } // graphene::chain
