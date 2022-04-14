/*
 * Copyright (c) 2018 Peerplays Blockchain Standards Association, and contributors.
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
#include <graphene/bookie/bookie_plugin.hpp>
#include <graphene/bookie/bookie_objects.hpp>

#include <graphene/chain/impacted.hpp>

#include <graphene/chain/account_evaluator.hpp>
#include <graphene/chain/account_object.hpp>
#include <graphene/chain/config.hpp>
#include <graphene/chain/database.hpp>
#include <graphene/chain/evaluator.hpp>
#include <graphene/chain/operation_history_object.hpp>
#include <graphene/chain/transaction_evaluation_state.hpp>

#include <boost/algorithm/string/case_conv.hpp>

#include <fc/thread/thread.hpp>

#include <boost/polymorphic_cast.hpp>

#if 0
# ifdef DEFAULT_LOGGER
#  undef DEFAULT_LOGGER
# endif
# define DEFAULT_LOGGER "bookie_plugin"
#endif

namespace graphene { namespace bookie {

namespace detail
{
/* As a plugin, we get notified of new/changed objects at the end of every block processed.
 * For most objects, that's fine, because we expect them to always be around until the end of
 * the block.  However, with bet objects, it's possible that the user places a bet and it fills
 * and is removed during the same block, so need another strategy to detect them immediately after
 * they are created. 
 * We do this by creating a secondary index on bet_object.  We don't actually use it
 * to index any property of the bet, we just use it to register for callbacks.
 */

void persistent_bet_index::object_inserted(const object& obj)
{
   const bet_object& bet_obj = *boost::polymorphic_downcast<const bet_object*>(&obj);
   if(0 == internal.count(bet_obj.id))
      internal.insert( {bet_obj.id, bet_obj} );
   else
      internal[bet_obj.id] = bet_obj;
}
void persistent_bet_index::object_modified(const object& after)
{
   const bet_object& bet_obj = *boost::polymorphic_downcast<const bet_object*>(&after);
   auto iter = internal.find(bet_obj.id);
   assert (iter != internal.end());
   if (iter != internal.end())
      iter->second = bet_obj;
}

//////////// end bet_object ///////////////////

void persistent_betting_market_index::object_inserted(const object& obj)
{
   const betting_market_object& betting_market_obj = *boost::polymorphic_downcast<const betting_market_object*>(&obj);
   if(0 == ephemeral_betting_market_object.count(betting_market_obj.id))
      ephemeral_betting_market_object.insert( {betting_market_obj.id, betting_market_obj} );
   else
      ephemeral_betting_market_object[betting_market_obj.id] = betting_market_obj;

}
void persistent_betting_market_index::object_modified(const object& after)
{
   const betting_market_object& betting_market_obj = *boost::polymorphic_downcast<const betting_market_object*>(&after);
   auto iter = ephemeral_betting_market_object.find(betting_market_obj.id);
   assert (iter != ephemeral_betting_market_object.end());
   if (iter != ephemeral_betting_market_object.end())
      iter->second = betting_market_obj;
}

//////////// end betting_market_object ///////////////////

void persistent_betting_market_group_index::object_inserted(const object& obj)
{
   const betting_market_group_object& betting_market_group_obj = *boost::polymorphic_downcast<const betting_market_group_object*>(&obj);
   if(0 == internal.count(betting_market_group_obj.id))
      internal.insert( {betting_market_group_obj.id, betting_market_group_obj} );
   else
      internal[betting_market_group_obj.id] = betting_market_group_obj;
}
void persistent_betting_market_group_index::object_modified(const object& after)
{
   const betting_market_group_object& betting_market_group_obj = *boost::polymorphic_downcast<const betting_market_group_object*>(&after);
   auto iter = internal.find(betting_market_group_obj.id);
   assert (iter != internal.end());
   if (iter != internal.end())
      iter->second = betting_market_group_obj;
}

//////////// end betting_market_group_object ///////////////////

