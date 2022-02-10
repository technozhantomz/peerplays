#include <graphene/peerplays_sidechain/sidechain_net_handler.hpp>

#include <boost/algorithm/string.hpp>

#include <fc/log/logger.hpp>
#include <graphene/chain/chain_property_object.hpp>
#include <graphene/chain/proposal_object.hpp>
#include <graphene/peerplays_sidechain/common/utils.hpp>

namespace graphene { namespace peerplays_sidechain {

sidechain_net_handler::sidechain_net_handler(peerplays_sidechain_plugin &_plugin, const boost::program_options::variables_map &options) :
      plugin(_plugin),
      database(_plugin.database()) {

   database.applied_block.connect([&](const signed_block &b) {
      on_applied_block(b);
   });
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

bool sidechain_net_handler::proposal_exists(int32_t operation_tag, const object_id_type &object_id, boost::optional<chain::operation &> proposal_op) {

   bool result = false;

   const auto &idx = database.get_index_type<proposal_index>().indices().get<by_id>();
   vector<proposal_id_type> proposals;
   for (const auto &proposal : idx) {
      proposals.push_back(proposal.id);
   }

   for (const auto proposal_id : proposals) {
      const auto &idx = database.get_index_type<proposal_index>().indices().get<by_id>();
      const auto po = idx.find(proposal_id);
      if (po != idx.end()) {

         int32_t op_idx_0 = -1;
         chain::operation op_obj_idx_0;

         if (po->proposed_transaction.operations.size() >= 1) {
            op_idx_0 = po->proposed_transaction.operations[0].which();
            op_obj_idx_0 = po->proposed_transaction.operations[0];
         }

         switch (op_idx_0) {
         case chain::operation::tag<chain::son_wallet_update_operation>::value: {
            result = (op_obj_idx_0.get<son_wallet_update_operation>().son_wallet_id == object_id);
            break;
         }

         case chain::operation::tag<chain::son_wallet_deposit_process_operation>::value: {
            result = (op_obj_idx_0.get<son_wallet_deposit_process_operation>().son_wallet_deposit_id == object_id);
            break;
         }

         case chain::operation::tag<chain::son_wallet_withdraw_process_operation>::value: {
            result = (op_obj_idx_0.get<son_wallet_withdraw_process_operation>().son_wallet_withdraw_id == object_id);
            break;
         }

         case chain::operation::tag<chain::sidechain_transaction_sign_operation>::value: {
            if (proposal_op) {
               chain::operation proposal_op_obj_0 = proposal_op.get();
               result = ((proposal_op_obj_0.get<sidechain_transaction_sign_operation>().sidechain_transaction_id == op_obj_idx_0.get<sidechain_transaction_sign_operation>().sidechain_transaction_id) &&
                         (proposal_op_obj_0.get<sidechain_transaction_sign_operation>().signer == op_obj_idx_0.get<sidechain_transaction_sign_operation>().signer) &&
                         (proposal_op_obj_0.get<sidechain_transaction_sign_operation>().signature == op_obj_idx_0.get<sidechain_transaction_sign_operation>().signature));
            }
            break;
         }

         default:
            return false;
         }
      }

      if (result) {
         break;
      }
   }
   return result;
}

bool sidechain_net_handler::signer_expected(const sidechain_transaction_object &sto, son_id_type signer) {
   bool expected = false;
   for (auto signature : sto.signatures) {
      if (signature.first == signer) {
         expected = signature.second.empty();
      }
   }
   return expected;
}

bool sidechain_net_handler::approve_proposal(const proposal_id_type &proposal_id, const son_id_type &son_id) {

   proposal_update_operation op;
   op.fee_paying_account = plugin.get_son_object(son_id).son_account;
   op.proposal = proposal_id;
   op.active_approvals_to_add = {plugin.get_son_object(son_id).son_account};

   signed_transaction trx = database.create_signed_transaction(plugin.get_private_key(son_id), op);
   try {
      trx.validate();
      database.push_transaction(trx, database::validation_steps::skip_block_size_check);
      if (plugin.app().p2p_node())
         plugin.app().p2p_node()->broadcast(net::trx_message(trx));
      return true;
   } catch (fc::exception &e) {
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

   bool enable_peerplays_asset_deposits = false;
#ifdef ENABLE_PEERPLAYS_ASSET_DEPOSITS
   //enable_peerplays_asset_deposits = (sed.sidechain == sidechain_type::peerplays) &&
   //                                  (sed.sidechain_currency.compare("BTC") != 0) &&
   //                                  (sed.sidechain_currency.compare("HBD") != 0) &&
   //                                  (sed.sidechain_currency.compare("HIVE") != 0);
#endif

   bool deposit_condition = (sed.peerplays_to == gpo.parameters.son_account()) &&
                            (((sed.sidechain == sidechain_type::bitcoin) && (sed.sidechain_currency.compare("BTC") == 0)) ||
                             ((sed.sidechain == sidechain_type::hive) && (sed.sidechain_currency.compare("HBD") == 0)) ||
                             ((sed.sidechain == sidechain_type::hive) && (sed.sidechain_currency.compare("HIVE") == 0)) ||
                             enable_peerplays_asset_deposits);

   bool withdraw_condition = (sed.peerplays_to == gpo.parameters.son_account()) && (sed.sidechain == sidechain_type::peerplays) &&
                             ((sed.sidechain_currency == object_id_to_string(gpo.parameters.btc_asset())) ||
                              (sed.sidechain_currency == object_id_to_string(gpo.parameters.hbd_asset())) ||
                              (sed.sidechain_currency == object_id_to_string(gpo.parameters.hive_asset())));

   // Deposit request
   if (deposit_condition) {

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
               trx.validate();
               database.push_transaction(trx, database::validation_steps::skip_block_size_check);
               if (plugin.app().p2p_node())
                  plugin.app().p2p_node()->broadcast(net::trx_message(trx));
            } catch (fc::exception &e) {
               elog("Sending son wallet deposit create operation by ${son} failed with exception ${e}", ("son", son_id)("e", e.what()));
            }
         }
      }
      return;
   }

