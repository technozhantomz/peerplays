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

#include <boost/multiprecision/integer.hpp>

#include <fc/uint128.hpp>

#include <graphene/chain/database.hpp>
#include <graphene/chain/fba_accumulator_id.hpp>
#include <graphene/chain/hardfork.hpp>
#include <graphene/chain/is_authorized_asset.hpp>

#include <graphene/chain/account_object.hpp>
#include <graphene/chain/account_role_object.hpp>
#include <graphene/chain/asset_object.hpp>
#include <graphene/chain/budget_record_object.hpp>
#include <graphene/chain/buyback_object.hpp>
#include <graphene/chain/chain_property_object.hpp>
#include <graphene/chain/committee_member_object.hpp>
#include <graphene/chain/custom_account_authority_object.hpp>
#include <graphene/chain/fba_object.hpp>
#include <graphene/chain/global_property_object.hpp>
#include <graphene/chain/market_object.hpp>
#include <graphene/chain/special_authority_object.hpp>
#include <graphene/chain/son_object.hpp>
#include <graphene/chain/son_wallet_object.hpp>
#include <graphene/chain/vesting_balance_object.hpp>
#include <graphene/chain/vote_count.hpp>
#include <graphene/chain/witness_object.hpp>
#include <graphene/chain/witness_schedule_object.hpp>
#include <graphene/chain/worker_object.hpp>

namespace graphene { namespace chain {

template<class Index>
vector<std::reference_wrapper<const typename Index::object_type>> database::sort_votable_objects(size_t count) const
{
   using ObjectType = typename Index::object_type;
   const auto& all_objects = get_index_type<Index>().indices();
   count = std::min(count, all_objects.size());
   vector<std::reference_wrapper<const ObjectType>> refs;
   refs.reserve(all_objects.size());
   std::transform(all_objects.begin(), all_objects.end(),
                  std::back_inserter(refs),
                  [](const ObjectType& o) { return std::cref(o); });
   std::partial_sort(refs.begin(), refs.begin() + count, refs.end(),
                   [this](const ObjectType& a, const ObjectType& b)->bool {
      share_type oa_vote = _vote_tally_buffer[a.vote_id];
      share_type ob_vote = _vote_tally_buffer[b.vote_id];
      if( oa_vote != ob_vote )
         return oa_vote > ob_vote;
      return a.vote_id < b.vote_id;
   });

   refs.resize(count, refs.front());
   return refs;
}

template<>
vector<std::reference_wrapper<const son_object>> database::sort_votable_objects<son_index>(size_t count) const
{
   const auto& all_sons = get_index_type<son_index>().indices().get< by_id >();
   std::vector<std::reference_wrapper<const son_object>> refs;
   for( auto& son : all_sons )
   {
      if(son.has_valid_config(head_block_time()) && son.status != son_status::deregistered)
      {
         refs.push_back(std::cref(son));
      }
   }
   count = std::min(count, refs.size());
   std::partial_sort(refs.begin(), refs.begin() + count, refs.end(),
                   [this](const son_object& a, const son_object& b)->bool {
      share_type oa_vote = _vote_tally_buffer[a.vote_id];
      share_type ob_vote = _vote_tally_buffer[b.vote_id];
      if( oa_vote != ob_vote )
         return oa_vote > ob_vote;
      return a.vote_id < b.vote_id;
   });

   refs.resize(count, refs.front());
   return refs;
}

template<class Type>
void database::perform_account_maintenance(Type tally_helper)
{
   const auto& bal_idx = get_index_type< account_balance_index >().indices().get< by_maintenance_flag >();
   if( bal_idx.begin() != bal_idx.end() )
   {
      auto bal_itr = bal_idx.rbegin();
      while( bal_itr->maintenance_flag )
      {
         const account_balance_object& bal_obj = *bal_itr;

         modify( get_account_stats_by_owner( bal_obj.owner ), [&bal_obj](account_statistics_object& aso) {
            aso.core_in_balance = bal_obj.balance;
         });

         modify( bal_obj, []( account_balance_object& abo ) {
            abo.maintenance_flag = false;
         });

         bal_itr = bal_idx.rbegin();
      }
   }

   const auto& stats_idx = get_index_type< account_stats_index >().indices().get< by_maintenance_seq >();
   auto stats_itr = stats_idx.lower_bound( true );

   while( stats_itr != stats_idx.end() )
   {
      const account_statistics_object& acc_stat = *stats_itr;
      const account_object& acc_obj = acc_stat.owner( *this );
      ++stats_itr;

      if( acc_stat.has_some_core_voting() )
         tally_helper( acc_obj, acc_stat );

      if( acc_stat.has_pending_fees() )
         acc_stat.process_fees( acc_obj, *this );
   }
}

/// @brief A visitor for @ref worker_type which calls pay_worker on the worker within
struct worker_pay_visitor
{
   private:
      share_type pay;
      database& db;

   public:
      worker_pay_visitor(share_type pay, database& db)
         : pay(pay), db(db) {}