void persistent_event_index::object_inserted(const object& obj)
{
   const event_object& event_obj = *boost::polymorphic_downcast<const event_object*>(&obj);
   if(0 == ephemeral_event_object.count(event_obj.id))
      ephemeral_event_object.insert( {event_obj.id, event_obj} );
   else
      ephemeral_event_object[event_obj.id] = event_obj;
}
void persistent_event_index::object_modified(const object& after)
{
   const event_object& event_obj = *boost::polymorphic_downcast<const event_object*>(&after);
   auto iter = ephemeral_event_object.find(event_obj.id);
   assert (iter != ephemeral_event_object.end());
   if (iter != ephemeral_event_object.end())
      iter->second = event_obj;
}

//////////// end event_object ///////////////////
class bookie_plugin_impl
{
   public:
      bookie_plugin_impl(bookie_plugin& _plugin)
         : _self( _plugin )
      { }
      virtual ~bookie_plugin_impl();

      /**
       *  Called After a block has been applied and committed.  The callback
       *  should not yield and should execute quickly.
       */
      void on_objects_changed(const vector<object_id_type>& changed_object_ids);

      void on_objects_new(const vector<object_id_type>& new_object_ids);
      void on_objects_removed(const vector<object_id_type>& removed_object_ids);

      /** this method is called as a callback after a block is applied
       * and will process/index all operations that were applied in the block.
       */
      void on_block_applied( const signed_block& b );

      asset get_total_matched_bet_amount_for_betting_market_group(betting_market_group_id_type group_id);

      void fill_localized_event_strings();

      std::vector<event_object> get_events_containing_sub_string(const std::string& sub_string, const std::string& language);

      graphene::chain::database& database()
      {
         return _self.database();
      }

      //                1.18.          "Washington Capitals/Chicago Blackhawks"
      typedef std::pair<event_id_type, std::string> event_string;
      struct event_string_less : public std::less<const event_string&>
      {
          bool operator()(const event_string &_left, const event_string &_right) const
          {
              return (_left.first.instance < _right.first.instance);
          }
      };

      typedef flat_set<event_string, event_string_less> event_string_set;
      //       "en"
      std::map<std::string, event_string_set> localized_event_strings;

      bookie_plugin& _self;
      flat_set<account_id_type> _tracked_accounts;
};

bookie_plugin_impl::~bookie_plugin_impl()
{
}

void bookie_plugin_impl::on_objects_new(const vector<object_id_type>& new_object_ids)
{
}

void bookie_plugin_impl::on_objects_removed(const vector<object_id_type>& removed_object_ids)
{
}

void bookie_plugin_impl::on_objects_changed(const vector<object_id_type>& changed_object_ids)
{
}

bool is_operation_history_object_stored(operation_history_id_type id)
{
   if (id == operation_history_id_type())
   {
      elog("Warning: the operation history object for an operation the bookie plugin needs to track "
           "has id of ${id}, which means the account history plugin isn't storing this operation, or that "
           "it is running after the bookie plugin. Make sure the account history plugin is tracking operations for "
           "all accounts,, and that it is loaded before the bookie plugin", ("id", id));
      return false;
   }
   else
      return true;
}