   // Withdrawal request
   if (withdraw_condition) {
      std::string withdraw_address = "";
      const auto &sidechain_addresses_idx = database.get_index_type<sidechain_address_index>().indices().get<by_account_and_sidechain_and_expires>();
      const auto &addr_itr = sidechain_addresses_idx.find(std::make_tuple(sed.peerplays_from, sidechain, time_point_sec::maximum()));
      if (addr_itr != sidechain_addresses_idx.end()) {
         withdraw_address = addr_itr->withdraw_address;
      } else {
         withdraw_address = sed.sidechain_from;
      }

      std::string withdraw_currency = "";
      price withdraw_currency_price = {};
      if (sed.sidechain_currency == object_id_to_string(gpo.parameters.btc_asset())) {
         withdraw_currency = "BTC";
         withdraw_currency_price = database.get<asset_object>(database.get_global_properties().parameters.btc_asset()).options.core_exchange_rate;
      }
      if (sed.sidechain_currency == object_id_to_string(gpo.parameters.hbd_asset())) {
         withdraw_currency = "HBD";
         withdraw_currency_price = database.get<asset_object>(database.get_global_properties().parameters.hbd_asset()).options.core_exchange_rate;
      }
      if (sed.sidechain_currency == object_id_to_string(gpo.parameters.hive_asset())) {
         withdraw_currency = "HIVE";
         withdraw_currency_price = database.get<asset_object>(database.get_global_properties().parameters.hive_asset()).options.core_exchange_rate;
      }
      if (withdraw_currency.empty()) {
         return;
      }

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
            op.withdraw_sidechain = sidechain;
            op.withdraw_address = withdraw_address;
            op.withdraw_currency = withdraw_currency;
            op.withdraw_amount = sed.peerplays_asset.amount * withdraw_currency_price.quote.amount / withdraw_currency_price.base.amount;

            signed_transaction trx = database.create_signed_transaction(plugin.get_private_key(son_id), op);
            try {
               trx.validate();
               database.push_transaction(trx, database::validation_steps::skip_block_size_check);
               if (plugin.app().p2p_node())
                  plugin.app().p2p_node()->broadcast(net::trx_message(trx));
            } catch (fc::exception &e) {
               elog("Sending son wallet withdraw create operation by ${son} failed with exception ${e}", ("son", son_id)("e", e.what()));
            }
         }
      }
      return;
   }
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

         if (po->available_active_approvals.find(plugin.get_current_son_object().son_account) != po->available_active_approvals.end()) {
            continue;
         }

         bool should_process = false;

         int32_t op_idx_0 = -1;
         chain::operation op_obj_idx_0;
         object_id_type object_id;

         if (po->proposed_transaction.operations.size() >= 1) {
            op_idx_0 = po->proposed_transaction.operations[0].which();
            op_obj_idx_0 = po->proposed_transaction.operations[0];
         }

         int32_t op_idx_1 = -1;
         chain::operation op_obj_idx_1;
         (void)op_idx_1;

         if (po->proposed_transaction.operations.size() >= 2) {
            op_idx_1 = po->proposed_transaction.operations[1].which();
            op_obj_idx_1 = po->proposed_transaction.operations[1];
         }

         switch (op_idx_0) {
         case chain::operation::tag<chain::son_wallet_update_operation>::value: {
            should_process = (op_obj_idx_0.get<son_wallet_update_operation>().sidechain == sidechain);
            object_id = op_obj_idx_0.get<son_wallet_update_operation>().son_wallet_id;
            break;
         }

         case chain::operation::tag<chain::son_wallet_deposit_process_operation>::value: {
            son_wallet_deposit_id_type swdo_id = op_obj_idx_0.get<son_wallet_deposit_process_operation>().son_wallet_deposit_id;
            object_id = swdo_id;
            const auto &idx = database.get_index_type<son_wallet_deposit_index>().indices().get<by_id>();
            const auto swdo = idx.find(swdo_id);
            if (swdo != idx.end()) {
               should_process = (swdo->sidechain == sidechain);
            }
            break;
         }

         case chain::operation::tag<chain::son_wallet_withdraw_process_operation>::value: {
            son_wallet_withdraw_id_type swwo_id = op_obj_idx_0.get<son_wallet_withdraw_process_operation>().son_wallet_withdraw_id;
            object_id = swwo_id;
            const auto &idx = database.get_index_type<son_wallet_withdraw_index>().indices().get<by_id>();
            const auto swwo = idx.find(swwo_id);
            if (swwo != idx.end()) {
               should_process = (swwo->withdraw_sidechain == sidechain);
            }
            break;
         }

         case chain::operation::tag<chain::sidechain_transaction_sign_operation>::value: {
            sidechain_transaction_id_type st_id = op_obj_idx_0.get<sidechain_transaction_sign_operation>().sidechain_transaction_id;
            son_id_type signer = op_obj_idx_0.get<sidechain_transaction_sign_operation>().signer;
            const auto &idx = database.get_index_type<sidechain_transaction_index>().indices().get<by_id>();
            const auto sto = idx.find(st_id);
            if (sto != idx.end()) {
               should_process = ((sto->sidechain == sidechain) && (sto->status == sidechain_transaction_status::valid) && signer_expected(*sto, signer));
               object_id = sto->object_id;
            }
            break;
         }

         case chain::operation::tag<chain::sidechain_transaction_settle_operation>::value: {
            sidechain_transaction_id_type st_id = op_obj_idx_0.get<sidechain_transaction_settle_operation>().sidechain_transaction_id;
            const auto &idx = database.get_index_type<sidechain_transaction_index>().indices().get<by_id>();
            const auto sto = idx.find(st_id);
            if (sto != idx.end()) {
               should_process = (sto->sidechain == sidechain);
               object_id = sto->object_id;
            }
            break;
         }

         default:
            should_process = false;
            elog("==================================================");
            elog("Proposal not processed ${po}", ("po", *po));
            elog("==================================================");
         }

         if (should_process && (op_idx_0 == chain::operation::tag<chain::sidechain_transaction_sign_operation>::value || plugin.can_son_participate(op_idx_0, object_id))) {
            bool should_approve = process_proposal(*po);
            if (should_approve) {
               if (approve_proposal(po->id, plugin.get_current_son_id())) {
                  if (op_idx_0 != chain::operation::tag<chain::sidechain_transaction_sign_operation>::value) {
                     plugin.log_son_proposal_retry(op_idx_0, object_id);
                  }
               }
            }
         }
      }
   }
}