      typedef void result_type;
      template<typename W>
      void operator()(W& worker)const
      {
         worker.pay_worker(pay, db);
      }
};
void database::update_worker_votes()
{
   auto& idx = get_index_type<worker_index>();
   auto itr = idx.indices().get<by_account>().begin();
   bool allow_negative_votes = (head_block_time() < HARDFORK_607_TIME);
   while( itr != idx.indices().get<by_account>().end() )
   {
      modify( *itr, [&]( worker_object& obj ){
         obj.total_votes_for = _vote_tally_buffer[obj.vote_for];
         obj.total_votes_against = allow_negative_votes ? _vote_tally_buffer[obj.vote_against] : 0;
      });
      ++itr;
   }
}

void database::pay_sons()
{
   time_point_sec now = head_block_time();
   const dynamic_global_property_object& dpo = get_dynamic_global_properties();
   // Current requirement is that we have to pay every 24 hours, so the following check
   if( dpo.son_budget.value > 0 && ((now - dpo.last_son_payout_time) >= fc::seconds(get_global_properties().parameters.son_pay_time()))) {
      auto sons = sort_votable_objects<son_index>(get_global_properties().parameters.maximum_son_count());
      // After SON2 HF
      uint64_t total_votes = 0;
      for( const son_object& son : sons )
      {
         total_votes += _vote_tally_buffer[son.vote_id];
      }
      int8_t bits_to_drop = std::max(int(boost::multiprecision::detail::find_msb(total_votes)) - 15, 0);
      auto get_weight = [&bits_to_drop]( uint64_t son_votes ) {
         uint16_t weight = std::max((son_votes >> bits_to_drop), uint64_t(1) );
         return weight;
      };
      // Before SON2 HF
      auto get_weight_before_son2_hf = []( uint64_t son_votes ) {
         int8_t bits_to_drop = std::max(int(boost::multiprecision::detail::find_msb(son_votes)) - 15, 0);
         uint16_t weight = std::max((son_votes >> bits_to_drop), uint64_t(1) );
         return weight;
      };
      uint64_t weighted_total_txs_signed = 0;
      share_type son_budget = dpo.son_budget;
      get_index_type<son_stats_index>().inspect_all_objects([this, &weighted_total_txs_signed, &get_weight, &now, &get_weight_before_son2_hf](const object& o) {
         const son_statistics_object& s = static_cast<const son_statistics_object&>(o);
         const auto& idx = get_index_type<son_index>().indices().get<by_id>();
         auto son_obj = idx.find( s.owner );
         auto son_weight = get_weight(_vote_tally_buffer[son_obj->vote_id]);
         if( now < HARDFORK_SON2_TIME ) {
            son_weight = get_weight_before_son2_hf(_vote_tally_buffer[son_obj->vote_id]);
         }
         uint64_t txs_signed = 0;
         for (const auto &ts : s.txs_signed) {
            txs_signed = txs_signed + ts.second;
         }
         weighted_total_txs_signed += (txs_signed * son_weight);
      });

      // Now pay off each SON proportional to the number of transactions signed.
      get_index_type<son_stats_index>().inspect_all_objects([this, &weighted_total_txs_signed, &dpo, &son_budget, &get_weight, &get_weight_before_son2_hf, &now](const object& o) {
         const son_statistics_object& s = static_cast<const son_statistics_object&>(o);
         uint64_t txs_signed = 0;
         for (const auto &ts : s.txs_signed) {
            txs_signed = txs_signed + ts.second;
         }

         if(txs_signed > 0){
            const auto& idx = get_index_type<son_index>().indices().get<by_id>();
            auto son_obj = idx.find( s.owner );
            auto son_weight = get_weight(_vote_tally_buffer[son_obj->vote_id]);
            if( now < HARDFORK_SON2_TIME ) {
               son_weight = get_weight_before_son2_hf(_vote_tally_buffer[son_obj->vote_id]);
            }
            share_type pay = (txs_signed * son_weight * son_budget.value)/weighted_total_txs_signed;
            modify( *son_obj, [&]( son_object& _son_obj)
            {
               _son_obj.pay_son_fee(pay, *this);
            });
            //Remove the amount paid out to SON from global SON Budget
            modify( dpo, [&]( dynamic_global_property_object& _dpo )
            {
               _dpo.son_budget -= pay;
            } );
            //Reset the tx counter in each son statistics object
            modify( s, [&]( son_statistics_object& _s)
            {
               for (const auto &ts : s.txs_signed) {
                  _s.txs_signed.at(ts.first) = 0;
               }
            });
         }
      });
      //Note the last son pay out time
      modify( dpo, [&]( dynamic_global_property_object& _dpo )
      {
         _dpo.last_son_payout_time = now;
      });
   }
}

void database::update_son_metrics(const vector<son_info>& curr_active_sons)
{
   vector<son_id_type> current_sons;

   current_sons.reserve(curr_active_sons.size());
   std::transform(curr_active_sons.begin(), curr_active_sons.end(),
                  std::inserter(current_sons, current_sons.end()),
                  [](const son_info &swi) {
                     return swi.son_id;
                  });

   const auto& son_idx = get_index_type<son_index>().indices().get< by_id >();
   for( auto& son : son_idx )
   {
      auto& stats = son.statistics(*this);
      bool is_active_son = (std::find(current_sons.begin(), current_sons.end(), son.id) != current_sons.end());
      modify( stats, [&]( son_statistics_object& _stats )
      {
         if(is_active_son) {
            _stats.total_voted_time = _stats.total_voted_time + get_global_properties().parameters.maintenance_interval;
         }
         _stats.total_downtime += _stats.current_interval_downtime;
         _stats.current_interval_downtime = 0;
         for (const auto &str : _stats.sidechain_txs_reported) {
            _stats.sidechain_txs_reported.at(str.first) = 0;
         }
      });
   }
}

void database::update_son_statuses(const vector<son_info>& curr_active_sons, const vector<son_info>& new_active_sons)
{
   vector<son_id_type> current_sons, new_sons;
   vector<son_id_type> sons_to_remove, sons_to_add;
   const auto& idx = get_index_type<son_index>().indices().get<by_id>();

   current_sons.reserve(curr_active_sons.size());
   std::transform(curr_active_sons.begin(), curr_active_sons.end(),
                  std::inserter(current_sons, current_sons.end()),
                  [](const son_info &swi) {
                     return swi.son_id;
                  });

   new_sons.reserve(new_active_sons.size());
   std::transform(new_active_sons.begin(), new_active_sons.end(),
                  std::inserter(new_sons, new_sons.end()),
                  [](const son_info &swi) {
                     return swi.son_id;
                  });

   // find all cur_active_sons members that is not in new_active_sons
   for_each(current_sons.begin(), current_sons.end(),
            [&sons_to_remove, &new_sons](const son_id_type& si)
            {
               if(std::find(new_sons.begin(), new_sons.end(), si) ==
                     new_sons.end())
               {
                  sons_to_remove.push_back(si);
               }
            }
   );

   for( const auto& sid : sons_to_remove )
   {
      auto son = idx.find( sid );
      if(son == idx.end()) // SON is deleted already
         continue;
      // keep maintenance status for nodes becoming inactive
      if(son->status == son_status::active)
      {
         modify( *son, [&]( son_object& obj ){
                  obj.status = son_status::inactive;
                  });
      }
   }

   // find all new_active_sons members that is not in cur_active_sons
   for_each(new_sons.begin(), new_sons.end(),
            [&sons_to_add, &current_sons](const son_id_type& si)
            {
               if(std::find(current_sons.begin(), current_sons.end(), si) ==
                     current_sons.end())
               {
                  sons_to_add.push_back(si);
               }
            }
   );

   for( const auto& sid : sons_to_add )
   {
      auto son = idx.find( sid );
      FC_ASSERT(son != idx.end(), "Invalid SON in active list, id={sonid}.", ("sonid", sid));
      // keep maintenance status for new nodes
      if(son->status == son_status::inactive)
      {
         modify( *son, [&]( son_object& obj ){
                  obj.status = son_status::active;
                  });
      }
   }

   ilog("New SONS");
   for(size_t i = 0; i < new_sons.size(); i++) {
         auto son = idx.find( new_sons[i] );
         if(son == idx.end()) // SON is deleted already
            continue;
      ilog( "${s}, status = ${ss}, total_votes = ${sv}", ("s", new_sons[i])("ss", son->status)("sv", son->total_votes) );
   }

   if( sons_to_remove.size() > 0 )
   {
      remove_inactive_son_proposals(sons_to_remove);
   }
}

void database::update_son_wallet(const vector<son_info>& new_active_sons)
{
   bool should_recreate_pw = true;

   // Expire for current son_wallet_object wallet, if exists
   const auto& idx_swi = get_index_type<son_wallet_index>().indices().get<by_id>();
   auto obj = idx_swi.rbegin();
   if (obj != idx_swi.rend()) {
      // Compare current wallet SONs and to-be lists of active sons
      auto cur_wallet_sons = (*obj).sons;

      bool wallet_son_sets_equal = (cur_wallet_sons.size() == new_active_sons.size());
      if (wallet_son_sets_equal) {
         for( size_t i = 0; i < cur_wallet_sons.size(); i++ ) {
            wallet_son_sets_equal = wallet_son_sets_equal && cur_wallet_sons.at(i) == new_active_sons.at(i);
         }
      }

      should_recreate_pw = !wallet_son_sets_equal;

      if (should_recreate_pw) {
         modify(*obj, [&, obj](son_wallet_object &swo) {
            swo.expires = head_block_time();
         });
      }
   }

   should_recreate_pw = should_recreate_pw && (new_active_sons.size() >= get_chain_properties().immutable_parameters.min_son_count);

   if (should_recreate_pw) {
      // Create new son_wallet_object, to initiate wallet recreation
      create<son_wallet_object>( [&]( son_wallet_object& obj ) {
         obj.valid_from = head_block_time();
         obj.expires = time_point_sec::maximum();
         obj.sons.insert(obj.sons.end(), new_active_sons.begin(), new_active_sons.end());
      });
   }
}

void database::pay_workers( share_type& budget )
{
   const auto head_time = head_block_time();
//   ilog("Processing payroll! Available budget is ${b}", ("b", budget));
   vector<std::reference_wrapper<const worker_object>> active_workers;
   // TODO optimization: add by_expiration index to avoid iterating through all objects
   get_index_type<worker_index>().inspect_all_objects([head_time, &active_workers](const object& o) {
      const worker_object& w = static_cast<const worker_object&>(o);
      if( w.is_active(head_time) && w.approving_stake() > 0 )
         active_workers.emplace_back(w);
   });

   // worker with more votes is preferred
   // if two workers exactly tie for votes, worker with lower ID is preferred
   std::sort(active_workers.begin(), active_workers.end(), [this](const worker_object& wa, const worker_object& wb) {
      share_type wa_vote = wa.approving_stake();
      share_type wb_vote = wb.approving_stake();
      if( wa_vote != wb_vote )
         return wa_vote > wb_vote;
      return wa.id < wb.id;
   });

   const auto last_budget_time = get_dynamic_global_properties().last_budget_time;
   const auto passed_time_ms = head_time - last_budget_time;
   const auto passed_time_count = passed_time_ms.count();
   const auto day_count = fc::days(1).count();
   for( uint32_t i = 0; i < active_workers.size() && budget > 0; ++i )
   {
      const worker_object& active_worker = active_workers[i];
      share_type requested_pay = active_worker.daily_pay;

      // Note: if there is a good chance that passed_time_count == day_count,
      //       for better performance, can avoid the 128 bit calculation by adding a check.
      //       Since it's not the case on BitShares mainnet, we're not using a check here.
      fc::uint128 pay(requested_pay.value);
      pay *= passed_time_count;
      pay /= day_count;
      requested_pay = pay.to_uint64();

      share_type actual_pay = std::min(budget, requested_pay);
      //ilog(" ==> Paying ${a} to worker ${w}", ("w", active_worker.id)("a", actual_pay));
      modify(active_worker, [&](worker_object& w) {
         w.worker.visit(worker_pay_visitor(actual_pay, *this));
      });

      budget -= actual_pay;
   }
}

void database::update_active_witnesses()
{ try {
   assert( _witness_count_histogram_buffer.size() > 0 );
   share_type stake_target = (_total_voting_stake-_witness_count_histogram_buffer[0]) / 2;

   /// accounts that vote for 0 or 1 witness do not get to express an opinion on
   /// the number of witnesses to have (they abstain and are non-voting accounts)

   share_type stake_tally = 0;

   size_t witness_count = 0;
   if( stake_target > 0 )
   {
      while( (witness_count < _witness_count_histogram_buffer.size() - 1)
             && (stake_tally <= stake_target) )
      {
         stake_tally += _witness_count_histogram_buffer[++witness_count];
      }
   }

   const chain_property_object& cpo = get_chain_properties();
   auto wits = sort_votable_objects<witness_index>(std::max(witness_count*2+1, (size_t)cpo.immutable_parameters.min_witness_count));

   const global_property_object& gpo = get_global_properties();

   auto update_witness_total_votes = [this]( const witness_object& wit ) {
      modify( wit, [this]( witness_object& obj )
      {
         obj.total_votes = _vote_tally_buffer[obj.vote_id];
      });
   };

   if( _track_standby_votes )
   {
      const auto& all_witnesses = get_index_type<witness_index>().indices();
      for( const witness_object& wit : all_witnesses )
      {
         update_witness_total_votes( wit );
      }
   }
   else
   {
      for( const witness_object& wit : wits )
      {
         update_witness_total_votes( wit );
      }
   }

   // Update witness authority
   modify( get(GRAPHENE_WITNESS_ACCOUNT), [&]( account_object& a )
   {
      if( head_block_time() < HARDFORK_533_TIME )
      {
         uint64_t total_votes = 0;
         map<account_id_type, uint64_t> weights;
         a.active.weight_threshold = 0;
         a.active.clear();

         for( const witness_object& wit : wits )
         {
            weights.emplace(wit.witness_account, _vote_tally_buffer[wit.vote_id]);
            total_votes += _vote_tally_buffer[wit.vote_id];
         }

         // total_votes is 64 bits. Subtract the number of leading low bits from 64 to get the number of useful bits,
         // then I want to keep the most significant 16 bits of what's left.
         int8_t bits_to_drop = std::max(int(boost::multiprecision::detail::find_msb(total_votes)) - 15, 0);
         for( const auto& weight : weights )
         {
            // Ensure that everyone has at least one vote. Zero weights aren't allowed.
            uint16_t votes = std::max((weight.second >> bits_to_drop), uint64_t(1) );
            a.active.account_auths[weight.first] += votes;
            a.active.weight_threshold += votes;
         }

         a.active.weight_threshold /= 2;
         a.active.weight_threshold += 1;
      }
      else
      {
         vote_counter vc;
         for( const witness_object& wit : wits )
            vc.add( wit.witness_account, std::max(_vote_tally_buffer[wit.vote_id], UINT64_C(1)) );
         vc.finish( a.active );
      }
   } );

   modify(gpo, [&]( global_property_object& gp ){
      gp.active_witnesses.clear();
      gp.active_witnesses.reserve(wits.size());
      std::transform(wits.begin(), wits.end(),
                     std::inserter(gp.active_witnesses, gp.active_witnesses.end()),
                     [](const witness_object& w) {
         return w.id;
      });
   });

   const witness_schedule_object& wso = witness_schedule_id_type()(*this);
   modify(wso, [&](witness_schedule_object& _wso)
   {
      _wso.scheduler.update(gpo.active_witnesses);
   });
} FC_CAPTURE_AND_RETHROW() }

void database::update_active_committee_members()
{ try {
   assert( _committee_count_histogram_buffer.size() > 0 );
   share_type stake_target = (_total_voting_stake-_committee_count_histogram_buffer[0]) / 2;

   /// accounts that vote for 0 or 1 witness do not get to express an opinion on
   /// the number of witnesses to have (they abstain and are non-voting accounts)
   uint64_t stake_tally = 0; // _committee_count_histogram_buffer[0];
   size_t committee_member_count = 0;
   if( stake_target > 0 )
      while( (committee_member_count < _committee_count_histogram_buffer.size() - 1)
             && (stake_tally <= stake_target) )
         stake_tally += _committee_count_histogram_buffer[++committee_member_count];

   const chain_property_object& cpo = get_chain_properties();
   auto committee_members = sort_votable_objects<committee_member_index>(std::max(committee_member_count*2+1, (size_t)cpo.immutable_parameters.min_committee_member_count));

   auto update_committee_member_total_votes = [this]( const committee_member_object& cm ) {
      modify( cm, [this]( committee_member_object& obj )
      {
         obj.total_votes = _vote_tally_buffer[obj.vote_id];
      });
   };

   if( _track_standby_votes )
   {
      const auto& all_committee_members = get_index_type<committee_member_index>().indices();
      for( const committee_member_object& cm : all_committee_members )
      {
         update_committee_member_total_votes( cm );
      }
   }
   else
   {
      for( const committee_member_object& cm : committee_members )
      {
         update_committee_member_total_votes( cm );
      }
   }

   // Update committee authorities
   if( !committee_members.empty() )
   {
      modify(get(GRAPHENE_COMMITTEE_ACCOUNT), [&](account_object& a)
      {
         if( head_block_time() < HARDFORK_533_TIME )
         {
            uint64_t total_votes = 0;
            map<account_id_type, uint64_t> weights;
            a.active.weight_threshold = 0;
            a.active.clear();

            for( const committee_member_object& del : committee_members )
            {
               weights.emplace(del.committee_member_account, _vote_tally_buffer[del.vote_id]);
               total_votes += _vote_tally_buffer[del.vote_id];
            }

            // total_votes is 64 bits. Subtract the number of leading low bits from 64 to get the number of useful bits,
            // then I want to keep the most significant 16 bits of what's left.
            int8_t bits_to_drop = std::max(int(boost::multiprecision::detail::find_msb(total_votes)) - 15, 0);
            for( const auto& weight : weights )
            {
               // Ensure that everyone has at least one vote. Zero weights aren't allowed.
               uint16_t votes = std::max((weight.second >> bits_to_drop), uint64_t(1) );
               a.active.account_auths[weight.first] += votes;
               a.active.weight_threshold += votes;
            }

            a.active.weight_threshold /= 2;
            a.active.weight_threshold += 1;
         }
         else
         {
            vote_counter vc;
            for( const committee_member_object& cm : committee_members )
               vc.add( cm.committee_member_account, std::max(_vote_tally_buffer[cm.vote_id], UINT64_C(1)) );
            vc.finish( a.active );
         }
      } );
      modify(get(GRAPHENE_RELAXED_COMMITTEE_ACCOUNT), [&](account_object& a) {
         a.active = get(GRAPHENE_COMMITTEE_ACCOUNT).active;
      });
   }
   modify(get_global_properties(), [&](global_property_object& gp) {
      gp.active_committee_members.clear();
      std::transform(committee_members.begin(), committee_members.end(),
                     std::inserter(gp.active_committee_members, gp.active_committee_members.begin()),
                     [](const committee_member_object& d) { return d.id; });
   });
} FC_CAPTURE_AND_RETHROW() }

void database::update_active_sons()
{ try {
   if (head_block_time() < HARDFORK_SON_TIME) {
      return;
   }

   assert( _son_count_histogram_buffer.size() > 0 );
   share_type stake_target = (_total_voting_stake-_son_count_histogram_buffer[0]) / 2;

   /// accounts that vote for 0 or 1 son do not get to express an opinion on
   /// the number of sons to have (they abstain and are non-voting accounts)

   share_type stake_tally = 0;

   size_t son_count = 0;
   if( stake_target > 0 )
   {
      while( (son_count < _son_count_histogram_buffer.size() - 1)
             && (stake_tally <= stake_target) )
      {
         stake_tally += _son_count_histogram_buffer[++son_count];
      }
   }

   const global_property_object& gpo = get_global_properties();
   const chain_parameters& cp = gpo.parameters;
   auto sons = sort_votable_objects<son_index>(cp.maximum_son_count());

   const auto& all_sons = get_index_type<son_index>().indices();

   auto& local_vote_buffer_ref = _vote_tally_buffer;
   for( const son_object& son : all_sons )
   {
      if(son.status == son_status::request_maintenance)
      {
         auto& stats = son.statistics(*this);
         modify( stats, [&]( son_statistics_object& _s){
               _s.last_down_timestamp = head_block_time();
            });
      }
      modify( son, [local_vote_buffer_ref]( son_object& obj ){
              obj.total_votes = local_vote_buffer_ref[obj.vote_id];
              if(obj.status == son_status::request_maintenance)
                 obj.status = son_status::in_maintenance;
              });
   }

   // Update SON authority
   if( gpo.parameters.son_account() != GRAPHENE_NULL_ACCOUNT )
   {
      modify( get(gpo.parameters.son_account()), [&]( account_object& a )
      {
         if( head_block_time() < HARDFORK_533_TIME )
         {
            map<account_id_type, uint64_t> weights;
            a.active.weight_threshold = 0;
            a.active.account_auths.clear();

            for( const son_object& son : sons )
            {
               weights.emplace(son.son_account, uint64_t(1));
            }

            for( const auto& weight : weights )
            {
               // Ensure that everyone has at least one vote. Zero weights aren't allowed.
               a.active.account_auths[weight.first] += 1;
               a.active.weight_threshold += 1;
            }

            a.active.weight_threshold *= 2;
            a.active.weight_threshold /= 3;
            a.active.weight_threshold += 1;
         }
         else
         {
            vote_counter vc;
            for( const son_object& son : sons )
               vc.add( son.son_account, UINT64_C(1) );
            vc.finish_2_3( a.active );
         }
      } );
   }


   // Compare current and to-be lists of active sons
   auto cur_active_sons = gpo.active_sons;
   vector<son_info> new_active_sons;
   const auto &acc = get(gpo.parameters.son_account());
   for( const son_object& son : sons ) {
      son_info swi;
      swi.son_id = son.id;
      swi.weight = acc.active.account_auths.at(son.son_account);
      swi.signing_key = son.signing_key;
      swi.sidechain_public_keys = son.sidechain_public_keys;
      new_active_sons.push_back(swi);
   }

   bool son_sets_equal = (cur_active_sons.size() == new_active_sons.size());
   if (son_sets_equal) {
      for( size_t i = 0; i < cur_active_sons.size(); i++ ) {
         son_sets_equal = son_sets_equal && cur_active_sons.at(i) == new_active_sons.at(i);
      }
   }

   if (!son_sets_equal) {
      update_son_wallet(new_active_sons);
      update_son_statuses(cur_active_sons, new_active_sons);
   }

   // Update son performance metrics
   update_son_metrics(cur_active_sons);

   modify(gpo, [&]( global_property_object& gp ){
      gp.active_sons.clear();
      gp.active_sons.reserve(new_active_sons.size());
      gp.active_sons.insert(gp.active_sons.end(), new_active_sons.begin(), new_active_sons.end());
   });

   const son_schedule_object& sso = son_schedule_id_type()(*this);
   modify(sso, [&](son_schedule_object& _sso)
   {
      flat_set<son_id_type> active_sons;
      active_sons.reserve(gpo.active_sons.size());
      std::transform(gpo.active_sons.begin(), gpo.active_sons.end(),
                     std::inserter(active_sons, active_sons.end()),
                     [](const son_info& swi) {
         return swi.son_id;
      });
      _sso.scheduler.update(active_sons);
      // similar to witness, produce schedule for sons
      if(cur_active_sons.size() == 0 && new_active_sons.size() > 0)
      {
         witness_scheduler_rng rng(_sso.rng_seed.begin(), GRAPHENE_NEAR_SCHEDULE_CTR_IV);
         for( size_t i=0; i<new_active_sons.size(); ++i )
            _sso.scheduler.produce_schedule(rng);
      }
   });
} FC_CAPTURE_AND_RETHROW() }

void database::initialize_budget_record( fc::time_point_sec now, budget_record& rec )const
{
   const dynamic_global_property_object& dpo = get_dynamic_global_properties();
   const asset_object& core = get_core_asset();
   const asset_dynamic_data_object& core_dd = get_core_dynamic_data();

   rec.from_initial_reserve = core.reserved(*this);
   rec.from_accumulated_fees = core_dd.accumulated_fees;
   rec.from_unused_witness_budget = dpo.witness_budget;

   if(    (dpo.last_budget_time == fc::time_point_sec())
       || (now <= dpo.last_budget_time) )
   {
      rec.time_since_last_budget = 0;
      return;
   }

   int64_t dt = (now - dpo.last_budget_time).to_seconds();
   rec.time_since_last_budget = uint64_t( dt );

   // We'll consider accumulated_fees to be reserved at the BEGINNING
   // of the maintenance interval.  However, for speed we only
   // call modify() on the asset_dynamic_data_object once at the
   // end of the maintenance interval.  Thus the accumulated_fees
   // are available for the budget at this point, but not included
   // in core.reserved().
   share_type reserve = rec.from_initial_reserve + core_dd.accumulated_fees;
   // Similarly, we consider leftover witness_budget to be burned
   // at the BEGINNING of the maintenance interval.
   reserve += dpo.witness_budget;

   fc::uint128_t budget_u128 = reserve.value;
   budget_u128 *= uint64_t(dt);
   budget_u128 *= GRAPHENE_CORE_ASSET_CYCLE_RATE;
   //round up to the nearest satoshi -- this is necessary to ensure
   //   there isn't an "untouchable" reserve, and we will eventually
   //   be able to use the entire reserve
   budget_u128 += ((uint64_t(1) << GRAPHENE_CORE_ASSET_CYCLE_RATE_BITS) - 1);
   budget_u128 >>= GRAPHENE_CORE_ASSET_CYCLE_RATE_BITS;
   share_type budget;
   if( budget_u128 < reserve.value )
      rec.total_budget = share_type(budget_u128.to_uint64());
   else
      rec.total_budget = reserve;

   return;
}

/**
 * Update the budget for witnesses and workers.
 */
void database::process_budget()
{
   try
   {
      const global_property_object& gpo = get_global_properties();
      const dynamic_global_property_object& dpo = get_dynamic_global_properties();
      const asset_dynamic_data_object& core = get_core_dynamic_data();
      fc::time_point_sec now = head_block_time();

      int64_t time_to_maint = (dpo.next_maintenance_time - now).to_seconds();
      //
      // The code that generates the next maintenance time should
      //    only produce a result in the future.  If this assert
      //    fails, then the next maintenance time algorithm is buggy.
      //
      assert( time_to_maint > 0 );
      //
      // Code for setting chain parameters should validate
      //    block_interval > 0 (as well as the humans proposing /
      //    voting on changes to block interval).
      //
      assert( gpo.parameters.block_interval > 0 );
      uint64_t blocks_to_maint = (uint64_t(time_to_maint) + gpo.parameters.block_interval - 1) / gpo.parameters.block_interval;

      // blocks_to_maint > 0 because time_to_maint > 0,
      // which means numerator is at least equal to block_interval

      budget_record rec;
      initialize_budget_record( now, rec );
      share_type available_funds = rec.total_budget;

      share_type witness_budget = gpo.parameters.witness_pay_per_block.value * blocks_to_maint;
      rec.requested_witness_budget = witness_budget;
      witness_budget = std::min(witness_budget, available_funds);
      rec.witness_budget = witness_budget;
      available_funds -= witness_budget;

      // We should not factor-in the son budget before SON HARDFORK
      share_type son_budget = 0;
      if(now >= HARDFORK_SON_TIME){
         rec.leftover_son_funds = dpo.son_budget;
         available_funds += rec.leftover_son_funds;
         son_budget = gpo.parameters.son_pay_max();
         son_budget = std::min(son_budget, available_funds);
         rec.son_budget = son_budget;
         available_funds -= son_budget;
      }

      fc::uint128_t worker_budget_u128 = gpo.parameters.worker_budget_per_day.value;
      worker_budget_u128 *= uint64_t(time_to_maint);
      worker_budget_u128 /= 60*60*24;

      share_type worker_budget;
      if( worker_budget_u128 >= available_funds.value )
         worker_budget = available_funds;
      else
         worker_budget = worker_budget_u128.to_uint64();
      rec.worker_budget = worker_budget;
      available_funds -= worker_budget;

      share_type leftover_worker_funds = worker_budget;
      pay_workers(leftover_worker_funds);
      rec.leftover_worker_funds = leftover_worker_funds;
      available_funds += leftover_worker_funds;

      rec.supply_delta = rec.witness_budget
         + rec.worker_budget
         + rec.son_budget
         - rec.leftover_worker_funds
         - rec.from_accumulated_fees
         - rec.from_unused_witness_budget
         - rec.leftover_son_funds;

      modify(core, [&]( asset_dynamic_data_object& _core )
      {
         _core.current_supply = (_core.current_supply + rec.supply_delta );

         assert( rec.supply_delta ==
                                   witness_budget
                                 + worker_budget
                                 + son_budget
                                 - leftover_worker_funds
                                 - _core.accumulated_fees
                                 - dpo.witness_budget
                                 - dpo.son_budget
                                );
         _core.accumulated_fees = 0;
      });

      modify(dpo, [&]( dynamic_global_property_object& _dpo )
      {
         // Since initial witness_budget was rolled into
         // available_funds, we replace it with witness_budget
         // instead of adding it.
         _dpo.witness_budget = witness_budget;
         _dpo.son_budget = son_budget;
         _dpo.last_budget_time = now;
      });

      create< budget_record_object >( [&]( budget_record_object& _rec )
      {
         _rec.time = head_block_time();
         _rec.record = rec;
      });

      // available_funds is money we could spend, but don't want to.
      // we simply let it evaporate back into the reserve.
   }
   FC_CAPTURE_AND_RETHROW()
}

template< typename Visitor >
void visit_special_authorities( const database& db, Visitor visit )
{
   const auto& sa_idx = db.get_index_type< special_authority_index >().indices().get<by_id>();

   for( const special_authority_object& sao : sa_idx )
   {
      const account_object& acct = sao.account(db);
      if( acct.owner_special_authority.which() != special_authority::tag< no_special_authority >::value )
      {
         visit( acct, true, acct.owner_special_authority );
      }
      if( acct.active_special_authority.which() != special_authority::tag< no_special_authority >::value )
      {
         visit( acct, false, acct.active_special_authority );
      }
   }
}

void update_top_n_authorities( database& db )
{
   visit_special_authorities( db,
   [&]( const account_object& acct, bool is_owner, const special_authority& auth )
   {
      if( auth.which() == special_authority::tag< top_holders_special_authority >::value )
      {
         // use index to grab the top N holders of the asset and vote_counter to obtain the weights

         const top_holders_special_authority& tha = auth.get< top_holders_special_authority >();
         vote_counter vc;
         const auto& bal_idx = db.get_index_type< account_balance_index >().indices().get< by_asset_balance >();
         uint8_t num_needed = tha.num_top_holders;
         if( num_needed == 0 )
            return;

         // find accounts
         const auto range = bal_idx.equal_range( boost::make_tuple( tha.asset ) );
         for( const account_balance_object& bal : boost::make_iterator_range( range.first, range.second ) )
         {
             assert( bal.asset_type == tha.asset );
             if( bal.owner == acct.id )
                continue;
             vc.add( bal.owner, bal.balance.value );
             --num_needed;
             if( num_needed == 0 )
                break;
         }

         db.modify( acct, [&]( account_object& a )
         {
            vc.finish( is_owner ? a.owner : a.active );
            if( !vc.is_empty() )
               a.top_n_control_flags |= (is_owner ? account_object::top_n_control_owner : account_object::top_n_control_active);
         } );
      }
   } );
}

void split_fba_balance(
   database& db,
   uint64_t fba_id,
   uint16_t network_pct,
   uint16_t designated_asset_buyback_pct,
   uint16_t designated_asset_issuer_pct
)
{
   FC_ASSERT( uint32_t(network_pct) + uint32_t(designated_asset_buyback_pct) + uint32_t(designated_asset_issuer_pct) == GRAPHENE_100_PERCENT );
   const fba_accumulator_object& fba = fba_accumulator_id_type( fba_id )(db);
   if( fba.accumulated_fba_fees == 0 )
      return;

   const asset_dynamic_data_object& core_dd = db.get_core_dynamic_data();

   if( !fba.is_configured(db) )
   {
      ilog( "${n} core given to network at block ${b} due to non-configured FBA", ("n", fba.accumulated_fba_fees)("b", db.head_block_time()) );
      db.modify( core_dd, [&]( asset_dynamic_data_object& _core_dd )
      {
         _core_dd.current_supply -= fba.accumulated_fba_fees;
      } );
      db.modify( fba, [&]( fba_accumulator_object& _fba )
      {
         _fba.accumulated_fba_fees = 0;
      } );
      return;
   }

   fc::uint128_t buyback_amount_128 = fba.accumulated_fba_fees.value;
   buyback_amount_128 *= designated_asset_buyback_pct;
   buyback_amount_128 /= GRAPHENE_100_PERCENT;
   share_type buyback_amount = buyback_amount_128.to_uint64();

   fc::uint128_t issuer_amount_128 = fba.accumulated_fba_fees.value;
   issuer_amount_128 *= designated_asset_issuer_pct;
   issuer_amount_128 /= GRAPHENE_100_PERCENT;
   share_type issuer_amount = issuer_amount_128.to_uint64();

   // this assert should never fail
   FC_ASSERT( buyback_amount + issuer_amount <= fba.accumulated_fba_fees );

   share_type network_amount = fba.accumulated_fba_fees - (buyback_amount + issuer_amount);

   const asset_object& designated_asset = (*fba.designated_asset)(db);

   if( network_amount != 0 )
   {
      db.modify( core_dd, [&]( asset_dynamic_data_object& _core_dd )
      {
         _core_dd.current_supply -= network_amount;
      } );
   }

   fba_distribute_operation vop;
   vop.account_id = *designated_asset.buyback_account;
   vop.fba_id = fba.id;
   vop.amount = buyback_amount;
   if( vop.amount != 0 )
   {
      db.adjust_balance( *designated_asset.buyback_account, asset(buyback_amount) );
      db.push_applied_operation(vop);
   }

   vop.account_id = designated_asset.issuer;
   vop.fba_id = fba.id;
   vop.amount = issuer_amount;
   if( vop.amount != 0 )
   {
      db.adjust_balance( designated_asset.issuer, asset(issuer_amount) );
      db.push_applied_operation(vop);
   }

   db.modify( fba, [&]( fba_accumulator_object& _fba )
   {
      _fba.accumulated_fba_fees = 0;
   } );
}

void distribute_fba_balances( database& db )
{
   split_fba_balance( db, fba_accumulator_id_transfer_to_blind  , 20*GRAPHENE_1_PERCENT, 60*GRAPHENE_1_PERCENT, 20*GRAPHENE_1_PERCENT );
   split_fba_balance( db, fba_accumulator_id_blind_transfer     , 20*GRAPHENE_1_PERCENT, 60*GRAPHENE_1_PERCENT, 20*GRAPHENE_1_PERCENT );
   split_fba_balance( db, fba_accumulator_id_transfer_from_blind, 20*GRAPHENE_1_PERCENT, 60*GRAPHENE_1_PERCENT, 20*GRAPHENE_1_PERCENT );
}

void create_buyback_orders( database& db )
{
   const auto& bbo_idx = db.get_index_type< buyback_index >().indices().get<by_id>();
   const auto& bal_idx = db.get_index_type< primary_index< account_balance_index > >().get_secondary_index< balances_by_account_index >();

   for( const buyback_object& bbo : bbo_idx )
   {
      const asset_object& asset_to_buy = bbo.asset_to_buy(db);
      assert( asset_to_buy.buyback_account.valid() );

      const account_object& buyback_account = (*(asset_to_buy.buyback_account))(db);

      if( !buyback_account.allowed_assets.valid() )
      {
         wlog( "skipping buyback account ${b} at block ${n} because allowed_assets does not exist", ("b", buyback_account)("n", db.head_block_num()) );
         continue;
      }

      for( const auto& entry : bal_idx.get_account_balances( buyback_account.id ) )
      {
         const auto* it = entry.second;
         asset_id_type asset_to_sell = it->asset_type;
         share_type amount_to_sell = it->balance;
         if( asset_to_sell == asset_to_buy.id )
            continue;
         if( amount_to_sell == 0 )
            continue;
         if( buyback_account.allowed_assets->find( asset_to_sell ) == buyback_account.allowed_assets->end() )
         {
            wlog( "buyback account ${b} not selling disallowed holdings of asset ${a} at block ${n}", ("b", buyback_account)("a", asset_to_sell)("n", db.head_block_num()) );
            continue;
         }

         try
         {
            transaction_evaluation_state buyback_context(&db);
            buyback_context.skip_fee_schedule_check = true;

            limit_order_create_operation create_vop;
            create_vop.fee = asset( 0, asset_id_type() );
            create_vop.seller = buyback_account.id;
            create_vop.amount_to_sell = asset( amount_to_sell, asset_to_sell );
            create_vop.min_to_receive = asset( 1, asset_to_buy.id );
            create_vop.expiration = time_point_sec::maximum();
            create_vop.fill_or_kill = false;

            limit_order_id_type order_id = db.apply_operation( buyback_context, create_vop ).get< object_id_type >();

            if( db.find( order_id ) != nullptr )
            {
               limit_order_cancel_operation cancel_vop;
               cancel_vop.fee = asset( 0, asset_id_type() );
               cancel_vop.order = order_id;
               cancel_vop.fee_paying_account = buyback_account.id;

               db.apply_operation( buyback_context, cancel_vop );
            }
         }
         catch( const fc::exception& e )
         {
            // we can in fact get here, e.g. if asset issuer of buy/sell asset blacklists/whitelists the buyback account
            wlog( "Skipping buyback processing selling ${as} for ${ab} for buyback account ${b} at block ${n}; exception was ${e}",
                  ("as", asset_to_sell)("ab", asset_to_buy)("b", buyback_account)("n", db.head_block_num())("e", e.to_detail_string()) );
            continue;
         }
      }
   }
   return;
}

void deprecate_annual_members( database& db )
{
   const auto& account_idx = db.get_index_type<account_index>().indices().get<by_id>();
   fc::time_point_sec now = db.head_block_time();
   for( const account_object& acct : account_idx )
   {
      try
      {
         transaction_evaluation_state upgrade_context(&db);
         upgrade_context.skip_fee_schedule_check = true;

         if( acct.is_annual_member( now ) )
         {
            account_upgrade_operation upgrade_vop;
            upgrade_vop.fee = asset( 0, asset_id_type() );
            upgrade_vop.account_to_upgrade = acct.id;
            upgrade_vop.upgrade_to_lifetime_member = true;
            db.apply_operation( upgrade_context, upgrade_vop );
         }
      }
      catch( const fc::exception& e )
      {
         // we can in fact get here, e.g. if asset issuer of buy/sell asset blacklists/whitelists the buyback account
         wlog( "Skipping annual member deprecate processing for account ${a} (${an}) at block ${n}; exception was ${e}",
               ("a", acct.id)("an", acct.name)("n", db.head_block_num())("e", e.to_detail_string()) );
         continue;
      }
   }
   return;
}

uint32_t database::get_gpos_current_subperiod()
{
   if(this->head_block_time() < HARDFORK_GPOS_TIME)  //Can be deleted after GPOS hardfork time
      return 0;

   fc::time_point_sec last_date_voted;

   const auto &gpo = this->get_global_properties();
   const auto vesting_period = gpo.parameters.gpos_period();
   const auto vesting_subperiod = gpo.parameters.gpos_subperiod();
   const auto period_start = fc::time_point_sec(gpo.parameters.gpos_period_start());

   //  variables needed
   const auto number_of_subperiods = vesting_period / vesting_subperiod;
   const auto now = this->head_block_time();
   auto seconds_since_period_start = now.sec_since_epoch() - period_start.sec_since_epoch();

   // get in what sub period we are
   uint32_t current_subperiod = 0;
   std::list<uint32_t> period_list(number_of_subperiods);
   std::iota(period_list.begin(), period_list.end(), 1);

   std::for_each(period_list.begin(), period_list.end(),[&](uint32_t period) {
      if(seconds_since_period_start >= vesting_subperiod * (period - 1) &&
            seconds_since_period_start < vesting_subperiod * period)
         current_subperiod = period;
   });

   return current_subperiod;
}

double database::calculate_vesting_factor(const account_object& stake_account)
{
   fc::time_point_sec last_date_voted;
   // get last time voted form account stats
   // check last_vote_time of proxy voting account if proxy is set
   if (stake_account.options.voting_account == GRAPHENE_PROXY_TO_SELF_ACCOUNT)
      last_date_voted = stake_account.statistics(*this).last_vote_time;
   else
      last_date_voted = stake_account.options.voting_account(*this).statistics(*this).last_vote_time;

   // get global data related to gpos
   const auto &gpo = this->get_global_properties();
   const auto vesting_period = gpo.parameters.gpos_period();
   const auto vesting_subperiod = gpo.parameters.gpos_subperiod();
   const auto period_start = fc::time_point_sec(gpo.parameters.gpos_period_start());

   //  variables needed
   const auto number_of_subperiods = vesting_period / vesting_subperiod;
   double vesting_factor;

    // get in what sub period we are
   uint32_t current_subperiod = get_gpos_current_subperiod();

   if(current_subperiod == 0 || current_subperiod > number_of_subperiods) return 0;

   // On starting new vesting period, all votes become zero until someone votes, To avoid a situation of zero votes,
   // changes were done to roll in GPOS rules, the vesting factor will be 1 for whoever votes in 6th sub-period of last vesting period
   // BLOCKBACK-174 fix
   if(current_subperiod == 1 && this->head_block_time() >= HARDFORK_GPOS_TIME + vesting_period)   //Applicable only from 2nd vesting period
   {
      if(last_date_voted > period_start - vesting_subperiod)
         return 1;
   }
   if(last_date_voted < period_start) return 0;

   double numerator = number_of_subperiods;

   if(current_subperiod > 1) {
      std::list<uint32_t> subperiod_list(current_subperiod - 1);
      std::iota(subperiod_list.begin(), subperiod_list.end(), 2);
      subperiod_list.reverse();

      for(auto subperiod: subperiod_list)
      {
         numerator--;

         auto last_period_start = period_start + fc::seconds(vesting_subperiod * (subperiod - 1));
         auto last_period_end = period_start + fc::seconds(vesting_subperiod * (subperiod));

         if (last_date_voted > last_period_start && last_date_voted <= last_period_end) {
            numerator++;
            break;
         }
      }
   }
   vesting_factor = numerator / number_of_subperiods;
   return vesting_factor;
}

share_type credit_account(database& db, const account_id_type owner_id, const std::string owner_name,
                          share_type remaining_amount_to_distribute,
                          const share_type shares_to_credit, const asset_id_type payout_asset_type,
                          const pending_dividend_payout_balance_for_holder_object_index& pending_payout_balance_index,
                          const asset_id_type dividend_id) {

   //wdump((delta_balance.value)(holder_balance)(total_balance_of_dividend_asset));
   if (shares_to_credit.value) {

      remaining_amount_to_distribute -= shares_to_credit;

      dlog("Crediting account ${account} with ${amount}",
           ("account", owner_name)
                 ("amount", asset(shares_to_credit, payout_asset_type)));
      auto pending_payout_iter =
            pending_payout_balance_index.indices().get<by_dividend_payout_account>().find(
                  boost::make_tuple(dividend_id, payout_asset_type,
                                    owner_id));
      if (pending_payout_iter ==
          pending_payout_balance_index.indices().get<by_dividend_payout_account>().end())
         db.create<pending_dividend_payout_balance_for_holder_object>(
               [&](pending_dividend_payout_balance_for_holder_object &obj) {
                  obj.owner = owner_id;
                  obj.dividend_holder_asset_type = dividend_id;
                  obj.dividend_payout_asset_type = payout_asset_type;
                  obj.pending_balance = shares_to_credit;
               });
      else
         db.modify(*pending_payout_iter,
                   [&](pending_dividend_payout_balance_for_holder_object &pending_balance) {
                      pending_balance.pending_balance += shares_to_credit;
                   });
   }
   return remaining_amount_to_distribute;
}

void rolling_period_start(database& db)
{
   if(db.head_block_time() >= HARDFORK_GPOS_TIME)
   {
      auto gpo = db.get_global_properties();
      auto period_start = db.get_global_properties().parameters.gpos_period_start();
      auto vesting_period = db.get_global_properties().parameters.gpos_period();

      auto now = db.head_block_time();
      if(now.sec_since_epoch() >= (period_start + vesting_period))
      {
         // roll
         db.modify(db.get_global_properties(), [period_start, vesting_period](global_property_object& p) {
            p.parameters.extensions.value.gpos_period_start =  period_start + vesting_period;
         });
      }
   }
}

void clear_expired_custom_account_authorities(database& db)
{
   const auto& cindex = db.get_index_type<custom_account_authority_index>().indices().get<by_expiration>();
   while(!cindex.empty() && cindex.begin()->valid_to < db.head_block_time())
   {
      db.remove(*cindex.begin());
   }
}

void clear_expired_account_roles(database& db)
{
   const auto& arindex = db.get_index_type<account_role_index>().indices().get<by_expiration>();
   while(!arindex.empty() && arindex.begin()->valid_to < db.head_block_time())
   {
      db.remove(*arindex.begin());
   }
}

// Schedules payouts from a dividend distribution account to the current holders of the
// dividend-paying asset.  This takes any deposits made to the dividend distribution account
// since the last time it was called, and distributes them to the current owners of the
// dividend-paying asset according to the amount they own.
void schedule_pending_dividend_balances(database& db,
                                        const asset_object& dividend_holder_asset_obj,
                                        const asset_dividend_data_object& dividend_data,
                                        const fc::time_point_sec& current_head_block_time,
                                        const account_balance_index& balance_index,
                                        const vesting_balance_index& vesting_index,
                                        const total_distributed_dividend_balance_object_index& distributed_dividend_balance_index,
                                        const pending_dividend_payout_balance_for_holder_object_index& pending_payout_balance_index)
{ try {
   dlog("Processing dividend payments for dividend holder asset type ${holder_asset} at time ${t}",
        ("holder_asset", dividend_holder_asset_obj.symbol)("t", db.head_block_time()));
   auto balance_by_acc_index = db.get_index_type< primary_index< account_balance_index > >().get_secondary_index< balances_by_account_index >();
   auto current_distribution_account_balance_range =
      //balance_index.indices().get<by_account_asset>().equal_range(boost::make_tuple(dividend_data.dividend_distribution_account));
      balance_by_acc_index.get_account_balances(dividend_data.dividend_distribution_account);
   auto previous_distribution_account_balance_range =
      distributed_dividend_balance_index.indices().get<by_dividend_payout_asset>().equal_range(boost::make_tuple(dividend_holder_asset_obj.id));
   // the current range is now all current balances for the distribution account, sorted by asset_type
   // the previous range is now all previous balances for this account, sorted by asset type

   const auto& gpo = db.get_global_properties();

   // get the list of accounts that hold nonzero balances of the dividend asset
   auto holder_balances_begin =
      balance_index.indices().get<by_asset_balance>().lower_bound(boost::make_tuple(dividend_holder_asset_obj.id));
   auto holder_balances_end =
      balance_index.indices().get<by_asset_balance>().upper_bound(boost::make_tuple(dividend_holder_asset_obj.id, share_type()));
   uint64_t distribution_base_fee = gpo.parameters.current_fees->get<asset_dividend_distribution_operation>().distribution_base_fee;
   uint32_t distribution_fee_per_holder = gpo.parameters.current_fees->get<asset_dividend_distribution_operation>().distribution_fee_per_holder;

   std::map<account_id_type, share_type> vesting_amounts;

   auto balance_type = vesting_balance_type::normal;
   if(db.head_block_time() >= HARDFORK_GPOS_TIME)
      balance_type = vesting_balance_type::gpos;

   uint32_t holder_account_count = 0;

   // get only once a collection of accounts that hold nonzero vesting balances of the dividend asset
   auto vesting_balances_begin =
      vesting_index.indices().get<by_asset_balance>().lower_bound(boost::make_tuple(dividend_holder_asset_obj.id, balance_type));
   auto vesting_balances_end =
      vesting_index.indices().get<by_asset_balance>().upper_bound(boost::make_tuple(dividend_holder_asset_obj.id, balance_type, share_type()));

   for (const vesting_balance_object& vesting_balance_obj : boost::make_iterator_range(vesting_balances_begin, vesting_balances_end))
   {
        vesting_amounts[vesting_balance_obj.owner] += vesting_balance_obj.balance.amount;
        ++holder_account_count;
        dlog("Vesting balance for account: ${owner}, amount: ${amount}",
             ("owner", vesting_balance_obj.owner(db).name)
             ("amount", vesting_balance_obj.balance.amount));
   }

   auto current_distribution_account_balance_iter = current_distribution_account_balance_range.begin();
   if(db.head_block_time() < HARDFORK_GPOS_TIME)
      holder_account_count = std::distance(holder_balances_begin, holder_balances_end);
   // the fee, in BTS, for distributing each asset in the account
   uint64_t total_fee_per_asset_in_core = distribution_base_fee + holder_account_count * (uint64_t)distribution_fee_per_holder;

   //auto current_distribution_account_balance_iter = current_distribution_account_balance_range.first;
   auto previous_distribution_account_balance_iter = previous_distribution_account_balance_range.first;
   dlog("Current balances in distribution account: ${current}, Previous balances: ${previous}",
        ("current", (int64_t)std::distance(current_distribution_account_balance_range.begin(), current_distribution_account_balance_range.end()))
        ("previous", (int64_t)std::distance(previous_distribution_account_balance_range.first, previous_distribution_account_balance_range.second)));

   // when we pay out the dividends to the holders, we need to know the total balance of the dividend asset in all
   // accounts other than the distribution account (it would be silly to distribute dividends back to
   // the distribution account)
   share_type total_balance_of_dividend_asset;
   if(db.head_block_time() >= HARDFORK_GPOS_TIME && dividend_holder_asset_obj.symbol == GRAPHENE_SYMBOL) { // only core
      for (const vesting_balance_object &holder_balance_object : boost::make_iterator_range(vesting_balances_begin,
                                                                                            vesting_balances_end))
         if (holder_balance_object.owner != dividend_data.dividend_distribution_account) {
            total_balance_of_dividend_asset += holder_balance_object.balance.amount;
         }
   }
   else {
      for (const account_balance_object &holder_balance_object : boost::make_iterator_range(holder_balances_begin,
                                                                                            holder_balances_end))
         if (holder_balance_object.owner != dividend_data.dividend_distribution_account) {
            total_balance_of_dividend_asset += holder_balance_object.balance;
            auto itr = vesting_amounts.find(holder_balance_object.owner);
            if (itr != vesting_amounts.end())
               total_balance_of_dividend_asset += itr->second;
         }
   }
   // loop through all of the assets currently or previously held in the distribution account
   while (current_distribution_account_balance_iter != current_distribution_account_balance_range.end() ||
          previous_distribution_account_balance_iter != previous_distribution_account_balance_range.second)
   {
      try
      {
         // First, figure out how much the balance on this asset has changed since the last sharing out
         share_type current_balance;
         share_type previous_balance;
         asset_id_type payout_asset_type;

         if (previous_distribution_account_balance_iter == previous_distribution_account_balance_range.second ||
             current_distribution_account_balance_iter->second->asset_type < previous_distribution_account_balance_iter->dividend_payout_asset_type)
         {
            // there are no more previous balances or there is no previous balance for this particular asset type
            payout_asset_type = current_distribution_account_balance_iter->second->asset_type;
            current_balance = current_distribution_account_balance_iter->second->balance;
            idump((payout_asset_type)(current_balance));
         }
         else if (current_distribution_account_balance_iter == current_distribution_account_balance_range.end() ||
                  previous_distribution_account_balance_iter->dividend_payout_asset_type < current_distribution_account_balance_iter->second->asset_type)
         {
            // there are no more current balances or there is no current balance for this particular previous asset type
            payout_asset_type = previous_distribution_account_balance_iter->dividend_payout_asset_type;
            previous_balance = previous_distribution_account_balance_iter->balance_at_last_maintenance_interval;
            idump((payout_asset_type)(previous_balance));
         }
         else
         {
            // we have both a previous and a current balance for this asset type
            payout_asset_type = current_distribution_account_balance_iter->second->asset_type;
            current_balance = current_distribution_account_balance_iter->second->balance;
            previous_balance = previous_distribution_account_balance_iter->balance_at_last_maintenance_interval;
            idump((payout_asset_type)(current_balance)(previous_balance));
         }

         share_type delta_balance = current_balance - previous_balance;

         // Next, figure out if we want to share this out -- if the amount added to the distribution
         // account since last payout is too small, we won't bother.

         share_type total_fee_per_asset_in_payout_asset;
         const asset_object* payout_asset_object = nullptr;
         if (payout_asset_type == asset_id_type())
         {
            payout_asset_object = &db.get_core_asset();
            total_fee_per_asset_in_payout_asset = total_fee_per_asset_in_core;
            dlog("Fee for distributing ${payout_asset_type}: ${fee}",
                 ("payout_asset_type", asset_id_type()(db).symbol)
                 ("fee", asset(total_fee_per_asset_in_core, asset_id_type())));
         }
         else
         {
            // figure out what the total fee is in terms of the payout asset
            const asset_index& asset_object_index = db.get_index_type<asset_index>();
            auto payout_asset_object_iter = asset_object_index.indices().find(payout_asset_type);
            FC_ASSERT(payout_asset_object_iter != asset_object_index.indices().end());

            payout_asset_object = &*payout_asset_object_iter;
            asset total_fee_per_asset = asset(total_fee_per_asset_in_core, asset_id_type()) * payout_asset_object->options.core_exchange_rate;
            FC_ASSERT(total_fee_per_asset.asset_id == payout_asset_type);

            total_fee_per_asset_in_payout_asset = total_fee_per_asset.amount;
            dlog("Fee for distributing ${payout_asset_type}: ${fee}",
                 ("payout_asset_type", payout_asset_type(db).symbol)("fee", total_fee_per_asset_in_payout_asset));
         }

         share_type minimum_shares_to_distribute;
         if (dividend_data.options.minimum_fee_percentage)
         {
            fc::uint128_t minimum_amount_to_distribute = total_fee_per_asset_in_payout_asset.value;
            minimum_amount_to_distribute *= 100 * GRAPHENE_1_PERCENT;
            minimum_amount_to_distribute /= dividend_data.options.minimum_fee_percentage;
            wdump((total_fee_per_asset_in_payout_asset)(dividend_data.options));
            minimum_shares_to_distribute = minimum_amount_to_distribute.to_uint64();
         }

         dlog("Processing dividend payments of asset type ${payout_asset_type}, delta balance is ${delta_balance}", ("payout_asset_type", payout_asset_type(db).symbol)("delta_balance", delta_balance));
         if (delta_balance > 0)
         {
            if (delta_balance >= minimum_shares_to_distribute)
            {
               // first, pay the fee for scheduling these dividend  payments
               if (payout_asset_type == asset_id_type())
               {
                  // pay fee to network
                  db.modify(asset_dynamic_data_id_type()(db), [total_fee_per_asset_in_core](asset_dynamic_data_object& d) {
                     d.accumulated_fees += total_fee_per_asset_in_core;
                  });
                  db.adjust_balance(dividend_data.dividend_distribution_account,
                                    asset(-total_fee_per_asset_in_core, asset_id_type()));
                  delta_balance -= total_fee_per_asset_in_core;
               }
               else
               {
                  const asset_dynamic_data_object& dynamic_data = payout_asset_object->dynamic_data(db);
                  if (dynamic_data.fee_pool < total_fee_per_asset_in_core)
                     FC_THROW("Not distributing dividends for ${holder_asset_type} in asset ${payout_asset_type} "
                              "because insufficient funds in fee pool (need: ${need}, have: ${have})",
                              ("holder_asset_type", dividend_holder_asset_obj.symbol)
                              ("payout_asset_type", payout_asset_object->symbol)
                              ("need", asset(total_fee_per_asset_in_core, asset_id_type()))
                              ("have", asset(dynamic_data.fee_pool, payout_asset_type)));
                  // deduct the fee from the dividend distribution account
                  db.adjust_balance(dividend_data.dividend_distribution_account,
                                    asset(-total_fee_per_asset_in_payout_asset, payout_asset_type));
                  // convert it to core
                  db.modify(payout_asset_object->dynamic_data(db), [total_fee_per_asset_in_core, total_fee_per_asset_in_payout_asset](asset_dynamic_data_object& d) {
                     d.fee_pool -= total_fee_per_asset_in_core;
                     d.accumulated_fees += total_fee_per_asset_in_payout_asset;
                  });
                  // and pay it to the network
                  db.modify(asset_dynamic_data_id_type()(db), [total_fee_per_asset_in_core](asset_dynamic_data_object& d) {
                     d.accumulated_fees += total_fee_per_asset_in_core;
                  });
                  delta_balance -= total_fee_per_asset_in_payout_asset;
               }

               dlog("There are ${count} holders of the dividend-paying asset, with a total balance of ${total}",
                    ("count", holder_account_count)
                    ("total", total_balance_of_dividend_asset));
               share_type remaining_amount_to_distribute = delta_balance;

               if(db.head_block_time() >= HARDFORK_GPOS_TIME && dividend_holder_asset_obj.symbol == GRAPHENE_SYMBOL) { // core only
                  // credit each account with their portion, don't send any back to the dividend distribution account
                  for (const vesting_balance_object &holder_balance_object : boost::make_iterator_range(
                        vesting_balances_begin, vesting_balances_end)) {
                     if (holder_balance_object.owner == dividend_data.dividend_distribution_account) continue;

                     auto vesting_factor = db.calculate_vesting_factor(holder_balance_object.owner(db));

                     auto holder_balance = holder_balance_object.balance;

                     fc::uint128_t amount_to_credit(delta_balance.value);
                     amount_to_credit *= holder_balance.amount.value;
                     amount_to_credit /= total_balance_of_dividend_asset.value;
                     share_type full_shares_to_credit((int64_t) amount_to_credit.to_uint64());
                     share_type shares_to_credit = (uint64_t) floor(full_shares_to_credit.value * vesting_factor);

                     if (shares_to_credit < full_shares_to_credit) {
                        // Todo: sending results of decay to committee account, need to change to specified account
                        dlog("Crediting committee_account with ${amount}",
                             ("amount", asset(full_shares_to_credit - shares_to_credit, payout_asset_type)));
                        db.adjust_balance(dividend_data.dividend_distribution_account,
                                          -asset(full_shares_to_credit - shares_to_credit, payout_asset_type));
                        db.adjust_balance(account_id_type(0), asset(full_shares_to_credit - shares_to_credit, payout_asset_type));
                     }

                     remaining_amount_to_distribute = credit_account(db,
                                                                     holder_balance_object.owner,
                                                                     holder_balance_object.owner(db).name,
                                                                     remaining_amount_to_distribute,
                                                                     shares_to_credit,
                                                                     payout_asset_type,
                                                                     pending_payout_balance_index,
                                                                     dividend_holder_asset_obj.id);
                  }
               }
               else {
                  // credit each account with their portion, don't send any back to the dividend distribution account
                  for (const account_balance_object &holder_balance_object : boost::make_iterator_range(
                        holder_balances_begin, holder_balances_end)) {
                     if (holder_balance_object.owner == dividend_data.dividend_distribution_account) continue;

                     auto holder_balance = holder_balance_object.balance;

                     auto itr = vesting_amounts.find(holder_balance_object.owner);
                     if (itr != vesting_amounts.end())
                        holder_balance += itr->second;

                     fc::uint128_t amount_to_credit(delta_balance.value);
                     amount_to_credit *= holder_balance.value;
                     amount_to_credit /= total_balance_of_dividend_asset.value;
                     share_type shares_to_credit((int64_t) amount_to_credit.to_uint64());

                     remaining_amount_to_distribute = credit_account(db,
                                                                     holder_balance_object.owner,
                                                                     holder_balance_object.owner(db).name,
                                                                     remaining_amount_to_distribute,
                                                                     shares_to_credit,
                                                                     payout_asset_type,
                                                                     pending_payout_balance_index,
                                                                     dividend_holder_asset_obj.id);
                  }
               }
               for (const auto& pending_payout : pending_payout_balance_index.indices())
                  if (pending_payout.pending_balance.value)
                      dlog("Pending payout: ${account_name}   ->   ${amount}",
                           ("account_name", pending_payout.owner(db).name)
                           ("amount", asset(pending_payout.pending_balance, pending_payout.dividend_payout_asset_type)));
               dlog("Remaining balance not paid out: ${amount}",
                    ("amount", asset(remaining_amount_to_distribute, payout_asset_type)));

               share_type distributed_amount = delta_balance - remaining_amount_to_distribute;
               if (previous_distribution_account_balance_iter == previous_distribution_account_balance_range.second ||
                   previous_distribution_account_balance_iter->dividend_payout_asset_type != payout_asset_type)
                  db.create<total_distributed_dividend_balance_object>( [&]( total_distributed_dividend_balance_object& obj ){
                     obj.dividend_holder_asset_type = dividend_holder_asset_obj.id;
                     obj.dividend_payout_asset_type = payout_asset_type;
                     obj.balance_at_last_maintenance_interval = distributed_amount;
                  });
               else
                  db.modify(*previous_distribution_account_balance_iter, [&]( total_distributed_dividend_balance_object& obj ){
                     obj.balance_at_last_maintenance_interval += distributed_amount;
                  });
            }
            else
               FC_THROW("Not distributing dividends for ${holder_asset_type} in asset ${payout_asset_type} "
                        "because amount ${delta_balance} is too small an amount to distribute.",
                        ("holder_asset_type", dividend_holder_asset_obj.symbol)
                        ("payout_asset_type", payout_asset_object->symbol)
                        ("delta_balance", asset(delta_balance, payout_asset_type)));
         }
         else if (delta_balance < 0)
         {
            // some amount of the asset has been withdrawn from the dividend_distribution_account,
            // meaning the current pending payout balances will add up to more than our current balance.
            // This should be extremely rare (caused by an override transfer by the asset owner).
            // Reduce all pending payouts proportionally
            share_type total_pending_balances;
            auto pending_payouts_range =
               pending_payout_balance_index.indices().get<by_dividend_payout_account>().equal_range(boost::make_tuple(dividend_holder_asset_obj.id, payout_asset_type));

            for (const pending_dividend_payout_balance_for_holder_object& pending_balance_object : boost::make_iterator_range(pending_payouts_range.first, pending_payouts_range.second))
               total_pending_balances += pending_balance_object.pending_balance;

            share_type remaining_amount_to_recover = -delta_balance;
            share_type remaining_pending_balances = total_pending_balances;
            for (const pending_dividend_payout_balance_for_holder_object& pending_balance_object : boost::make_iterator_range(pending_payouts_range.first, pending_payouts_range.second))
            {
               fc::uint128_t amount_to_debit(remaining_amount_to_recover.value);
               amount_to_debit *= pending_balance_object.pending_balance.value;
               amount_to_debit /= remaining_pending_balances.value;
               share_type shares_to_debit((int64_t)amount_to_debit.to_uint64());

               remaining_amount_to_recover -= shares_to_debit;
               remaining_pending_balances -= pending_balance_object.pending_balance;

               db.modify(pending_balance_object, [&]( pending_dividend_payout_balance_for_holder_object& pending_balance ){
                  pending_balance.pending_balance -= shares_to_debit;
               });
            }

            // if we're here, we know there must be a previous balance, so just adjust it by the
            // amount we just reclaimed
            db.modify(*previous_distribution_account_balance_iter, [&]( total_distributed_dividend_balance_object& obj ){
               obj.balance_at_last_maintenance_interval += delta_balance;
               assert(obj.balance_at_last_maintenance_interval == current_balance);
            });
         } // end if deposit was large enough to distribute
      }
      catch (const fc::exception& e)
      {
         dlog("${e}", ("e", e));
      }

      // iterate
      if (previous_distribution_account_balance_iter == previous_distribution_account_balance_range.second ||
          current_distribution_account_balance_iter->second->asset_type < previous_distribution_account_balance_iter->dividend_payout_asset_type)
         ++current_distribution_account_balance_iter;
      else if (current_distribution_account_balance_iter == current_distribution_account_balance_range.end() ||
               previous_distribution_account_balance_iter->dividend_payout_asset_type < current_distribution_account_balance_iter->second->asset_type)
         ++previous_distribution_account_balance_iter;
      else
      {
         ++current_distribution_account_balance_iter;
         ++previous_distribution_account_balance_iter;
      }
   }
   db.modify(dividend_data, [current_head_block_time](asset_dividend_data_object& dividend_data_obj) {
      dividend_data_obj.last_scheduled_distribution_time = current_head_block_time;
      dividend_data_obj.last_distribution_time = current_head_block_time;
      });

} FC_CAPTURE_AND_RETHROW() }

void process_dividend_assets(database& db)
{ try {
   ilog("In process_dividend_assets time ${time}", ("time", db.head_block_time()));

   const account_balance_index& balance_index = db.get_index_type<account_balance_index>();
   //const auto& balance_index = db.get_index_type< primary_index< account_balance_index > >().get_secondary_index< balances_by_account_index >();
   const vesting_balance_index& vbalance_index = db.get_index_type<vesting_balance_index>();
   const total_distributed_dividend_balance_object_index& distributed_dividend_balance_index = db.get_index_type<total_distributed_dividend_balance_object_index>();
   const pending_dividend_payout_balance_for_holder_object_index& pending_payout_balance_index = db.get_index_type<pending_dividend_payout_balance_for_holder_object_index>();

   // TODO: switch to iterating over only dividend assets (generalize the by_type index)
   for( const asset_object& dividend_holder_asset_obj : db.get_index_type<asset_index>().indices() )
      if (dividend_holder_asset_obj.dividend_data_id)
      {
         const asset_dividend_data_object& dividend_data = dividend_holder_asset_obj.dividend_data(db);
         const account_object& dividend_distribution_account_object = dividend_data.dividend_distribution_account(db);

         fc::time_point_sec current_head_block_time = db.head_block_time();

         schedule_pending_dividend_balances(db, dividend_holder_asset_obj, dividend_data, current_head_block_time,
                                            balance_index, vbalance_index, distributed_dividend_balance_index, pending_payout_balance_index);
         if (dividend_data.options.next_payout_time &&
             db.head_block_time() >= *dividend_data.options.next_payout_time)
         {
            try
            {
               dlog("Dividend payout time has arrived for asset ${holder_asset}",
                    ("holder_asset", dividend_holder_asset_obj.symbol));
#ifndef NDEBUG
               // dump balances before the payouts for debugging
               const auto& balance_index = db.get_index_type< primary_index< account_balance_index > >();
               const auto& balances = balance_index.get_secondary_index< balances_by_account_index >().get_account_balances( dividend_data.dividend_distribution_account );
               for( const auto balance : balances )
                  ilog("  Current balance: ${asset}", ("asset", asset(balance.second->balance, balance.second->asset_type)));
#endif

               // when we do the payouts, we first increase the balances in all of the receiving accounts
               // and use this map to keep track of the total amount of each asset paid out.
               // Afterwards, we decrease the distribution account's balance by the total amount paid out,
               // and modify the distributed_balances accordingly
               std::map<asset_id_type, share_type> amounts_paid_out_by_asset;

               auto pending_payouts_range =
                  pending_payout_balance_index.indices().get<by_dividend_account_payout>().equal_range(boost::make_tuple(dividend_holder_asset_obj.id));
               // the pending_payouts_range is all payouts for this dividend asset, sorted by the holder's account
               // we iterate in this order so we can build up a list of payouts for each account to put in the
               // virtual op
               vector<asset> payouts_for_this_holder;
               fc::optional<account_id_type> last_holder_account_id;

               // cache the assets the distribution account is approved to send, we will be asking
               // for these often
               flat_map<asset_id_type, bool> approved_assets; // assets that the dividend distribution account is authorized to send/receive
               auto is_asset_approved_for_distribution_account = [&](const asset_id_type& asset_id) {
                  auto approved_assets_iter = approved_assets.find(asset_id);
                  if (approved_assets_iter != approved_assets.end())
                     return approved_assets_iter->second;
                  bool is_approved = is_authorized_asset(db, dividend_distribution_account_object,
                                                         asset_id(db));
                  approved_assets[asset_id] = is_approved;
                  return is_approved;
               };

               for (auto pending_balance_object_iter = pending_payouts_range.first; pending_balance_object_iter != pending_payouts_range.second; )
               {
                  const pending_dividend_payout_balance_for_holder_object& pending_balance_object = *pending_balance_object_iter;

                  if (last_holder_account_id && *last_holder_account_id != pending_balance_object.owner && payouts_for_this_holder.size())
                  {
                     // we've moved on to a new account, generate the dividend payment virtual op for the previous one
                     db.push_applied_operation(asset_dividend_distribution_operation(dividend_holder_asset_obj.id,
                                                                                     *last_holder_account_id,
                                                                                     payouts_for_this_holder));
                     dlog("Just pushed virtual op for payout to ${account}", ("account", (*last_holder_account_id)(db).name));
                     payouts_for_this_holder.clear();
                     last_holder_account_id.reset();
                  }


                  if (pending_balance_object.pending_balance.value &&
                      is_authorized_asset(db, pending_balance_object.owner(db), pending_balance_object.dividend_payout_asset_type(db)) &&
                      is_asset_approved_for_distribution_account(pending_balance_object.dividend_payout_asset_type))
                  {
                     dlog("Processing payout of ${asset} to account ${account}",
                          ("asset", asset(pending_balance_object.pending_balance, pending_balance_object.dividend_payout_asset_type))
                          ("account", pending_balance_object.owner(db).name));

                     db.adjust_balance(pending_balance_object.owner,
                                       asset(pending_balance_object.pending_balance,
                                             pending_balance_object.dividend_payout_asset_type));
                     payouts_for_this_holder.push_back(asset(pending_balance_object.pending_balance,
                                                             pending_balance_object.dividend_payout_asset_type));
                     last_holder_account_id = pending_balance_object.owner;
                     amounts_paid_out_by_asset[pending_balance_object.dividend_payout_asset_type] += pending_balance_object.pending_balance;

                     db.modify(pending_balance_object, [&]( pending_dividend_payout_balance_for_holder_object& pending_balance ){
                        pending_balance.pending_balance = 0;
                     });
                  }

                  ++pending_balance_object_iter;
               }
               // we will always be left with the last holder's data, generate the virtual op for it now.
               if (last_holder_account_id && payouts_for_this_holder.size())
               {
                  // we've moved on to a new account, generate the dividend payment virtual op for the previous one
                  db.push_applied_operation(asset_dividend_distribution_operation(dividend_holder_asset_obj.id,
                                                                                  *last_holder_account_id,
                                                                                  payouts_for_this_holder));
                  dlog("Just pushed virtual op for payout to ${account}", ("account", (*last_holder_account_id)(db).name));
               }

               // now debit the total amount of dividends paid out from the distribution account
               // and reduce the distributed_balances accordingly

               for (const auto& value : amounts_paid_out_by_asset)
               {
                  const asset_id_type& asset_paid_out = value.first;
                  const share_type& amount_paid_out = value.second;

                  db.adjust_balance(dividend_data.dividend_distribution_account,
                                    asset(-amount_paid_out,
                                          asset_paid_out));
                  auto distributed_balance_iter =
                     distributed_dividend_balance_index.indices().get<by_dividend_payout_asset>().find(boost::make_tuple(dividend_holder_asset_obj.id,
                                                                                                                         asset_paid_out));
                  assert(distributed_balance_iter != distributed_dividend_balance_index.indices().get<by_dividend_payout_asset>().end());
                  if (distributed_balance_iter != distributed_dividend_balance_index.indices().get<by_dividend_payout_asset>().end())
                     db.modify(*distributed_balance_iter, [&]( total_distributed_dividend_balance_object& obj ){
                        obj.balance_at_last_maintenance_interval -= amount_paid_out; // now they've been paid out, reset to zero
                     });

               }

               // now schedule the next payout time
               db.modify(dividend_data, [current_head_block_time](asset_dividend_data_object& dividend_data_obj) {
                  dividend_data_obj.last_scheduled_payout_time = dividend_data_obj.options.next_payout_time;
                  dividend_data_obj.last_payout_time = current_head_block_time;
                  fc::optional<fc::time_point_sec> next_payout_time;
                  if (dividend_data_obj.options.payout_interval)
                  {
                     // if there was a previous payout, make our next payment one interval
                     uint32_t current_time_sec = current_head_block_time.sec_since_epoch();
                     uint32_t next_possible_time_sec = dividend_data_obj.last_scheduled_payout_time->sec_since_epoch();
                     do
                        next_possible_time_sec += *dividend_data_obj.options.payout_interval;
                     while (next_possible_time_sec <= current_time_sec);

                     next_payout_time = next_possible_time_sec;
                  }
                  dividend_data_obj.options.next_payout_time = next_payout_time;
                  idump((dividend_data_obj.last_scheduled_payout_time)
                        (dividend_data_obj.last_payout_time)
                        (dividend_data_obj.options.next_payout_time));
               });
            }
            FC_RETHROW_EXCEPTIONS(error, "Error while paying out dividends for holder asset ${holder_asset}", ("holder_asset", dividend_holder_asset_obj.symbol))
         }
      }
} FC_CAPTURE_AND_RETHROW() }

void database::perform_son_tasks()
{
   const global_property_object& gpo = get_global_properties();
   if(gpo.parameters.son_account() == GRAPHENE_NULL_ACCOUNT && head_block_time() >= HARDFORK_SON_TIME)
   {
      const auto& son_account = create<account_object>([&](account_object& a) {
         a.name = "son-account";
         a.statistics = create<account_statistics_object>([&a](account_statistics_object& s){
            s.owner = a.id;
            s.name = a.name;
         }).id;
         a.owner.weight_threshold = 1;
         a.active.weight_threshold = 0;
         a.registrar = a.lifetime_referrer = a.referrer = a.id;
         a.membership_expiration_date = time_point_sec::maximum();
         a.network_fee_percentage = GRAPHENE_DEFAULT_NETWORK_PERCENT_OF_FEE;
         a.lifetime_referrer_fee_percentage = GRAPHENE_100_PERCENT - GRAPHENE_DEFAULT_NETWORK_PERCENT_OF_FEE;
      });

      modify( gpo, [&son_account]( global_property_object& gpo ) {
            gpo.parameters.extensions.value.son_account = son_account.get_id();
            if( gpo.pending_parameters )
               gpo.pending_parameters->extensions.value.son_account = son_account.get_id();
      });
   }
   // create BTC asset here because son_account is the issuer of the BTC
   if (gpo.parameters.btc_asset() == asset_id_type()  && head_block_time() >= HARDFORK_SON_TIME)
   {
      const asset_dynamic_data_object& dyn_asset =
         create<asset_dynamic_data_object>([](asset_dynamic_data_object& a) {
            a.current_supply = 0;
         });

      const asset_object& btc_asset =
         create<asset_object>( [&gpo, &dyn_asset]( asset_object& a ) {
            a.symbol = "BTC";
            a.precision = 8;
            a.issuer = gpo.parameters.son_account();
            a.options.max_supply = GRAPHENE_MAX_SHARE_SUPPLY;
            a.options.market_fee_percent = 500; // 5%
            a.options.issuer_permissions = UIA_ASSET_ISSUER_PERMISSION_MASK;
            a.options.flags = asset_issuer_permission_flags::charge_market_fee |
                              asset_issuer_permission_flags::override_authority;
            a.options.core_exchange_rate.base.amount = 100000;
            a.options.core_exchange_rate.base.asset_id = asset_id_type(0);
            a.options.core_exchange_rate.quote.amount = 2500; // CoinMarketCap approx value
            a.options.core_exchange_rate.quote.asset_id = a.id;
            a.options.whitelist_authorities.clear(); // accounts allowed to use asset, if not empty
            a.options.blacklist_authorities.clear(); // accounts who can blacklist other accounts to use asset, if white_list flag is set
            a.options.whitelist_markets.clear(); // might be traded with
            a.options.blacklist_markets.clear(); // might not be traded with
            a.dynamic_asset_data_id = dyn_asset.id;
         });
      modify( gpo, [&btc_asset]( global_property_object& gpo ) {
            gpo.parameters.extensions.value.btc_asset = btc_asset.get_id();
            if( gpo.pending_parameters )
               gpo.pending_parameters->extensions.value.btc_asset = btc_asset.get_id();
      });
   }
   // create HBD asset here because son_account is the issuer of the HBD
   if (gpo.parameters.hbd_asset() == asset_id_type()  && head_block_time() >= HARDFORK_SON_FOR_HIVE_TIME)
   {
      const asset_dynamic_data_object& dyn_asset =
         create<asset_dynamic_data_object>([](asset_dynamic_data_object& a) {
            a.current_supply = 0;
         });

      const asset_object& hbd_asset =
         create<asset_object>( [&gpo, &dyn_asset]( asset_object& a ) {
            a.symbol = "HBD";
            a.precision = 3;
            a.issuer = gpo.parameters.son_account();
            a.options.max_supply = GRAPHENE_MAX_SHARE_SUPPLY;
            a.options.market_fee_percent = 500; // 5%
            a.options.issuer_permissions = UIA_ASSET_ISSUER_PERMISSION_MASK;
            a.options.flags = asset_issuer_permission_flags::charge_market_fee |
                              asset_issuer_permission_flags::override_authority;
            a.options.core_exchange_rate.base.amount = 100000;
            a.options.core_exchange_rate.base.asset_id = asset_id_type(0);
            a.options.core_exchange_rate.quote.amount = 2500; // CoinMarketCap approx value
            a.options.core_exchange_rate.quote.asset_id = a.id;
            a.options.whitelist_authorities.clear(); // accounts allowed to use asset, if not empty
            a.options.blacklist_authorities.clear(); // accounts who can blacklist other accounts to use asset, if white_list flag is set
            a.options.whitelist_markets.clear(); // might be traded with
            a.options.blacklist_markets.clear(); // might not be traded with
            a.dynamic_asset_data_id = dyn_asset.id;
         });
      modify( gpo, [&hbd_asset]( global_property_object& gpo ) {
            gpo.parameters.extensions.value.hbd_asset = hbd_asset.get_id();
            if( gpo.pending_parameters )
               gpo.pending_parameters->extensions.value.hbd_asset = hbd_asset.get_id();
      });
   }
   // create HIVE asset here because son_account is the issuer of the HIVE
   if (gpo.parameters.hive_asset() == asset_id_type()  && head_block_time() >= HARDFORK_SON_FOR_HIVE_TIME)
   {
      const asset_dynamic_data_object& dyn_asset =
         create<asset_dynamic_data_object>([](asset_dynamic_data_object& a) {
            a.current_supply = 0;
         });

      const asset_object& hive_asset =
         create<asset_object>( [&gpo, &dyn_asset]( asset_object& a ) {
            a.symbol = "HIVE";
            a.precision = 3;
            a.issuer = gpo.parameters.son_account();
            a.options.max_supply = GRAPHENE_MAX_SHARE_SUPPLY;
            a.options.market_fee_percent = 500; // 5%
            a.options.issuer_permissions = UIA_ASSET_ISSUER_PERMISSION_MASK;
            a.options.flags = asset_issuer_permission_flags::charge_market_fee |
                              asset_issuer_permission_flags::override_authority;
            a.options.core_exchange_rate.base.amount = 100000;
            a.options.core_exchange_rate.base.asset_id = asset_id_type(0);
            a.options.core_exchange_rate.quote.amount = 2500; // CoinMarketCap approx value
            a.options.core_exchange_rate.quote.asset_id = a.id;
            a.options.whitelist_authorities.clear(); // accounts allowed to use asset, if not empty
            a.options.blacklist_authorities.clear(); // accounts who can blacklist other accounts to use asset, if white_list flag is set
            a.options.whitelist_markets.clear(); // might be traded with
            a.options.blacklist_markets.clear(); // might not be traded with
            a.dynamic_asset_data_id = dyn_asset.id;
         });
      modify( gpo, [&hive_asset]( global_property_object& gpo ) {
            gpo.parameters.extensions.value.hive_asset = hive_asset.get_id();
            if( gpo.pending_parameters )
               gpo.pending_parameters->extensions.value.hive_asset = hive_asset.get_id();
      });
   }
   // Pay the SONs
   if (head_block_time() >= HARDFORK_SON_TIME)
   {
      // Before making a budget we should pay out SONs
      // This function should check if its time to pay sons
      // and modify the global son funds accordingly, whatever is left is passed on to next budget
      pay_sons();
   }
}

void update_son_params(database& db)
{
   if( (db.head_block_time() >= HARDFORK_SON2_TIME) && (db.head_block_time() < HARDFORK_SON3_TIME) )
   {
      const auto& gpo = db.get_global_properties();
      db.modify( gpo, []( global_property_object& gpo ) {
         gpo.parameters.extensions.value.maximum_son_count = 7;
      });
   }
}

void database::perform_chain_maintenance(const signed_block& next_block, const global_property_object& global_props)
{ try {
   const auto& gpo = get_global_properties();

   distribute_fba_balances(*this);
   create_buyback_orders(*this);

   process_dividend_assets(*this);

   rolling_period_start(*this);

   update_son_params(*this);

   struct vote_tally_helper {
      database& d;
      const global_property_object& props;
      std::map<account_id_type, share_type> vesting_amounts;

      vote_tally_helper(database& d, const global_property_object& gpo)
         : d(d), props(gpo)
      {
         d._vote_tally_buffer.resize(props.next_available_vote_id);
         d._witness_count_histogram_buffer.resize(props.parameters.maximum_witness_count / 2 + 1);
         d._committee_count_histogram_buffer.resize(props.parameters.maximum_committee_count / 2 + 1);
         d._son_count_histogram_buffer.resize(props.parameters.maximum_son_count() / 2 + 1);
         d._total_voting_stake = 0;

         auto balance_type = vesting_balance_type::normal;
         if(d.head_block_time() >= HARDFORK_GPOS_TIME)
            balance_type = vesting_balance_type::gpos;

         const vesting_balance_index& vesting_index = d.get_index_type<vesting_balance_index>();

         auto vesting_balances_begin =
              vesting_index.indices().get<by_asset_balance>().lower_bound(boost::make_tuple(asset_id_type(), balance_type));
         auto vesting_balances_end =
              vesting_index.indices().get<by_asset_balance>().upper_bound(boost::make_tuple(asset_id_type(), balance_type, share_type()));
         for (const vesting_balance_object& vesting_balance_obj : boost::make_iterator_range(vesting_balances_begin, vesting_balances_end))
         {
            vesting_amounts[vesting_balance_obj.owner] += vesting_balance_obj.balance.amount;
            dlog("Vesting balance for account: ${owner}, amount: ${amount}",
                 ("owner", vesting_balance_obj.owner(d).name)
                 ("amount", vesting_balance_obj.balance.amount));
         }

      }

      void operator()( const account_object& stake_account, const account_statistics_object& stats )
      {
         if( props.parameters.count_non_member_votes || stake_account.is_member(d.head_block_time()) )
         {
            // There may be a difference between the account whose stake is voting and the one specifying opinions.
            // Usually they're the same, but if the stake account has specified a voting_account, that account is the one
            // specifying the opinions.
            const account_object* opinion_account_ptr =
                  (stake_account.options.voting_account ==
                   GRAPHENE_PROXY_TO_SELF_ACCOUNT)? &stake_account
                                     : d.find(stake_account.options.voting_account);

            if( !opinion_account_ptr ) // skip non-exist account
               return;

            const account_object& opinion_account = *opinion_account_ptr;

            const auto& stats = stake_account.statistics(d);
            uint64_t voting_stake = 0;

            auto itr = vesting_amounts.find(stake_account.id);
            if (itr != vesting_amounts.end())
                voting_stake += itr->second.value;

            if(d.head_block_time() >= HARDFORK_GPOS_TIME)
            {
               if (itr == vesting_amounts.end() && d.head_block_time() >= (HARDFORK_GPOS_TIME + props.parameters.gpos_subperiod()/2))
                  return;

               auto vesting_factor = d.calculate_vesting_factor(stake_account);
               voting_stake = (uint64_t)floor(voting_stake * vesting_factor);

               //Include votes(based on stake) for the period of gpos_subperiod()/2 as system has zero votes on GPOS activation
               if(d.head_block_time() < (HARDFORK_GPOS_TIME + props.parameters.gpos_subperiod()/2))
               {
                  voting_stake += stats.total_core_in_orders.value
                                 + (stake_account.cashback_vb.valid() ? (*stake_account.cashback_vb)(d).balance.amount.value : 0)
                                 + d.get_balance(stake_account.get_id(), asset_id_type()).amount.value;
               }
            }
            else
            {
               voting_stake += stats.total_core_in_orders.value
                               + (stake_account.cashback_vb.valid() ? (*stake_account.cashback_vb)(d).balance.amount.value : 0)
                               + d.get_balance(stake_account.get_id(), asset_id_type()).amount.value;
            }

            for( vote_id_type id : opinion_account.options.votes )
            {
               uint32_t offset = id.instance();
               // if they somehow managed to specify an illegal offset, ignore it.
               if( offset < d._vote_tally_buffer.size() )
                  d._vote_tally_buffer[offset] += voting_stake;
            }

            if( opinion_account.options.num_witness <= props.parameters.maximum_witness_count )
            {
               uint16_t offset = std::min(size_t(opinion_account.options.num_witness/2),
                                          d._witness_count_histogram_buffer.size() - 1);
               // votes for a number greater than maximum_witness_count
               // are turned into votes for maximum_witness_count.
               //
               // in particular, this takes care of the case where a
               // member was voting for a high number, then the
               // parameter was lowered.
               d._witness_count_histogram_buffer[offset] += voting_stake;
            }
            if( opinion_account.options.num_committee <= props.parameters.maximum_committee_count )
            {
               uint16_t offset = std::min(size_t(opinion_account.options.num_committee/2),
                                          d._committee_count_histogram_buffer.size() - 1);
               // votes for a number greater than maximum_committee_count
               // are turned into votes for maximum_committee_count.
               //
               // same rationale as for witnesses
               d._committee_count_histogram_buffer[offset] += voting_stake;
            }
            if( opinion_account.options.num_son <= props.parameters.maximum_son_count() )
            {
               uint16_t offset = std::min(size_t(opinion_account.options.num_son/2),
                                          d._son_count_histogram_buffer.size() - 1);
               // votes for a number greater than maximum_son_count
               // are turned into votes for maximum_son_count.
               //
               // in particular, this takes care of the case where a
               // member was voting for a high number, then the
               // parameter was lowered.
               d._son_count_histogram_buffer[offset] += voting_stake;
            }

            d._total_voting_stake += voting_stake;
         }
      }
   } tally_helper(*this, gpo);

   perform_account_maintenance( tally_helper );
   struct clear_canary {
      clear_canary(vector<uint64_t>& target): target(target){}
      ~clear_canary() { target.clear(); }
   private:
      vector<uint64_t>& target;
   };
   clear_canary a(_witness_count_histogram_buffer),
                b(_committee_count_histogram_buffer),
                d(_son_count_histogram_buffer),
                c(_vote_tally_buffer);

   perform_son_tasks();
   update_top_n_authorities(*this);
   update_active_witnesses();
   update_active_committee_members();
   update_active_sons();
   update_worker_votes();

   const dynamic_global_property_object& dgpo = get_dynamic_global_properties();

   modify(gpo, [&dgpo](global_property_object& p) {
      // Remove scaling of account registration fee
      p.parameters.current_fees->get<account_create_operation>().basic_fee >>= p.parameters.account_fee_scale_bitshifts *
            (dgpo.accounts_registered_this_interval / p.parameters.accounts_per_fee_scale);

      if( p.pending_parameters )
      {
         if( !p.pending_parameters->extensions.value.min_bet_multiplier.valid() )
            p.pending_parameters->extensions.value.min_bet_multiplier = p.parameters.extensions.value.min_bet_multiplier;
         if( !p.pending_parameters->extensions.value.max_bet_multiplier.valid() )
            p.pending_parameters->extensions.value.max_bet_multiplier = p.parameters.extensions.value.max_bet_multiplier;
         if( !p.pending_parameters->extensions.value.betting_rake_fee_percentage.valid() )
            p.pending_parameters->extensions.value.betting_rake_fee_percentage = p.parameters.extensions.value.betting_rake_fee_percentage;
         if( !p.pending_parameters->extensions.value.permitted_betting_odds_increments.valid() )
            p.pending_parameters->extensions.value.permitted_betting_odds_increments = p.parameters.extensions.value.permitted_betting_odds_increments;
         if( !p.pending_parameters->extensions.value.live_betting_delay_time.valid() )
            p.pending_parameters->extensions.value.live_betting_delay_time = p.parameters.extensions.value.live_betting_delay_time;
         if( !p.pending_parameters->extensions.value.gpos_period_start.valid() )
            p.pending_parameters->extensions.value.gpos_period_start = p.parameters.extensions.value.gpos_period_start;
         if( !p.pending_parameters->extensions.value.gpos_period.valid() )
            p.pending_parameters->extensions.value.gpos_period = p.parameters.extensions.value.gpos_period;
         if( !p.pending_parameters->extensions.value.gpos_subperiod.valid() )
            p.pending_parameters->extensions.value.gpos_subperiod = p.parameters.extensions.value.gpos_subperiod;
         if( !p.pending_parameters->extensions.value.gpos_vesting_lockin_period.valid() )
            p.pending_parameters->extensions.value.gpos_vesting_lockin_period = p.parameters.extensions.value.gpos_vesting_lockin_period;
         if( !p.pending_parameters->extensions.value.rbac_max_permissions_per_account.valid() )
            p.pending_parameters->extensions.value.rbac_max_permissions_per_account = p.parameters.extensions.value.rbac_max_permissions_per_account;
         if( !p.pending_parameters->extensions.value.rbac_max_account_authority_lifetime.valid() )
            p.pending_parameters->extensions.value.rbac_max_account_authority_lifetime = p.parameters.extensions.value.rbac_max_account_authority_lifetime;
         if( !p.pending_parameters->extensions.value.rbac_max_authorities_per_permission.valid() )
            p.pending_parameters->extensions.value.rbac_max_authorities_per_permission = p.parameters.extensions.value.rbac_max_authorities_per_permission;
         if( !p.pending_parameters->extensions.value.account_roles_max_per_account.valid() )
            p.pending_parameters->extensions.value.account_roles_max_per_account = p.parameters.extensions.value.account_roles_max_per_account;
         if( !p.pending_parameters->extensions.value.account_roles_max_lifetime.valid() )
            p.pending_parameters->extensions.value.account_roles_max_lifetime = p.parameters.extensions.value.account_roles_max_lifetime;
         if( !p.pending_parameters->extensions.value.son_vesting_amount.valid() )
            p.pending_parameters->extensions.value.son_vesting_amount = p.parameters.extensions.value.son_vesting_amount;
         if( !p.pending_parameters->extensions.value.son_vesting_period.valid() )
            p.pending_parameters->extensions.value.son_vesting_period = p.parameters.extensions.value.son_vesting_period;
         if( !p.pending_parameters->extensions.value.son_pay_max.valid() )
            p.pending_parameters->extensions.value.son_pay_max = p.parameters.extensions.value.son_pay_max;
         if( !p.pending_parameters->extensions.value.son_pay_time.valid() )
            p.pending_parameters->extensions.value.son_pay_time = p.parameters.extensions.value.son_pay_time;
         if( !p.pending_parameters->extensions.value.son_deregister_time.valid() )
            p.pending_parameters->extensions.value.son_deregister_time = p.parameters.extensions.value.son_deregister_time;
         if( !p.pending_parameters->extensions.value.son_heartbeat_frequency.valid() )
            p.pending_parameters->extensions.value.son_heartbeat_frequency = p.parameters.extensions.value.son_heartbeat_frequency;
         if( !p.pending_parameters->extensions.value.son_down_time.valid() )
            p.pending_parameters->extensions.value.son_down_time = p.parameters.extensions.value.son_down_time;
         if( !p.pending_parameters->extensions.value.son_bitcoin_min_tx_confirmations.valid() )
            p.pending_parameters->extensions.value.son_bitcoin_min_tx_confirmations = p.parameters.extensions.value.son_bitcoin_min_tx_confirmations;
	 if( !p.pending_parameters->extensions.value.son_account.valid() )
            p.pending_parameters->extensions.value.son_account = p.parameters.extensions.value.son_account;
         if( !p.pending_parameters->extensions.value.btc_asset.valid() )
            p.pending_parameters->extensions.value.btc_asset = p.parameters.extensions.value.btc_asset;
	 if( !p.pending_parameters->extensions.value.maximum_son_count.valid() )
            p.pending_parameters->extensions.value.maximum_son_count = p.parameters.extensions.value.maximum_son_count;
         if( !p.pending_parameters->extensions.value.hbd_asset.valid() )
            p.pending_parameters->extensions.value.hbd_asset = p.parameters.extensions.value.hbd_asset;
         if( !p.pending_parameters->extensions.value.hive_asset.valid() )
            p.pending_parameters->extensions.value.hive_asset = p.parameters.extensions.value.hive_asset;

         // the following parameters are not allowed to be changed. So take what is in global property
         p.pending_parameters->extensions.value.hive_asset = p.parameters.extensions.value.hive_asset;
         p.pending_parameters->extensions.value.hbd_asset = p.parameters.extensions.value.hbd_asset;
         p.pending_parameters->extensions.value.btc_asset = p.parameters.extensions.value.btc_asset;
         p.pending_parameters->extensions.value.son_account = p.parameters.extensions.value.son_account;
         p.pending_parameters->extensions.value.gpos_period_start = p.parameters.extensions.value.gpos_period_start;

         p.parameters = std::move(*p.pending_parameters);
         p.pending_parameters.reset();
      }
   });

   auto next_maintenance_time = dgpo.next_maintenance_time;
   auto maintenance_interval = gpo.parameters.maintenance_interval;

   if( next_maintenance_time <= next_block.timestamp )
   {
      if( next_block.block_num() == 1 )
         next_maintenance_time = time_point_sec() +
               (((next_block.timestamp.sec_since_epoch() / maintenance_interval) + 1) * maintenance_interval);
      else
      {
         // We want to find the smallest k such that next_maintenance_time + k * maintenance_interval > head_block_time()
         //  This implies k > ( head_block_time() - next_maintenance_time ) / maintenance_interval
         //
         // Let y be the right-hand side of this inequality, i.e.
         // y = ( head_block_time() - next_maintenance_time ) / maintenance_interval
         //
         // and let the fractional part f be y-floor(y).  Clearly 0 <= f < 1.
         // We can rewrite f = y-floor(y) as floor(y) = y-f.
         //
         // Clearly k = floor(y)+1 has k > y as desired.  Now we must
         // show that this is the least such k, i.e. k-1 <= y.
         //
         // But k-1 = floor(y)+1-1 = floor(y) = y-f <= y.
         // So this k suffices.
         //
         auto y = (head_block_time() - next_maintenance_time).to_seconds() / maintenance_interval;
         next_maintenance_time += (y+1) * maintenance_interval;
      }
   }

   if( (dgpo.next_maintenance_time < HARDFORK_613_TIME) && (next_maintenance_time >= HARDFORK_613_TIME) )
      deprecate_annual_members(*this);

   modify(dgpo, [next_maintenance_time](dynamic_global_property_object& d) {
      d.next_maintenance_time = next_maintenance_time;
      d.accounts_registered_this_interval = 0;
   });

   // Reset all BitAsset force settlement volumes to zero
   //for( const asset_bitasset_data_object* d : get_index_type<asset_bitasset_data_index>() )
   for( const auto& d : get_index_type<asset_bitasset_data_index>().indices() )
      modify( d, [](asset_bitasset_data_object& o) { o.force_settled_volume = 0; });
   // Ideally we have to do this after every block but that leads to longer block applicaiton/replay times.
   // So keep it here as it is not critical. valid_to check ensures
   // these custom account auths and account roles are not usable.
   clear_expired_custom_account_authorities(*this);
   clear_expired_account_roles(*this);
   // process_budget needs to run at the bottom because
   //   it needs to know the next_maintenance_time
   process_budget();
} FC_CAPTURE_AND_RETHROW() }

} }