void bookie_plugin_impl::on_block_applied( const signed_block& )
{ try {

   graphene::chain::database& db = database();
   const vector<optional<operation_history_object> >& hist = db.get_applied_operations();
   for( const optional<operation_history_object>& o_op : hist )
   {
      if( !o_op.valid() )
         continue;

      const operation_history_object& op = *o_op;
      if( op.op.which() == operation::tag<bet_matched_operation>::value )
      {
         const bet_matched_operation& bet_matched_op = op.op.get<bet_matched_operation>();
         //idump((bet_matched_op));
         const asset& amount_bet = bet_matched_op.amount_bet;
         // object may no longer exist
         //const bet_object& bet = bet_matched_op.bet_id(db);
         const auto &idx_bet_object = db.get_index_type<bet_object_index>();
         const auto &aidx_bet_object = dynamic_cast<const base_primary_index &>(idx_bet_object);
         const auto &refs_bet_object = aidx_bet_object.get_secondary_index<detail::persistent_bet_index>();
         auto& nonconst_refs_bet_object = const_cast<persistent_bet_index&>(refs_bet_object);

         auto bet_iter = nonconst_refs_bet_object.internal.find(bet_matched_op.bet_id);
         assert(bet_iter != nonconst_refs_bet_object.internal.end());
         if (bet_iter != nonconst_refs_bet_object.internal.end())
         {
            bet_iter->second.amount_matched += amount_bet.amount;
            if (is_operation_history_object_stored(op.id))
               bet_iter->second.associated_operations.emplace_back(op.id);

            const bet_object& bet_obj = bet_iter->second.ephemeral_bet_object;

            const auto &idx_betting_market = db.get_index_type<betting_market_object_index>();
            const auto &aidx_betting_market = dynamic_cast<const base_primary_index &>(idx_betting_market);
            const auto &refs_betting_market = aidx_betting_market.get_secondary_index<detail::persistent_betting_market_index>();
            auto persistent_betting_market_object_iter = refs_betting_market.ephemeral_betting_market_object.find(bet_obj.betting_market_id);
            FC_ASSERT(persistent_betting_market_object_iter != refs_betting_market.ephemeral_betting_market_object.end());
            const betting_market_object& betting_market = persistent_betting_market_object_iter->second;

            const auto &idx_betting_market_group = db.get_index_type<betting_market_group_object_index>();
            const auto &aidx_betting_market_group = dynamic_cast<const base_primary_index &>(idx_betting_market_group);
            const auto &refs_betting_market_group = aidx_betting_market_group.get_secondary_index<detail::persistent_betting_market_group_index>();
            auto& nonconst_refs_betting_market_group = const_cast<persistent_betting_market_group_index&>(refs_betting_market_group);
            auto persistent_betting_market_group_object_iter = nonconst_refs_betting_market_group.internal.find(betting_market.group_id);
            FC_ASSERT(persistent_betting_market_group_object_iter != nonconst_refs_betting_market_group.internal.end());
            const betting_market_group_object& betting_market_group = persistent_betting_market_group_object_iter->second.ephemeral_betting_market_group_object;

            // if the object is still in the main database, keep the running total there
            // otherwise, add it directly to the persistent version
            auto& betting_market_group_idx = db.get_index_type<betting_market_group_object_index>().indices().get<by_id>();
            auto betting_market_group_iter = betting_market_group_idx.find(betting_market_group.id);
            if (betting_market_group_iter != betting_market_group_idx.end())
               db.modify( *betting_market_group_iter, [&]( betting_market_group_object& obj ){
                  obj.total_matched_bets_amount += amount_bet.amount;
               });
            else
               persistent_betting_market_group_object_iter->second.total_matched_bets_amount += amount_bet.amount;
         }
      }
      else if( op.op.which() == operation::tag<event_create_operation>::value )
      {
         FC_ASSERT(op.result.which() == operation_result::tag<object_id_type>::value);
         //object_id_type object_id = op.result.get<object_id_type>();
         event_id_type object_id = op.result.get<object_id_type>();
         FC_ASSERT( db.find_object(object_id), "invalid event specified" );
         const event_create_operation& event_create_op = op.op.get<event_create_operation>();
         for(const std::pair<std::string, std::string>& pair : event_create_op.name)
            localized_event_strings[pair.first].insert(event_string(object_id, pair.second));
      }
      else if( op.op.which() == operation::tag<event_update_operation>::value )
      {
         const event_update_operation& event_create_op = op.op.get<event_update_operation>();
         if (!event_create_op.new_name.valid())
            continue;
         event_id_type event_id = event_create_op.event_id;
         for(const std::pair<std::string, std::string>& pair : *event_create_op.new_name)
         {
            // try insert
            std::pair<event_string_set::iterator, bool> result =
               localized_event_strings[pair.first].insert(event_string(event_id, pair.second));
            if (!result.second)
               //  update string only
               result.first->second = pair.second;
         }
      }
      else if ( op.op.which() == operation::tag<bet_canceled_operation>::value )
      {
         const bet_canceled_operation& bet_canceled_op = op.op.get<bet_canceled_operation>();
         const auto &idx_bet_object = db.get_index_type<bet_object_index>();
         const auto &aidx_bet_object = dynamic_cast<const base_primary_index &>(idx_bet_object);
         const auto &refs_bet_object = aidx_bet_object.get_secondary_index<detail::persistent_bet_index>();
         auto& nonconst_refs_bet_object = const_cast<persistent_bet_index&>(refs_bet_object);

         auto bet_iter = nonconst_refs_bet_object.internal.find(bet_canceled_op.bet_id);
         assert(bet_iter != nonconst_refs_bet_object.internal.end());
         if (bet_iter != nonconst_refs_bet_object.internal.end())
         {
            // ilog("Adding bet_canceled_operation ${canceled_id} to bet ${bet_id}'s associated operations", 
            //     ("canceled_id", op.id)("bet_id", bet_canceled_op.bet_id));
            bet_iter->second.associated_operations.emplace_back(op.id);
         }
      }
      else if ( op.op.which() == operation::tag<bet_adjusted_operation>::value )
      {
         const bet_adjusted_operation& bet_adjusted_op = op.op.get<bet_adjusted_operation>();
         const auto &idx_bet_object = db.get_index_type<bet_object_index>();
         const auto &aidx_bet_object = dynamic_cast<const base_primary_index &>(idx_bet_object);
         const auto &refs_bet_object = aidx_bet_object.get_secondary_index<detail::persistent_bet_index>();
         auto& nonconst_refs_bet_object = const_cast<persistent_bet_index&>(refs_bet_object);

         auto bet_iter = nonconst_refs_bet_object.internal.find(bet_adjusted_op.bet_id);
         assert(bet_iter != nonconst_refs_bet_object.internal.end());
         if (bet_iter != nonconst_refs_bet_object.internal.end())
         {
            // ilog("Adding bet_adjusted_operation ${adjusted_id} to bet ${bet_id}'s associated operations", 
            //     ("adjusted_id", op.id)("bet_id", bet_adjusted_op.bet_id));
            bet_iter->second.associated_operations.emplace_back(op.id);
         }
      }

   }
} FC_RETHROW_EXCEPTIONS( warn, "" ) }

