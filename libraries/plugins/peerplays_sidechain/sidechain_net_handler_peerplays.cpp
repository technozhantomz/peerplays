#include <graphene/peerplays_sidechain/sidechain_net_handler_peerplays.hpp>

#include <algorithm>
#include <thread>

#include <boost/algorithm/hex.hpp>
#include <boost/property_tree/json_parser.hpp>
#include <boost/property_tree/ptree.hpp>

#include <fc/crypto/base64.hpp>
#include <fc/log/logger.hpp>
#include <fc/network/ip.hpp>

#include <graphene/chain/account_object.hpp>
#include <graphene/chain/protocol/son_wallet.hpp>
#include <graphene/chain/son_info.hpp>
#include <graphene/chain/son_wallet_object.hpp>

namespace graphene { namespace peerplays_sidechain {

sidechain_net_handler_peerplays::sidechain_net_handler_peerplays(peerplays_sidechain_plugin &_plugin, const boost::program_options::variables_map &options) :
      sidechain_net_handler(_plugin, options) {
   sidechain = sidechain_type::peerplays;
   database.applied_block.connect([&](const signed_block &b) {
      on_applied_block(b);
   });
}

sidechain_net_handler_peerplays::~sidechain_net_handler_peerplays() {
}

bool sidechain_net_handler_peerplays::process_proposal(const proposal_object &po) {

   ilog("Proposal to process: ${po}, SON id ${son_id}", ("po", po.id)("son_id", plugin.get_current_son_id()));

   bool should_approve = false;

   const chain::global_property_object &gpo = database.get_global_properties();

   int32_t op_idx_0 = -1;
   chain::operation op_obj_idx_0;
   //int32_t op_idx_1 = -1;
   //chain::operation op_obj_idx_1;

   if (po.proposed_transaction.operations.size() >= 1) {
      op_idx_0 = po.proposed_transaction.operations[0].which();
      op_obj_idx_0 = po.proposed_transaction.operations[0];
   }

   if (po.proposed_transaction.operations.size() >= 2) {
      //op_idx_1 = po.proposed_transaction.operations[1].which();
      //op_obj_idx_1 = po.proposed_transaction.operations[1];
   }

   switch (op_idx_0) {

   case chain::operation::tag<chain::son_wallet_update_operation>::value: {
      should_approve = true;
      break;
   }

   case chain::operation::tag<chain::son_wallet_deposit_process_operation>::value: {
      son_wallet_deposit_id_type swdo_id = op_obj_idx_0.get<son_wallet_deposit_process_operation>().son_wallet_deposit_id;
      const auto &idx = database.get_index_type<son_wallet_deposit_index>().indices().get<by_id>();
      const auto swdo = idx.find(swdo_id);
      if (swdo != idx.end()) {

         uint32_t swdo_block_num = swdo->block_num;
         std::string swdo_sidechain_transaction_id = swdo->sidechain_transaction_id;
         uint32_t swdo_op_idx = std::stoll(swdo->sidechain_uid.substr(swdo->sidechain_uid.find_last_of("-") + 1));

         const auto &block = database.fetch_block_by_number(swdo_block_num);

         for (const auto &tx : block->transactions) {
            if (tx.id().str() == swdo_sidechain_transaction_id) {
               operation op = tx.operations[swdo_op_idx];
               transfer_operation t_op = op.get<transfer_operation>();

               asset sidechain_asset = asset(swdo->sidechain_amount, fc::variant(swdo->sidechain_currency, 1).as<asset_id_type>(1));
               price sidechain_asset_price = database.get<asset_object>(sidechain_asset.asset_id).options.core_exchange_rate;
               asset peerplays_asset = asset(sidechain_asset.amount * sidechain_asset_price.base.amount / sidechain_asset_price.quote.amount);

               should_approve = (gpo.parameters.son_account() == t_op.to) &&
                                (swdo->peerplays_from == t_op.from) &&
                                (sidechain_asset == t_op.amount) &&
                                (swdo->peerplays_asset == peerplays_asset);
               break;
            }
         }
      }
      break;
   }

   case chain::operation::tag<chain::son_wallet_withdraw_process_operation>::value: {
      son_wallet_withdraw_id_type swwo_id = op_obj_idx_0.get<son_wallet_withdraw_process_operation>().son_wallet_withdraw_id;
      const auto &idx = database.get_index_type<son_wallet_withdraw_index>().indices().get<by_id>();
      const auto swwo = idx.find(swwo_id);
      if (swwo != idx.end()) {

         uint32_t swwo_block_num = swwo->block_num;
         std::string swwo_peerplays_transaction_id = swwo->peerplays_transaction_id;
         uint32_t swwo_op_idx = std::stoll(swwo->peerplays_uid.substr(swwo->peerplays_uid.find_last_of("-") + 1));

         const auto &block = database.fetch_block_by_number(swwo_block_num);

         for (const auto &tx : block->transactions) {
            if (tx.id().str() == swwo_peerplays_transaction_id) {
               operation op = tx.operations[swwo_op_idx];
               transfer_operation t_op = op.get<transfer_operation>();

               price asset_price = database.get<asset_object>(t_op.amount.asset_id).options.core_exchange_rate;
               asset peerplays_asset = asset(t_op.amount.amount * asset_price.base.amount / asset_price.quote.amount);

               should_approve = (t_op.to == gpo.parameters.son_account()) &&
                                (swwo->peerplays_from == t_op.from) &&
                                (swwo->peerplays_asset == peerplays_asset);
               break;
            }
         }
      }
      break;
   }

   case chain::operation::tag<chain::sidechain_transaction_create_operation>::value: {
      should_approve = true;
      break;
   }

   default:
      should_approve = false;
      elog("==================================================");
      elog("Proposal not considered for approval ${po}", ("po", po));
      elog("==================================================");
   }

   return should_approve;
}

void sidechain_net_handler_peerplays::process_primary_wallet() {
   return;
}

bool sidechain_net_handler_peerplays::process_deposit(const son_wallet_deposit_object &swdo) {
   return true;
}

bool sidechain_net_handler_peerplays::process_withdrawal(const son_wallet_withdraw_object &swwo) {
   return true;
}

std::string sidechain_net_handler_peerplays::process_sidechain_transaction(const sidechain_transaction_object &sto, bool &complete) {
   complete = true;
   return sto.transaction;
}

bool sidechain_net_handler_peerplays::send_sidechain_transaction(const sidechain_transaction_object &sto, std::string &sidechain_transaction) {
   sidechain_transaction = "";
   return true;
}

void sidechain_net_handler_peerplays::on_applied_block(const signed_block &b) {
   for (const auto &trx : b.transactions) {
      size_t operation_index = -1;
      for (auto op : trx.operations) {
         operation_index = operation_index + 1;
         if (op.which() == operation::tag<transfer_operation>::value) {
            transfer_operation transfer_op = op.get<transfer_operation>();
            if (transfer_op.to != plugin.database().get_global_properties().parameters.son_account()) {
               continue;
            }

            std::stringstream ss;
            ss << "peerplays"
               << "-" << trx.id().str() << "-" << operation_index;
            std::string sidechain_uid = ss.str();

            sidechain_event_data sed;
            sed.timestamp = database.head_block_time();
            sed.block_num = database.head_block_num();
            sed.sidechain = sidechain_type::peerplays;
            sed.sidechain_uid = sidechain_uid;
            sed.sidechain_transaction_id = trx.id().str();
            sed.sidechain_from = fc::to_string(transfer_op.from.space_id) + "." + fc::to_string(transfer_op.from.type_id) + "." + fc::to_string((uint64_t)transfer_op.from.instance);
            sed.sidechain_to = fc::to_string(transfer_op.to.space_id) + "." + fc::to_string(transfer_op.to.type_id) + "." + fc::to_string((uint64_t)transfer_op.to.instance);
            sed.sidechain_currency = fc::to_string(transfer_op.amount.asset_id.space_id) + "." + fc::to_string(transfer_op.amount.asset_id.type_id) + "." + fc::to_string((uint64_t)transfer_op.amount.asset_id.instance);
            sed.sidechain_amount = transfer_op.amount.amount;
            sed.peerplays_from = transfer_op.from;
            sed.peerplays_to = transfer_op.to;
            price asset_price = database.get<asset_object>(transfer_op.amount.asset_id).options.core_exchange_rate;
            sed.peerplays_asset = asset(transfer_op.amount.amount * asset_price.base.amount / asset_price.quote.amount);
            sidechain_event_data_received(sed);
         }
      }
   }
}

}} // namespace graphene::peerplays_sidechain