void sidechain_net_handler::process_active_sons_change() {
   process_primary_wallet();
}

void sidechain_net_handler::create_deposit_addresses() {
   if (database.get_global_properties().active_sons.size() < database.get_chain_properties().immutable_parameters.min_son_count) {
      return;
   }
   process_sidechain_addresses();
}

void sidechain_net_handler::process_deposits() {
   if (database.get_global_properties().active_sons.size() < database.get_chain_properties().immutable_parameters.min_son_count) {
      return;
   }

   const auto &idx = database.get_index_type<son_wallet_deposit_index>().indices().get<by_sidechain_and_confirmed_and_processed>();
   const auto &idx_range = idx.equal_range(std::make_tuple(sidechain, true, false));

   std::for_each(idx_range.first, idx_range.second, [&](const son_wallet_deposit_object &swdo) {
      if (swdo.id == object_id_type(0, 0, 0) || !plugin.can_son_participate(chain::operation::tag<chain::son_wallet_deposit_process_operation>::value, swdo.id)) {
         return;
      }
      //Ignore the deposits which are not valid anymore, considered refunds.
      const auto &sidechain_addresses_idx = database.get_index_type<sidechain_address_index>().indices().get<by_sidechain_and_deposit_address_and_expires>();
      const auto &addr_itr = sidechain_addresses_idx.find(std::make_tuple(sidechain, swdo.sidechain_from, time_point_sec::maximum()));
      if (addr_itr == sidechain_addresses_idx.end()) {
         const auto &account_idx = database.get_index_type<account_index>().indices().get<by_name>();
         const auto &account_itr = account_idx.find(swdo.sidechain_from);
         if (account_itr == account_idx.end()) {
            return;
         }
      }

      ilog("Deposit to process: ${swdo}", ("swdo", swdo));

      bool process_deposit_result = process_deposit(swdo);

      if (!process_deposit_result) {
         wlog("Deposit not processed: ${swdo}", ("swdo", swdo));
         return;
      }
      plugin.log_son_proposal_retry(chain::operation::tag<chain::son_wallet_deposit_process_operation>::value, swdo.id);
   });
}

