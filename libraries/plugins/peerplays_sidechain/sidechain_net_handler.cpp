#include <graphene/peerplays_sidechain/sidechain_net_handler.hpp>

#include <fc/log/logger.hpp>
#include <fc/smart_ref_fwd.hpp>

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

void sidechain_net_handler::sidechain_event_data_received(const sidechain_event_data &sed) {
   ilog("sidechain_event_data:");
   ilog("  timestamp:                ${timestamp}", ("timestamp", sed.timestamp));
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

   // Deposit request
   if ((sed.peerplays_to == gpo.parameters.son_account()) && (sed.sidechain_currency.compare("1.3.0") != 0)) {
      son_wallet_deposit_create_operation op;
      op.payer = gpo.parameters.son_account();
      //op.son_id = ; // to be filled for each son
      op.timestamp = sed.timestamp;
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

      for (son_id_type son_id : plugin.get_sons()) {
         if (plugin.is_active_son(son_id)) {

            op.son_id = son_id;

            proposal_create_operation proposal_op;
            proposal_op.fee_paying_account = plugin.get_son_object(son_id).son_account;
            proposal_op.proposed_ops.emplace_back(op);
            uint32_t lifetime = (gpo.parameters.block_interval * gpo.active_witnesses.size()) * 3;
            proposal_op.expiration_time = time_point_sec(database.head_block_time().sec_since_epoch() + lifetime);

            signed_transaction trx = database.create_signed_transaction(plugin.get_private_key(son_id), proposal_op);
            try {
               database.push_transaction(trx, database::validation_steps::skip_block_size_check);
               if (plugin.app().p2p_node())
                  plugin.app().p2p_node()->broadcast(net::trx_message(trx));
            } catch (fc::exception e) {
               elog("Sending proposal for son wallet deposit create operation by ${son} failed with exception ${e}", ("son", son_id)("e", e.what()));
            }
         }
      }
      return;
   }

   // Withdrawal request
   if ((sed.peerplays_to == gpo.parameters.son_account()) && (sed.sidechain_currency.compare("1.3.0") == 0)) {
      // BTC Payout only (for now)
      const auto &sidechain_addresses_idx = database.get_index_type<sidechain_address_index>().indices().get<by_account_and_sidechain>();
      const auto &addr_itr = sidechain_addresses_idx.find(std::make_tuple(sed.peerplays_from, sidechain_type::bitcoin));
      if (addr_itr == sidechain_addresses_idx.end())
         return;

      son_wallet_withdraw_create_operation op;
      op.payer = gpo.parameters.son_account();
      //op.son_id = ; // to be filled for each son
      op.timestamp = sed.timestamp;
      op.sidechain = sed.sidechain;
      op.peerplays_uid = sed.sidechain_uid;
      op.peerplays_transaction_id = sed.sidechain_transaction_id;
      op.peerplays_from = sed.peerplays_from;
      op.peerplays_asset = sed.peerplays_asset;
      op.withdraw_sidechain = sidechain_type::bitcoin;        // BTC payout only (for now)
      op.withdraw_address = addr_itr->withdraw_address;       // BTC payout only (for now)
      op.withdraw_currency = "BTC";                           // BTC payout only (for now)
      op.withdraw_amount = sed.peerplays_asset.amount * 1000; // BTC payout only (for now)

      for (son_id_type son_id : plugin.get_sons()) {
         if (plugin.is_active_son(son_id)) {

            op.son_id = son_id;

            proposal_create_operation proposal_op;
            proposal_op.fee_paying_account = plugin.get_son_object(son_id).son_account;
            proposal_op.proposed_ops.emplace_back(op);
            uint32_t lifetime = (gpo.parameters.block_interval * gpo.active_witnesses.size()) * 3;
            proposal_op.expiration_time = time_point_sec(database.head_block_time().sec_since_epoch() + lifetime);

            signed_transaction trx = database.create_signed_transaction(plugin.get_private_key(son_id), proposal_op);
            try {
               database.push_transaction(trx, database::validation_steps::skip_block_size_check);
               if (plugin.app().p2p_node())
                  plugin.app().p2p_node()->broadcast(net::trx_message(trx));
            } catch (fc::exception e) {
               elog("Sending proposal for son wallet withdraw create operation by ${son} failed with exception ${e}", ("son", son_id)("e", e.what()));
            }
         }
      }
      return;
   }

   FC_ASSERT(false, "Invalid sidechain event");
}