void bookie_plugin_impl::fill_localized_event_strings()
{
       graphene::chain::database& db = database();
       const auto& event_index = db.get_index_type<event_object_index>().indices().get<by_id>();
       auto event_itr = event_index.cbegin();
       while (event_itr != event_index.cend())
       {
           const event_object& event_obj = *event_itr;
           ++event_itr;
           for(const std::pair<std::string, std::string>& pair : event_obj.name)
           {
                localized_event_strings[pair.first].insert(event_string(event_obj.id, pair.second));
           }
       }
}

std::vector<event_object> bookie_plugin_impl::get_events_containing_sub_string(const std::string& sub_string, const std::string& language)
{
   graphene::chain::database& db = database();
   std::vector<event_object> events;
   if (localized_event_strings.find(language) != localized_event_strings.end())
   {
      std::string lower_case_sub_string = boost::algorithm::to_lower_copy(sub_string);
      const event_string_set& language_set = localized_event_strings[language];
      for (const event_string& pair : language_set)
      {
         std::string lower_case_string = boost::algorithm::to_lower_copy(pair.second);
         if (lower_case_string.find(lower_case_sub_string) != std::string::npos)
            events.push_back(pair.first(db));
      }
   }
   return events;
}

asset bookie_plugin_impl::get_total_matched_bet_amount_for_betting_market_group(betting_market_group_id_type group_id)
{
   graphene::chain::database& db = database();
   FC_ASSERT( db.find_object(group_id), "Invalid betting market group specified" );
   const betting_market_group_object& betting_market_group =  group_id(db);
   return asset(betting_market_group.total_matched_bets_amount, betting_market_group.asset_id);
}
} // end namespace detail