void sidechain_net_handler::process_withdrawals() {
   if (database.get_global_properties().active_sons.size() < database.get_chain_properties().immutable_parameters.min_son_count) {
      return;
   }

   const auto &idx = database.get_index_type<son_wallet_withdraw_index>().indices().get<by_withdraw_sidechain_and_confirmed_and_processed>();
   const auto &idx_range = idx.equal_range(std::make_tuple(sidechain, true, false));

   std::for_each(idx_range.first, idx_range.second, [&](const son_wallet_withdraw_object &swwo) {
      if (swwo.id == object_id_type(0, 0, 0) || !plugin.can_son_participate(chain::operation::tag<chain::son_wallet_withdraw_process_operation>::value, swwo.id)) {
         return;
      }

      ilog("Withdraw to process: ${swwo}", ("swwo", swwo));

      bool process_withdrawal_result = process_withdrawal(swwo);

      if (!process_withdrawal_result) {
         wlog("Withdraw not processed: ${swwo}", ("swwo", swwo));
         return;
      }
      plugin.log_son_proposal_retry(chain::operation::tag<chain::son_wallet_withdraw_process_operation>::value, swwo.id);
   });
}

void sidechain_net_handler::process_sidechain_transactions() {
   const auto &idx = database.get_index_type<sidechain_transaction_index>().indices().get<by_sidechain_and_status>();
   const auto &idx_range = idx.equal_range(std::make_tuple(sidechain, sidechain_transaction_status::valid));

   std::for_each(idx_range.first, idx_range.second, [&](const sidechain_transaction_object &sto) {
      if ((sto.id == object_id_type(0, 0, 0)) || !signer_expected(sto, plugin.get_current_son_id())) {
         return;
      }

      ilog("Sidechain transaction to process: ${sto}", ("sto", sto.id));

      std::string processed_sidechain_tx = process_sidechain_transaction(sto);

      if (processed_sidechain_tx.empty()) {
         wlog("Sidechain transaction not processed: ${sto}", ("sto", sto.id));
         return;
      }

      const chain::global_property_object &gpo = database.get_global_properties();
      sidechain_transaction_sign_operation sts_op;
      sts_op.signer = plugin.get_current_son_id();
      sts_op.payer = gpo.parameters.son_account();
      sts_op.sidechain_transaction_id = sto.id;
      sts_op.signature = processed_sidechain_tx;

      proposal_create_operation proposal_op;
      proposal_op.fee_paying_account = plugin.get_current_son_object().son_account;
      uint32_t lifetime = (gpo.parameters.block_interval * gpo.active_witnesses.size()) * 3;
      proposal_op.expiration_time = time_point_sec(database.head_block_time().sec_since_epoch() + lifetime);
      proposal_op.proposed_ops.emplace_back(sts_op);

      if (proposal_exists(chain::operation::tag<chain::sidechain_transaction_sign_operation>::value, sto.id, proposal_op.proposed_ops[0].op)) {
         return;
      }

      signed_transaction trx = database.create_signed_transaction(plugin.get_private_key(plugin.get_current_son_id()), proposal_op);
      try {
         trx.validate();
         database.push_transaction(trx, database::validation_steps::skip_block_size_check);
         if (plugin.app().p2p_node())
            plugin.app().p2p_node()->broadcast(net::trx_message(trx));
      } catch (fc::exception &e) {
         elog("Sending proposal for sidechain transaction sign operation failed with exception ${e}", ("e", e.what()));
      }
   });
}

