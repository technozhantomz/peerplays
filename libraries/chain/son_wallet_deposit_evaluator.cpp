#include <graphene/chain/son_wallet_deposit_evaluator.hpp>

#include <graphene/chain/database.hpp>
#include <graphene/chain/is_authorized_asset.hpp>
#include <graphene/chain/son_object.hpp>
#include <graphene/chain/son_wallet_deposit_object.hpp>

namespace graphene { namespace chain {

void_result create_son_wallet_deposit_evaluator::do_evaluate(const son_wallet_deposit_create_operation& op)
{ try{
   FC_ASSERT(db().head_block_time() >= HARDFORK_SON_TIME, "Not allowed until SON HARDFORK");
   FC_ASSERT( op.payer == db().get_global_properties().parameters.son_account(), "SON paying account must be set as payer." );

   const auto &idx = db().get_index_type<son_stats_index>().indices().get<by_owner>();
   FC_ASSERT(idx.find(op.son_id) != idx.end(), "Statistic object for a given SON ID does not exists");

   return void_result();
} FC_CAPTURE_AND_RETHROW( (op) ) }

object_id_type create_son_wallet_deposit_evaluator::do_apply(const son_wallet_deposit_create_operation& op)
{ try {
   const auto& idx = db().get_index_type<son_wallet_deposit_index>().indices().get<by_sidechain_uid>();
   auto itr = idx.find(op.sidechain_uid);
   if (itr == idx.end()) {
      const auto& new_son_wallet_deposit_object = db().create<son_wallet_deposit_object>( [&]( son_wallet_deposit_object& swdo ){
         swdo.timestamp = op.timestamp;
         swdo.sidechain = op.sidechain;
         swdo.sidechain_uid = op.sidechain_uid;
         swdo.sidechain_transaction_id = op.sidechain_transaction_id;
         swdo.sidechain_from = op.sidechain_from;
         swdo.sidechain_to = op.sidechain_to;
         swdo.sidechain_currency = op.sidechain_currency;
         swdo.sidechain_amount = op.sidechain_amount;
         swdo.peerplays_from = op.peerplays_from;
         swdo.peerplays_to = op.peerplays_to;
         swdo.peerplays_asset = op.peerplays_asset;

         auto &gpo = db().get_global_properties();
         for (auto &si : gpo.active_sons) {
            swdo.expected_reports.insert(si.son_id);

            auto stats_itr = db().get_index_type<son_stats_index>().indices().get<by_owner>().find(si.son_id);
            db().modify(*stats_itr, [&op, &si](son_statistics_object &sso) {
               sso.total_sidechain_txs_reported = sso.total_sidechain_txs_reported + 1;
               if (si.son_id == op.son_id) {
                  sso.sidechain_txs_reported = sso.sidechain_txs_reported + 1;
               }
            });
         }

         swdo.received_reports.insert(op.son_id);

         swdo.processed = false;
      });
      return new_son_wallet_deposit_object.id;
   } else {
      db().modify(*itr, [&op](son_wallet_deposit_object &swdo) {
         swdo.received_reports.insert(op.son_id);
      });
      auto stats_itr = db().get_index_type<son_stats_index>().indices().get<by_owner>().find(op.son_id);
      db().modify(*stats_itr, [&op](son_statistics_object &sso) {
         sso.sidechain_txs_reported = sso.sidechain_txs_reported + 1;
      });
      return (*itr).id;
   }
} FC_CAPTURE_AND_RETHROW( (op) ) }

void_result process_son_wallet_deposit_evaluator::do_evaluate(const son_wallet_deposit_process_operation& op)
{ try{
   FC_ASSERT(db().head_block_time() >= HARDFORK_SON_TIME, "Not allowed until SON HARDFORK");
   FC_ASSERT( op.payer == db().get_global_properties().parameters.son_account(), "SON paying account must be set as payer." );

   const auto& idx = db().get_index_type<son_wallet_deposit_index>().indices().get<by_id>();
   const auto& itr = idx.find(op.son_wallet_deposit_id);
   FC_ASSERT(itr != idx.end(), "Son wallet deposit not found");
   FC_ASSERT(!itr->processed, "Son wallet deposit is already processed");
   return void_result();
} FC_CAPTURE_AND_RETHROW( (op) ) }

object_id_type process_son_wallet_deposit_evaluator::do_apply(const son_wallet_deposit_process_operation& op)
{ try {
   const auto& idx = db().get_index_type<son_wallet_deposit_index>().indices().get<by_id>();
   auto itr = idx.find(op.son_wallet_deposit_id);
   if(itr != idx.end())
   {
      db().modify(*itr, [&op](son_wallet_deposit_object &swdo) {
         swdo.processed = true;
      });
   }
   return op.son_wallet_deposit_id;
} FC_CAPTURE_AND_RETHROW( (op) ) }

} } // namespace graphene::chain
