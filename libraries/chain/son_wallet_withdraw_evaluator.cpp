#include <graphene/chain/son_wallet_withdraw_evaluator.hpp>

#include <graphene/chain/database.hpp>
#include <graphene/chain/is_authorized_asset.hpp>
#include <graphene/chain/son_object.hpp>
#include <graphene/chain/son_wallet_withdraw_object.hpp>

namespace graphene { namespace chain {

void_result create_son_wallet_withdraw_evaluator::do_evaluate(const son_wallet_withdraw_create_operation& op)
{ try {
   FC_ASSERT(db().head_block_time() >= HARDFORK_SON_TIME, "Not allowed until SON HARDFORK");

   const auto &son_idx = db().get_index_type<son_index>().indices().get<by_id>();
   const auto so = son_idx.find(op.son_id);
   FC_ASSERT(so != son_idx.end(), "SON not found");
   FC_ASSERT(so->son_account == op.payer, "Payer is not SON account owner");

   const auto &ss_idx = db().get_index_type<son_stats_index>().indices().get<by_owner>();
   FC_ASSERT(ss_idx.find(op.son_id) != ss_idx.end(), "Statistic object for a given SON ID does not exists");

   const auto &swwo_idx = db().get_index_type<son_wallet_withdraw_index>().indices().get<by_peerplays_uid>();
   const auto swwo = swwo_idx.find(op.peerplays_uid);
   if (swwo == swwo_idx.end()) {
      auto &gpo = db().get_global_properties();
      bool expected = false;
      for (auto &si : gpo.active_sons) {
         if (op.son_id == si.son_id) {
            expected = true;
            break;
         }
      }
      FC_ASSERT(expected, "Only active SON can create deposit");
   } else {
      bool exactly_the_same = true;
      exactly_the_same = exactly_the_same && (swwo->sidechain == op.sidechain);
      exactly_the_same = exactly_the_same && (swwo->peerplays_uid == op.peerplays_uid);
      exactly_the_same = exactly_the_same && (swwo->peerplays_transaction_id == op.peerplays_transaction_id);
      exactly_the_same = exactly_the_same && (swwo->peerplays_from == op.peerplays_from);
      exactly_the_same = exactly_the_same && (swwo->peerplays_asset == op.peerplays_asset);
      exactly_the_same = exactly_the_same && (swwo->withdraw_sidechain == op.withdraw_sidechain);
      exactly_the_same = exactly_the_same && (swwo->withdraw_address == op.withdraw_address);
      exactly_the_same = exactly_the_same && (swwo->withdraw_currency == op.withdraw_currency);
      exactly_the_same = exactly_the_same && (swwo->withdraw_amount == op.withdraw_amount);
      FC_ASSERT(exactly_the_same, "Invalid withdraw confirmation");

      bool expected = false;
      for (auto &exp : swwo->expected_reports) {
         if (op.son_id == exp.first) {
            expected = true;
            break;
         }
      }
      FC_ASSERT(expected, "Confirmation from this SON not expected");
   }

   return void_result();
} FC_CAPTURE_AND_RETHROW( (op) ) }

object_id_type create_son_wallet_withdraw_evaluator::do_apply(const son_wallet_withdraw_create_operation& op)
{ try {
   const auto& idx = db().get_index_type<son_wallet_withdraw_index>().indices().get<by_peerplays_uid>();
   auto itr = idx.find(op.peerplays_uid);
   if (itr == idx.end()) {
      const auto& new_son_wallet_withdraw_object = db().create<son_wallet_withdraw_object>( [&]( son_wallet_withdraw_object& swwo ){
         swwo.timestamp = op.timestamp;
         swwo.sidechain = op.sidechain;
         swwo.peerplays_uid = op.peerplays_uid;
         swwo.peerplays_transaction_id = op.peerplays_transaction_id;
         swwo.peerplays_from = op.peerplays_from;
         swwo.peerplays_asset = op.peerplays_asset;
         swwo.withdraw_sidechain = op.withdraw_sidechain;
         swwo.withdraw_address = op.withdraw_address;
         swwo.withdraw_currency = op.withdraw_currency;
         swwo.withdraw_amount = op.withdraw_amount;

         auto &gpo = db().get_global_properties();
         for (auto &si : gpo.active_sons) {
            swwo.expected_reports.insert(std::make_pair(si.son_id, si.weight));

            auto stats_itr = db().get_index_type<son_stats_index>().indices().get<by_owner>().find(si.son_id);
            db().modify(*stats_itr, [&op, &si](son_statistics_object &sso) {
               sso.total_sidechain_txs_reported = sso.total_sidechain_txs_reported + 1;
               if (si.son_id == op.son_id) {
                  sso.sidechain_txs_reported = sso.sidechain_txs_reported + 1;
               }
            });
         }

         swwo.received_reports.insert(op.son_id);

         uint64_t total_weight = 0;
         for (const auto exp : swwo.expected_reports) {
             total_weight = total_weight + exp.second;
         }
         uint64_t current_weight = 0;
         for (const auto rec : swwo.received_reports) {
             current_weight = current_weight + swwo.expected_reports.find(rec)->second;
         }
         swwo.confirmed = (current_weight > (total_weight * 2 / 3));

         swwo.processed = false;
      });
      return new_son_wallet_withdraw_object.id;
   } else {
      db().modify(*itr, [&op](son_wallet_withdraw_object &swwo) {
         swwo.received_reports.insert(op.son_id);

         uint64_t total_weight = 0;
         for (const auto exp : swwo.expected_reports) {
             total_weight = total_weight + exp.second;
         }
         uint64_t current_weight = 0;
         for (const auto rec : swwo.received_reports) {
             current_weight = current_weight + swwo.expected_reports.find(rec)->second;
         }
         swwo.confirmed = (current_weight > (total_weight * 2 / 3));
      });
      auto stats_itr = db().get_index_type<son_stats_index>().indices().get<by_owner>().find(op.son_id);
      db().modify(*stats_itr, [&op](son_statistics_object &sso) {
         sso.sidechain_txs_reported = sso.sidechain_txs_reported + 1;
      });
      return (*itr).id;
   }
} FC_CAPTURE_AND_RETHROW( (op) ) }

void_result process_son_wallet_withdraw_evaluator::do_evaluate(const son_wallet_withdraw_process_operation& op)
{ try{
   FC_ASSERT(db().head_block_time() >= HARDFORK_SON_TIME, "Not allowed until SON HARDFORK");
   FC_ASSERT( op.payer == db().get_global_properties().parameters.son_account(), "SON paying account must be set as payer." );

   const auto& idx = db().get_index_type<son_wallet_withdraw_index>().indices().get<by_id>();
   const auto& itr = idx.find(op.son_wallet_withdraw_id);
   FC_ASSERT(itr != idx.end(), "Son wallet withdraw not found");
   FC_ASSERT(!itr->processed, "Son wallet withdraw is already processed");
   return void_result();
} FC_CAPTURE_AND_RETHROW( (op) ) }

object_id_type process_son_wallet_withdraw_evaluator::do_apply(const son_wallet_withdraw_process_operation& op)
{ try {
   const auto& idx = db().get_index_type<son_wallet_withdraw_index>().indices().get<by_id>();
   auto itr = idx.find(op.son_wallet_withdraw_id);
   if(itr != idx.end())
   {
      db().modify(*itr, [&op](son_wallet_withdraw_object &swwo) {
         swwo.processed = true;
      });
   }
   return op.son_wallet_withdraw_id;
} FC_CAPTURE_AND_RETHROW( (op) ) }

} } // namespace graphene::chain