bookie_plugin::bookie_plugin() :
   my( new detail::bookie_plugin_impl(*this) )
{
}

bookie_plugin::~bookie_plugin()
{
}

std::string bookie_plugin::plugin_name()const
{
   return "bookie";
}

void bookie_plugin::plugin_set_program_options(boost::program_options::options_description& cli, 
                                               boost::program_options::options_description& cfg)
{
   //cli.add_options()
   //      ("track-account", boost::program_options::value<std::vector<std::string>>()->composing()->multitoken(), "Account ID to track history for (may specify multiple times)")
   //      ;
   //cfg.add(cli);
}

void bookie_plugin::plugin_initialize(const boost::program_options::variables_map& options)
{
    ilog("bookie plugin: plugin_startup() begin");
    database().force_slow_replays();
    database().applied_block.connect( [&]( const signed_block& b){ my->on_block_applied(b); } );
    database().changed_objects.connect([&](const vector<object_id_type>& changed_object_ids, const fc::flat_set<graphene::chain::account_id_type>& impacted_accounts){ my->on_objects_changed(changed_object_ids); });
    database().new_objects.connect([this](const vector<object_id_type>& ids, const flat_set<account_id_type>& impacted_accounts) { my->on_objects_new(ids); });
    database().removed_objects.connect([this](const vector<object_id_type>& ids, const vector<const object*>& objs, const flat_set<account_id_type>& impacted_accounts) { my->on_objects_removed(ids); });

    const primary_index<bet_object_index>& bet_object_idx = database().get_index_type<primary_index<bet_object_index> >();
    primary_index<bet_object_index>& nonconst_bet_object_idx = const_cast<primary_index<bet_object_index>&>(bet_object_idx);
    nonconst_bet_object_idx.add_secondary_index<detail::persistent_bet_index>();

    const primary_index<betting_market_object_index>& betting_market_object_idx = database().get_index_type<primary_index<betting_market_object_index> >();
    primary_index<betting_market_object_index>& nonconst_betting_market_object_idx = const_cast<primary_index<betting_market_object_index>&>(betting_market_object_idx);
    nonconst_betting_market_object_idx.add_secondary_index<detail::persistent_betting_market_index>();

    const primary_index<betting_market_group_object_index>& betting_market_group_object_idx = database().get_index_type<primary_index<betting_market_group_object_index> >();
    primary_index<betting_market_group_object_index>& nonconst_betting_market_group_object_idx = const_cast<primary_index<betting_market_group_object_index>&>(betting_market_group_object_idx);
    nonconst_betting_market_group_object_idx.add_secondary_index<detail::persistent_betting_market_group_index>();

    const primary_index<event_object_index>& event_object_idx = database().get_index_type<primary_index<event_object_index> >();
    primary_index<event_object_index>& nonconst_event_object_idx = const_cast<primary_index<event_object_index>&>(event_object_idx);
    nonconst_event_object_idx.add_secondary_index<detail::persistent_event_index>();

    ilog("bookie plugin: plugin_startup() end");
 }

void bookie_plugin::plugin_startup()
{
   ilog("bookie plugin: plugin_startup()");
    my->fill_localized_event_strings();
}

flat_set<account_id_type> bookie_plugin::tracked_accounts() const
{
   return my->_tracked_accounts;
}

asset bookie_plugin::get_total_matched_bet_amount_for_betting_market_group(betting_market_group_id_type group_id)
{
     ilog("bookie plugin: get_total_matched_bet_amount_for_betting_market_group($group_id)", ("group_d", group_id));
     return my->get_total_matched_bet_amount_for_betting_market_group(group_id);
}
std::vector<event_object> bookie_plugin::get_events_containing_sub_string(const std::string& sub_string, const std::string& language)
{
    ilog("bookie plugin: get_events_containing_sub_string(${sub_string}, ${language})", (sub_string)(language));
    return my->get_events_containing_sub_string(sub_string, language);
}

} }