void sidechain_net_handler::send_sidechain_transactions() {
   const auto &idx = database.get_index_type<sidechain_transaction_index>().indices().get<by_sidechain_and_status>();
   const auto &idx_range = idx.equal_range(std::make_tuple(sidechain, sidechain_transaction_status::complete));

   std::for_each(idx_range.first, idx_range.second, [&](const sidechain_transaction_object &sto) {
      if (sto.id == object_id_type(0, 0, 0)) {
         return;
      }

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
      try {
         trx.validate();
         database.push_transaction(trx, database::validation_steps::skip_block_size_check);
         if (plugin.app().p2p_node())
            plugin.app().p2p_node()->broadcast(net::trx_message(trx));
      } catch (fc::exception &e) {
         elog("Sending proposal for sidechain transaction send operation failed with exception ${e}", ("e", e.what()));
      }
   });
}

void sidechain_net_handler::settle_sidechain_transactions() {
   const auto &idx = database.get_index_type<sidechain_transaction_index>().indices().get<by_sidechain_and_status>();
   const auto &idx_range = idx.equal_range(std::make_tuple(sidechain, sidechain_transaction_status::sent));

   std::for_each(idx_range.first, idx_range.second, [&](const sidechain_transaction_object &sto) {
      if (sto.id == object_id_type(0, 0, 0)) {
         return;
      }

      if (proposal_exists(chain::operation::tag<chain::sidechain_transaction_settle_operation>::value, sto.id)) {
         return;
      }

      if (!plugin.can_son_participate(chain::operation::tag<chain::sidechain_transaction_settle_operation>::value, sto.object_id)) {
         return;
      }

      ilog("Sidechain transaction to settle: ${sto}", ("sto", sto.id));

      asset settle_amount;
      bool settle_sidechain_result = settle_sidechain_transaction(sto, settle_amount);

      if (settle_sidechain_result == false) {
         wlog("Sidechain transaction not settled: ${sto}", ("sto", sto.id));
         return;
      }

      const chain::global_property_object &gpo = database.get_global_properties();

      proposal_create_operation proposal_op;
      proposal_op.fee_paying_account = plugin.get_current_son_object().son_account;
      uint32_t lifetime = (gpo.parameters.block_interval * gpo.active_witnesses.size()) * 3;
      proposal_op.expiration_time = time_point_sec(database.head_block_time().sec_since_epoch() + lifetime);

      sidechain_transaction_settle_operation sts_op;
      sts_op.payer = gpo.parameters.son_account();
      sts_op.sidechain_transaction_id = sto.id;
      proposal_op.proposed_ops.emplace_back(sts_op);

      if (settle_amount.amount != 0) {
         if (sto.object_id.is<son_wallet_deposit_id_type>()) {
            asset_issue_operation ai_op;
            ai_op.fee = database.current_fee_schedule().calculate_fee(ai_op);
            ai_op.issuer = gpo.parameters.son_account();
            ai_op.asset_to_issue = settle_amount;
            ai_op.issue_to_account = database.get<son_wallet_deposit_object>(sto.object_id).peerplays_from;
            proposal_op.proposed_ops.emplace_back(ai_op);
         }

         if (sto.object_id.is<son_wallet_withdraw_id_type>()) {
            asset_reserve_operation ar_op;
            ar_op.fee = database.current_fee_schedule().calculate_fee(ar_op);
            ar_op.payer = gpo.parameters.son_account();
            ar_op.amount_to_reserve = settle_amount;
            proposal_op.proposed_ops.emplace_back(ar_op);
         }
      }

      signed_transaction trx = database.create_signed_transaction(plugin.get_private_key(plugin.get_current_son_id()), proposal_op);
      try {
         trx.validate();
         database.push_transaction(trx, database::validation_steps::skip_block_size_check);
         if (plugin.app().p2p_node())
            plugin.app().p2p_node()->broadcast(net::trx_message(trx));
         plugin.log_son_proposal_retry(chain::operation::tag<chain::sidechain_transaction_settle_operation>::value, sto.object_id);
      } catch (fc::exception &e) {
         elog("Sending proposal for sidechain transaction settle operation failed with exception ${e}", ("e", e.what()));
      }
   });
}

