#include <graphene/peerplays_sidechain/sidechain_net_handler.hpp>

#include <fc/log/logger.hpp>
#include <fc/smart_ref_fwd.hpp>
#include <graphene/chain/chain_property_object.hpp>
#include <graphene/chain/proposal_object.hpp>

namespace graphene { namespace peerplays_sidechain {

sidechain_net_handler::sidechain_net_handler(peerplays_sidechain_plugin &_plugin, const boost::program_options::variables_map &options) :
      plugin(_plugin),
      database(_plugin.database()) {
}

sidechain_net_handler::~sidechain_net_handler() {
}

sidechain_type sidechain_net_handler::get_sidechain() {
   return sidechain;
}

std::vector<std::string> sidechain_net_handler::get_sidechain_deposit_addresses() {
   std::vector<std::string> result;

   const auto &sidechain_addresses_idx = database.get_index_type<sidechain_address_index>();
   const auto &sidechain_addresses_by_sidechain_idx = sidechain_addresses_idx.indices().get<by_sidechain>();
   const auto &sidechain_addresses_by_sidechain_range = sidechain_addresses_by_sidechain_idx.equal_range(sidechain);
   std::for_each(sidechain_addresses_by_sidechain_range.first, sidechain_addresses_by_sidechain_range.second,
                 [&result](const sidechain_address_object &sao) {
                    result.push_back(sao.deposit_address);
                 });
   return result;
}

std::vector<std::string> sidechain_net_handler::get_sidechain_withdraw_addresses() {
   std::vector<std::string> result;

   const auto &sidechain_addresses_idx = database.get_index_type<sidechain_address_index>();
   const auto &sidechain_addresses_by_sidechain_idx = sidechain_addresses_idx.indices().get<by_sidechain>();
   const auto &sidechain_addresses_by_sidechain_range = sidechain_addresses_by_sidechain_idx.equal_range(sidechain);
   std::for_each(sidechain_addresses_by_sidechain_range.first, sidechain_addresses_by_sidechain_range.second,
                 [&result](const sidechain_address_object &sao) {
                    result.push_back(sao.withdraw_address);
                 });
   return result;
}

std::string sidechain_net_handler::get_private_key(std::string public_key) {
   auto private_key_itr = private_keys.find(public_key);
   if (private_key_itr != private_keys.end()) {
      return private_key_itr->second;
   }
   return std::string();
}

bool sidechain_net_handler::approve_proposal(const proposal_id_type &proposal_id, const son_id_type &son_id) {

   proposal_update_operation op;
   op.fee_paying_account = plugin.get_son_object(son_id).son_account;
   op.proposal = proposal_id;
   op.active_approvals_to_add = {plugin.get_son_object(son_id).son_account};

   signed_transaction tx = database.create_signed_transaction(plugin.get_private_key(son_id), op);
   try {
      database.push_transaction(tx, database::validation_steps::skip_block_size_check);
      if (plugin.app().p2p_node())
         plugin.app().p2p_node()->broadcast(net::trx_message(tx));
      return true;
   } catch (fc::exception e) {
      elog("Sending approval from ${son_id} for proposal ${proposal_id} failed with exception ${e}",
           ("son_id", son_id)("proposal_id", proposal_id)("e", e.what()));
      return false;
   }
}

void sidechain_net_handler::sidechain_event_data_received(const sidechain_event_data &sed) {
   ilog("sidechain_event_data:");
   ilog("  timestamp:                ${timestamp}", ("timestamp", sed.timestamp));
   ilog("  block_num:                ${block_num}", ("block_num", sed.block_num));
   ilog("  sidechain:                ${sidechain}", ("sidechain", sed.sidechain));
   ilog("  sidechain_uid:            ${uid}", ("uid", sed.sidechain_uid));
   ilog("  sidechain_transaction_id: ${transaction_id}", ("transaction_id", sed.sidechain_transaction_id));
   ilog("  sidechain_from:           ${from}", ("from", sed.sidechain_from));
   ilog("  sidechain_to:             ${to}", ("to", sed.sidechain_to));
   ilog("  sidechain_currency:       ${currency}", ("currency", sed.sidechain_currency));
   ilog("  sidechain_amount:         ${amount}", ("amount", sed.sidechain_amount));
   ilog("  peerplays_from:           ${peerplays_from}", ("peerplays_from", sed.peerplays_from));
   ilog("  peerplays_to:             ${peerplays_to}", ("peerplays_to", sed.peerplays_to));
   ilog("  peerplays_asset:          ${peerplays_asset}", ("peerplays_asset", sed.peerplays_asset));

   const chain::global_property_object &gpo = database.get_global_properties();

   asset_id_type btc_asset_id = database.get_global_properties().parameters.btc_asset();
   std::string btc_asset_id_str = fc::to_string(btc_asset_id.space_id) + "." +
                                  fc::to_string(btc_asset_id.type_id) + "." +
                                  fc::to_string((uint64_t)btc_asset_id.instance);

   // Deposit request
   if ((sed.peerplays_to == gpo.parameters.son_account()) && (sed.sidechain_currency.compare(btc_asset_id_str) != 0)) {

      for (son_id_type son_id : plugin.get_sons()) {
         if (plugin.is_active_son(son_id)) {

            son_wallet_deposit_create_operation op;
            op.payer = plugin.get_son_object(son_id).son_account;
            op.son_id = son_id;
            op.timestamp = sed.timestamp;
            op.block_num = sed.block_num;
            op.sidechain = sed.sidechain;
            op.sidechain_uid = sed.sidechain_uid;
            op.sidechain_transaction_id = sed.sidechain_transaction_id;
            op.sidechain_from = sed.sidechain_from;
            op.sidechain_to = sed.sidechain_to;
            op.sidechain_currency = sed.sidechain_currency;
            op.sidechain_amount = sed.sidechain_amount;
            op.peerplays_from = sed.peerplays_from;
            op.peerplays_to = sed.peerplays_to;
            op.peerplays_asset = sed.peerplays_asset;

            signed_transaction trx = database.create_signed_transaction(plugin.get_private_key(son_id), op);
            try {
               database.push_transaction(trx, database::validation_steps::skip_block_size_check);
               if (plugin.app().p2p_node())
                  plugin.app().p2p_node()->broadcast(net::trx_message(trx));
            } catch (fc::exception e) {
               elog("Sending son wallet deposit create operation by ${son} failed with exception ${e}", ("son", son_id)("e", e.what()));
            }
         }
      }
      return;
   }

   // Withdrawal request
   if ((sed.peerplays_to == gpo.parameters.son_account()) && (sed.sidechain_currency.compare(btc_asset_id_str) == 0)) {
      // BTC Payout only (for now)
      const auto &sidechain_addresses_idx = database.get_index_type<sidechain_address_index>().indices().get<by_account_and_sidechain>();
      const auto &addr_itr = sidechain_addresses_idx.find(std::make_tuple(sed.peerplays_from, sidechain_type::bitcoin));
      if (addr_itr == sidechain_addresses_idx.end())
         return;

      for (son_id_type son_id : plugin.get_sons()) {
         if (plugin.is_active_son(son_id)) {

            son_wallet_withdraw_create_operation op;
            op.payer = plugin.get_son_object(son_id).son_account;
            op.son_id = son_id;
            op.timestamp = sed.timestamp;
            op.block_num = sed.block_num;
            op.sidechain = sed.sidechain;
            op.peerplays_uid = sed.sidechain_uid;
            op.peerplays_transaction_id = sed.sidechain_transaction_id;
            op.peerplays_from = sed.peerplays_from;
            op.peerplays_asset = sed.peerplays_asset;
            // BTC payout only (for now)
            op.withdraw_sidechain = sidechain_type::bitcoin;
            op.withdraw_address = addr_itr->withdraw_address;
            op.withdraw_currency = "BTC";
            price btc_price = database.get<asset_object>(database.get_global_properties().parameters.btc_asset()).options.core_exchange_rate;
            op.withdraw_amount = sed.peerplays_asset.amount * btc_price.quote.amount / btc_price.base.amount;

            signed_transaction trx = database.create_signed_transaction(plugin.get_private_key(son_id), op);
            try {
               database.push_transaction(trx, database::validation_steps::skip_block_size_check);
               if (plugin.app().p2p_node())
                  plugin.app().p2p_node()->broadcast(net::trx_message(trx));
            } catch (fc::exception e) {
               elog("Sending son wallet withdraw create operation by ${son} failed with exception ${e}", ("son", son_id)("e", e.what()));
            }
         }
      }
      return;
   }

   FC_ASSERT(false, "Invalid sidechain event");
}

void sidechain_net_handler::process_proposals() {
   const auto &idx = database.get_index_type<proposal_index>().indices().get<by_id>();
   vector<proposal_id_type> proposals;
   for (const auto &proposal : idx) {
      proposals.push_back(proposal.id);
   }

   for (const auto proposal_id : proposals) {
      const auto &idx = database.get_index_type<proposal_index>().indices().get<by_id>();
      const auto po = idx.find(proposal_id);
      if (po != idx.end()) {

         ilog("Proposal to process: ${po}, SON id ${son_id}", ("po", (*po).id)("son_id", plugin.get_current_son_id()));

         if (po->available_active_approvals.find(plugin.get_current_son_object().son_account) != po->available_active_approvals.end()) {
            continue;
         }

         bool should_process = false;

         int32_t op_idx_0 = -1;
         chain::operation op_obj_idx_0;

         if (po->proposed_transaction.operations.size() >= 1) {
            op_idx_0 = po->proposed_transaction.operations[0].which();
            op_obj_idx_0 = po->proposed_transaction.operations[0];
         }

         switch (op_idx_0) {
         case chain::operation::tag<chain::son_wallet_update_operation>::value: {
            should_process = (op_obj_idx_0.get<son_wallet_update_operation>().sidechain == sidechain);
            break;
         }

         case chain::operation::tag<chain::son_wallet_deposit_process_operation>::value: {
            son_wallet_deposit_id_type swdo_id = op_obj_idx_0.get<son_wallet_deposit_process_operation>().son_wallet_deposit_id;
            const auto &idx = database.get_index_type<son_wallet_deposit_index>().indices().get<by_id>();
            const auto swdo = idx.find(swdo_id);
            if (swdo != idx.end()) {
               should_process = (swdo->sidechain == sidechain);
            }
            break;
         }

         case chain::operation::tag<chain::son_wallet_withdraw_process_operation>::value: {
            son_wallet_withdraw_id_type swwo_id = op_obj_idx_0.get<son_wallet_withdraw_process_operation>().son_wallet_withdraw_id;
            const auto &idx = database.get_index_type<son_wallet_withdraw_index>().indices().get<by_id>();
            const auto swwo = idx.find(swwo_id);
            if (swwo != idx.end()) {
               should_process = (swwo->sidechain == sidechain);
            }
            break;
         }

         case chain::operation::tag<chain::sidechain_transaction_create_operation>::value: {
            sidechain_type sc = op_obj_idx_0.get<sidechain_transaction_create_operation>().sidechain;
            should_process = (sc == sidechain);
            break;
         }

         default:
            should_process = false;
            elog("==================================================");
            elog("Proposal not processed ${po}", ("po", *po));
            elog("==================================================");
         }

         if (should_process) {
            ilog("Proposal ${po} will be processed by sidechain handler ${sidechain}", ("po", (*po).id)("sidechain", sidechain));
            bool should_approve = process_proposal(*po);
            if (should_approve) {
               ilog("Proposal ${po} will be approved", ("po", *po));
               approve_proposal(po->id, plugin.get_current_son_id());
            } else {
               ilog("Proposal ${po} is not approved", ("po", (*po).id));
            }
         } else {
            ilog("Proposal ${po} will not be processed by sidechain handler ${sidechain}", ("po", (*po).id)("sidechain", sidechain));
         }
      }
   }
}

void sidechain_net_handler::process_active_sons_change() {
   process_primary_wallet();
}

void sidechain_net_handler::process_deposits() {
   if (database.get_global_properties().active_sons.size() < database.get_chain_properties().immutable_parameters.min_son_count) {
      return;
   }

   const auto &idx = database.get_index_type<son_wallet_deposit_index>().indices().get<by_sidechain_and_confirmed_and_processed>();
   const auto &idx_range = idx.equal_range(std::make_tuple(sidechain, true, false));

   std::for_each(idx_range.first, idx_range.second, [&](const son_wallet_deposit_object &swdo) {
      ilog("Deposit to process: ${swdo}", ("swdo", swdo));

      bool process_deposit_result = process_deposit(swdo);

      if (!process_deposit_result) {
         wlog("Deposit not processed: ${swdo}", ("swdo", swdo));
         return;
      }

      const chain::global_property_object &gpo = database.get_global_properties();

      son_wallet_deposit_process_operation swdp_op;
      swdp_op.payer = gpo.parameters.son_account();
      swdp_op.son_wallet_deposit_id = swdo.id;

      asset_issue_operation ai_op;
      ai_op.fee = asset(2001000);
      ai_op.issuer = gpo.parameters.son_account();
      price btc_price = database.get<asset_object>(database.get_global_properties().parameters.btc_asset()).options.core_exchange_rate;
      ai_op.asset_to_issue = asset(swdo.peerplays_asset.amount * btc_price.quote.amount / btc_price.base.amount, database.get_global_properties().parameters.btc_asset());
      ai_op.issue_to_account = swdo.peerplays_from;

      proposal_create_operation proposal_op;
      proposal_op.fee_paying_account = plugin.get_current_son_object().son_account;
      proposal_op.proposed_ops.emplace_back(swdp_op);
      proposal_op.proposed_ops.emplace_back(ai_op);
      uint32_t lifetime = (gpo.parameters.block_interval * gpo.active_witnesses.size()) * 3;
      proposal_op.expiration_time = time_point_sec(database.head_block_time().sec_since_epoch() + lifetime);

      signed_transaction trx = database.create_signed_transaction(plugin.get_private_key(plugin.get_current_son_id()), proposal_op);
      trx.validate();
      try {
         database.push_transaction(trx, database::validation_steps::skip_block_size_check);
         if (plugin.app().p2p_node())
            plugin.app().p2p_node()->broadcast(net::trx_message(trx));
      } catch (fc::exception e) {
         elog("Sending proposal for son wallet deposit process operation failed with exception ${e}", ("e", e.what()));
      }
   });
}

void sidechain_net_handler::process_withdrawals() {
   if (database.get_global_properties().active_sons.size() < database.get_chain_properties().immutable_parameters.min_son_count) {
      return;
   }

   const auto &idx = database.get_index_type<son_wallet_withdraw_index>().indices().get<by_withdraw_sidechain_and_confirmed_and_processed>();
   const auto &idx_range = idx.equal_range(std::make_tuple(sidechain, true, false));

   std::for_each(idx_range.first, idx_range.second, [&](const son_wallet_withdraw_object &swwo) {
      ilog("Withdraw to process: ${swwo}", ("swwo", swwo));

      bool process_withdrawal_result = process_withdrawal(swwo);

      if (!process_withdrawal_result) {
         wlog("Withdraw not processed: ${swwo}", ("swwo", swwo));
         return;
      }

      const chain::global_property_object &gpo = database.get_global_properties();

      son_wallet_withdraw_process_operation swwp_op;
      swwp_op.payer = gpo.parameters.son_account();
      swwp_op.son_wallet_withdraw_id = swwo.id;

      asset_reserve_operation ar_op;
      ar_op.fee = asset(2001000);
      ar_op.payer = gpo.parameters.son_account();
      ar_op.amount_to_reserve = asset(swwo.withdraw_amount, database.get_global_properties().parameters.btc_asset());

      proposal_create_operation proposal_op;
      proposal_op.fee_paying_account = plugin.get_current_son_object().son_account;
      proposal_op.proposed_ops.emplace_back(swwp_op);
      proposal_op.proposed_ops.emplace_back(ar_op);
      uint32_t lifetime = (gpo.parameters.block_interval * gpo.active_witnesses.size()) * 3;
      proposal_op.expiration_time = time_point_sec(database.head_block_time().sec_since_epoch() + lifetime);

      signed_transaction trx = database.create_signed_transaction(plugin.get_private_key(plugin.get_current_son_id()), proposal_op);
      trx.validate();
      try {
         database.push_transaction(trx, database::validation_steps::skip_block_size_check);
         if (plugin.app().p2p_node())
            plugin.app().p2p_node()->broadcast(net::trx_message(trx));
      } catch (fc::exception e) {
         elog("Sending proposal for son wallet withdraw process operation failed with exception ${e}", ("e", e.what()));
      }
   });
}

void sidechain_net_handler::process_sidechain_transactions() {
   const auto &idx = database.get_index_type<sidechain_transaction_index>().indices().get<by_sidechain_and_complete>();
   const auto &idx_range = idx.equal_range(std::make_tuple(sidechain, false));

   std::for_each(idx_range.first, idx_range.second, [&](const sidechain_transaction_object &sto) {
      ilog("Sidechain transaction to process: ${sto}", ("sto", sto.id));

      bool complete = false;
      std::string processed_sidechain_tx = process_sidechain_transaction(sto);

      if (processed_sidechain_tx.empty()) {
         wlog("Sidechain transaction not processed: ${sto}", ("sto", sto.id));
         return;
      }

      sidechain_transaction_sign_operation sts_op;
      sts_op.payer = plugin.get_current_son_object().son_account;
      sts_op.sidechain_transaction_id = sto.id;
      sts_op.signature = processed_sidechain_tx;

      signed_transaction trx = database.create_signed_transaction(plugin.get_private_key(plugin.get_current_son_id()), sts_op);
      trx.validate();
      try {
         database.push_transaction(trx, database::validation_steps::skip_block_size_check);
         if (plugin.app().p2p_node())
            plugin.app().p2p_node()->broadcast(net::trx_message(trx));
      } catch (fc::exception e) {
         elog("Sending proposal for sidechain transaction sign operation failed with exception ${e}", ("e", e.what()));
      }
   });
}

void sidechain_net_handler::send_sidechain_transactions() {
   const auto &idx = database.get_index_type<sidechain_transaction_index>().indices().get<by_sidechain_and_complete_and_sent>();
   const auto &idx_range = idx.equal_range(std::make_tuple(sidechain, true, false));

   std::for_each(idx_range.first, idx_range.second, [&](const sidechain_transaction_object &sto) {
      ilog("Sidechain transaction to send: ${sto}", ("sto", sto.id));

      std::string sidechain_transaction = send_sidechain_transaction(sto);

      if (sidechain_transaction.empty()) {
         wlog("Sidechain transaction not sent: ${sto}", ("sto", sto.id));
         return;
      }

      sidechain_transaction_send_operation sts_op;
      sts_op.payer = plugin.get_current_son_object().son_account;
      sts_op.sidechain_transaction_id = sto.id;
      sts_op.sidechain_transaction = sidechain_transaction;

      signed_transaction trx = database.create_signed_transaction(plugin.get_private_key(plugin.get_current_son_id()), sts_op);
      trx.validate();
      try {
         database.push_transaction(trx, database::validation_steps::skip_block_size_check);
         if (plugin.app().p2p_node())
            plugin.app().p2p_node()->broadcast(net::trx_message(trx));
      } catch (fc::exception e) {
         elog("Sending proposal for sidechain transaction send operation failed with exception ${e}", ("e", e.what()));
      }
   });
}

bool sidechain_net_handler::process_proposal(const proposal_object &po) {
   FC_ASSERT(false, "process_proposal not implemented");
}

void sidechain_net_handler::process_primary_wallet() {
   FC_ASSERT(false, "process_primary_wallet not implemented");
}

bool sidechain_net_handler::process_deposit(const son_wallet_deposit_object &swdo) {
   FC_ASSERT(false, "process_deposit not implemented");
}

bool sidechain_net_handler::process_withdrawal(const son_wallet_withdraw_object &swwo) {
   FC_ASSERT(false, "process_withdrawal not implemented");
}

std::string sidechain_net_handler::process_sidechain_transaction(const sidechain_transaction_object &sto) {
   FC_ASSERT(false, "process_sidechain_transaction not implemented");
}

std::string sidechain_net_handler::send_sidechain_transaction(const sidechain_transaction_object &sto) {
   FC_ASSERT(false, "send_sidechain_transaction not implemented");
}

}} // namespace graphene::peerplays_sidechain