void sidechain_net_handler::process_deposits() {
   const auto &idx = database.get_index_type<son_wallet_deposit_index>().indices().get<by_sidechain_and_processed>();
   const auto &idx_range = idx.equal_range(std::make_tuple(sidechain, false));

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

      transfer_operation t_op;
      t_op.fee = asset(2000000);
      t_op.from = swdo.peerplays_to; // gpo.parameters.son_account()
      t_op.to = swdo.peerplays_from;
      t_op.amount = swdo.peerplays_asset;

      proposal_create_operation proposal_op;
      proposal_op.fee_paying_account = plugin.get_current_son_object().son_account;
      proposal_op.proposed_ops.emplace_back(swdp_op);
      proposal_op.proposed_ops.emplace_back(t_op);
      uint32_t lifetime = (gpo.parameters.block_interval * gpo.active_witnesses.size()) * 3;
      proposal_op.expiration_time = time_point_sec(database.head_block_time().sec_since_epoch() + lifetime);

      signed_transaction trx = database.create_signed_transaction(plugin.get_private_key(plugin.get_current_son_id()), proposal_op);
      trx.validate();
      try {
         database.push_transaction(trx, database::validation_steps::skip_block_size_check);
         if (plugin.app().p2p_node())
            plugin.app().p2p_node()->broadcast(net::trx_message(trx));
      } catch (fc::exception e) {
         elog("Sending proposal for deposit sidechain transaction create operation failed with exception ${e}", ("e", e.what()));
      }
   });
}

void sidechain_net_handler::process_withdrawals() {
   const auto &idx = database.get_index_type<son_wallet_withdraw_index>().indices().get<by_withdraw_sidechain_and_processed>();
   const auto &idx_range = idx.equal_range(std::make_tuple(sidechain, false));

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

      proposal_create_operation proposal_op;
      proposal_op.fee_paying_account = plugin.get_current_son_object().son_account;
      proposal_op.proposed_ops.emplace_back(swwp_op);
      uint32_t lifetime = (gpo.parameters.block_interval * gpo.active_witnesses.size()) * 3;
      proposal_op.expiration_time = time_point_sec(database.head_block_time().sec_since_epoch() + lifetime);

      signed_transaction trx = database.create_signed_transaction(plugin.get_private_key(plugin.get_current_son_id()), proposal_op);
      trx.validate();
      try {
         database.push_transaction(trx, database::validation_steps::skip_block_size_check);
         if (plugin.app().p2p_node())
            plugin.app().p2p_node()->broadcast(net::trx_message(trx));
      } catch (fc::exception e) {
         elog("Sending proposal for withdraw sidechain transaction create operation failed with exception ${e}", ("e", e.what()));
      }
   });
}

void sidechain_net_handler::process_sidechain_transactions() {
   const auto &idx = database.get_index_type<sidechain_transaction_index>().indices().get<by_sidechain_and_complete>();
   const auto &idx_range = idx.equal_range(std::make_tuple(sidechain, false));

   std::for_each(idx_range.first, idx_range.second, [&](const sidechain_transaction_object &sto) {
      ilog("Sidechain transaction to process: ${sto}", ("sto", sto));

      bool complete = false;
      std::string processed_sidechain_tx = process_sidechain_transaction(sto, complete);

      if (processed_sidechain_tx.empty()) {
         wlog("Sidechain transaction not processed: ${sto}", ("sto", sto));
         return;
      }

      sidechain_transaction_sign_operation sts_op;
      sts_op.payer = plugin.get_current_son_object().son_account;
      sts_op.sidechain_transaction_id = sto.id;
      sts_op.signature = processed_sidechain_tx;
      sts_op.complete = complete;

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
      ilog("Sidechain transaction to send: ${sto}", ("sto", sto));

      std::string sidechain_transaction = "";
      bool sent = send_sidechain_transaction(sto, sidechain_transaction);

      if (!sent) {
         wlog("Sidechain transaction not sent: ${sto}", ("sto", sto));
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

void sidechain_net_handler::recreate_primary_wallet() {
   FC_ASSERT(false, "recreate_primary_wallet not implemented");
}

bool sidechain_net_handler::process_deposit(const son_wallet_deposit_object &swdo) {
   FC_ASSERT(false, "process_deposit not implemented");
}

bool sidechain_net_handler::process_withdrawal(const son_wallet_withdraw_object &swwo) {
   FC_ASSERT(false, "process_withdrawal not implemented");
}

std::string sidechain_net_handler::process_sidechain_transaction(const sidechain_transaction_object &sto, bool &complete) {
   FC_ASSERT(false, "process_sidechain_transaction not implemented");
}

bool sidechain_net_handler::send_sidechain_transaction(const sidechain_transaction_object &sto, std::string &sidechain_transaction) {
   FC_ASSERT(false, "send_sidechain_transaction not implemented");
}

}} // namespace graphene::peerplays_sidechain