void sidechain_net_handler::on_applied_block(const signed_block &b) {

   const chain::global_property_object &gpo = plugin.database().get_global_properties();

   for (const auto &trx : b.transactions) {
      size_t operation_index = -1;
      for (auto op : trx.operations) {
         operation_index = operation_index + 1;
         if (op.which() == operation::tag<transfer_operation>::value) {
            transfer_operation transfer_op = op.get<transfer_operation>();

            if (transfer_op.to != gpo.parameters.son_account()) {
               continue;
            }

            bool is_tracked_asset =
                  ((sidechain == sidechain_type::bitcoin) && (transfer_op.amount.asset_id == gpo.parameters.btc_asset())) ||
                  ((sidechain == sidechain_type::hive) && (transfer_op.amount.asset_id == gpo.parameters.hbd_asset())) ||
                  ((sidechain == sidechain_type::hive) && (transfer_op.amount.asset_id == gpo.parameters.hive_asset()));

            if (!is_tracked_asset) {
               continue;
            }

            std::string sidechain_from = object_id_to_string(transfer_op.from);
            std::string memo = "";
            if (transfer_op.memo) {
               memo = transfer_op.memo->get_message(fc::ecc::private_key(), public_key_type());
               boost::trim(memo);
               if (!memo.empty()) {
                  sidechain_from = memo;
               }
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
            sed.sidechain_from = sidechain_from;
            sed.sidechain_to = object_id_to_string(transfer_op.to);
            sed.sidechain_currency = object_id_to_string(transfer_op.amount.asset_id);
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
