#include <graphene/chain/son_wallet_deposit_evaluator.hpp>

#include <graphene/chain/database.hpp>
#include <graphene/chain/chain_property_object.hpp>
#include <graphene/chain/is_authorized_asset.hpp>
#include <graphene/chain/son_object.hpp>
#include <graphene/chain/son_wallet_deposit_object.hpp>

namespace graphene { namespace chain {

void_result create_son_wallet_deposit_evaluator::do_evaluate(const son_wallet_deposit_create_operation& op)
{ try {
   FC_ASSERT(db().head_block_time() >= HARDFORK_SON_TIME, "Not allowed until SON HARDFORK");

   const auto &son_idx = db().get_index_type<son_index>().indices().get<by_id>();
   const auto so = son_idx.find(op.son_id);
   FC_ASSERT(so != son_idx.end(), "SON not found");
   FC_ASSERT(so->son_account == op.payer, "Payer is not SON account owner");

   const auto &ss_idx = db().get_index_type<son_stats_index>().indices().get<by_owner>();
   FC_ASSERT(ss_idx.find(op.son_id) != ss_idx.end(), "Statistic object for a given SON ID does not exists");

   const auto &swdo_idx = db().get_index_type<son_wallet_deposit_index>().indices().get<by_sidechain_uid>();
   const auto swdo = swdo_idx.find(op.sidechain_uid);
   if (swdo == swdo_idx.end()) {
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
      exactly_the_same = exactly_the_same && (swdo->sidechain == op.sidechain);
      exactly_the_same = exactly_the_same && (swdo->sidechain_uid == op.sidechain_uid);
      exactly_the_same = exactly_the_same && (swdo->sidechain_transaction_id == op.sidechain_transaction_id);
      exactly_the_same = exactly_the_same && (swdo->sidechain_from == op.sidechain_from);
      exactly_the_same = exactly_the_same && (swdo->sidechain_to == op.sidechain_to);
      exactly_the_same = exactly_the_same && (swdo->sidechain_currency == op.sidechain_currency);
      exactly_the_same = exactly_the_same && (swdo->sidechain_amount == op.sidechain_amount);
      exactly_the_same = exactly_the_same && (swdo->peerplays_from == op.peerplays_from);
      exactly_the_same = exactly_the_same && (swdo->peerplays_to == op.peerplays_to);
      exactly_the_same = exactly_the_same && (swdo->peerplays_asset == op.peerplays_asset);
      FC_ASSERT(exactly_the_same, "Invalid withdraw confirmation");

      bool expected = false;
      for (auto &exp : swdo->expected_reports) {
         if (op.son_id == exp.first) {
            expected = true;
            break;
         }
      }
      FC_ASSERT(expected, "Confirmation from this SON not expected");
   }

   return void_result();
} FC_CAPTURE_AND_RETHROW( (op) ) }

object_id_type create_son_wallet_deposit_evaluator::do_apply(const son_wallet_deposit_create_operation& op)
{ try {
   const auto& idx = db().get_index_type<son_wallet_deposit_index>().indices().get<by_sidechain_uid>();
   auto itr = idx.find(op.sidechain_uid);
   if (itr == idx.end()) {
      const auto& new_son_wallet_deposit_object = db().create<son_wallet_deposit_object>( [&]( son_wallet_deposit_object& swdo ){
         swdo.timestamp = op.timestamp;
         swdo.block_num = op.block_num;
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
            swdo.expected_reports.insert(std::make_pair(si.son_id, si.weight));

            auto stats_itr = db().get_index_type<son_stats_index>().indices().get<by_owner>().find(si.son_id);
            db().modify(*stats_itr, [&op, &si](son_statistics_object &sso) {
               if (sso.total_sidechain_txs_reported.find(op.sidechain) == sso.total_sidechain_txs_reported.end()) {
                  sso.total_sidechain_txs_reported[op.sidechain] = 0;
               }
               sso.total_sidechain_txs_reported[op.sidechain] += 1;

               if (si.son_id == op.son_id) {
                  if (sso.sidechain_txs_reported.find(op.sidechain) == sso.sidechain_txs_reported.end()) {
                     sso.sidechain_txs_reported[op.sidechain] = 0;
                  }
                  sso.sidechain_txs_reported[op.sidechain] += 1;
               }
            });
         }

         swdo.received_reports.insert(op.son_id);

         uint64_t total_weight = 0;
         for (const auto exp : swdo.expected_reports) {
             total_weight = total_weight + exp.second;
         }
         uint64_t current_weight =  0;
         for (const auto rec : swdo.received_reports) {
             current_weight = current_weight + swdo.expected_reports.find(rec)->second;
         }
         swdo.confirmed = (current_weight > (total_weight * 2 / 3));

         swdo.processed = false;
      });
      return new_son_wallet_deposit_object.id;
   } else {
      db().modify(*itr, [&op](son_wallet_deposit_object &swdo) {
         swdo.received_reports.insert(op.son_id);

         uint64_t total_weight = 0;
         for (const auto exp : swdo.expected_reports) {
             total_weight = total_weight + exp.second;
         }
         uint64_t current_weight = 0;
         for (const auto rec : swdo.received_reports) {
             current_weight = current_weight + swdo.expected_reports.find(rec)->second;
         }
         swdo.confirmed = (current_weight > (total_weight * 2 / 3));
      });
      auto stats_itr = db().get_index_type<son_stats_index>().indices().get<by_owner>().find(op.son_id);
      db().modify(*stats_itr, [&op](son_statistics_object &sso) {
         if (sso.sidechain_txs_reported.find(op.sidechain) == sso.sidechain_txs_reported.end()) {
            sso.sidechain_txs_reported[op.sidechain] = 0;
         }
         sso.sidechain_txs_reported[op.sidechain] += 1;
      });
      return (*itr).id;
   }
} FC_CAPTURE_AND_RETHROW( (op) ) }

void_result process_son_wallet_deposit_evaluator::do_evaluate(const son_wallet_deposit_process_operation& op)
{ try{
   FC_ASSERT(db().head_block_time() >= HARDFORK_SON_TIME, "Not allowed until SON HARDFORK");
   FC_ASSERT( op.payer == db().get_global_properties().parameters.son_account(), "SON paying account must be set as payer." );
   FC_ASSERT(db().get_global_properties().active_sons.size() >= db().get_chain_properties().immutable_parameters.min_son_count, "Min required voted SONs not present");

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
