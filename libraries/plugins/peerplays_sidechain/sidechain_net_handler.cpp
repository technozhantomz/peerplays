#include <graphene/peerplays_sidechain/sidechain_net_handler.hpp>

#include <graphene/chain/sidechain_address_object.hpp>
#include <graphene/chain/son_wallet_deposit_object.hpp>
#include <graphene/chain/son_wallet_withdraw_object.hpp>

#include <fc/log/logger.hpp>

namespace graphene { namespace peerplays_sidechain {

sidechain_net_handler::sidechain_net_handler(peerplays_sidechain_plugin &_plugin, const boost::program_options::variables_map &options) :
      plugin(_plugin),
      database(_plugin.database()) {
}

sidechain_net_handler::~sidechain_net_handler() {
}

graphene::peerplays_sidechain::sidechain_type sidechain_net_handler::get_sidechain() {
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
   if ((sed.peerplays_to == GRAPHENE_SON_ACCOUNT) && (sed.sidechain_currency.compare("1.3.0") != 0)) {
      son_wallet_deposit_create_operation op;
      op.payer = GRAPHENE_SON_ACCOUNT;
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
            proposal_create_operation proposal_op;
            proposal_op.fee_paying_account = plugin.get_son_object(son_id).son_account;
            proposal_op.proposed_ops.emplace_back(op_wrapper(op));
            uint32_t lifetime = (gpo.parameters.block_interval * gpo.active_witnesses.size()) * 3;
            proposal_op.expiration_time = time_point_sec(database.head_block_time().sec_since_epoch() + lifetime);

            signed_transaction trx = plugin.database().create_signed_transaction(plugin.get_private_key(son_id), proposal_op);
            try {
               database.push_transaction(trx, database::validation_steps::skip_block_size_check);
               if (plugin.app().p2p_node())
                  plugin.app().p2p_node()->broadcast(net::trx_message(trx));
            } catch (fc::exception e) {
               ilog("sidechain_net_handler:  sending proposal for son wallet deposit create operation by ${son} failed with exception ${e}", ("son", son_id)("e", e.what()));
            }
         }
      }
      return;
   }

   // Withdrawal request
   if ((sed.peerplays_to == GRAPHENE_SON_ACCOUNT) && (sed.sidechain_currency.compare("1.3.0") == 0)) {
      // BTC Payout only (for now)
      const auto &sidechain_addresses_idx = database.get_index_type<sidechain_address_index>().indices().get<by_account_and_sidechain>();
      const auto &addr_itr = sidechain_addresses_idx.find(std::make_tuple(sed.peerplays_from, sidechain_type::bitcoin));
      if (addr_itr == sidechain_addresses_idx.end())
         return;

      son_wallet_withdraw_create_operation op;
      op.payer = GRAPHENE_SON_ACCOUNT;
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
            proposal_create_operation proposal_op;
            proposal_op.fee_paying_account = plugin.get_son_object(son_id).son_account;
            proposal_op.proposed_ops.emplace_back(op_wrapper(op));
            uint32_t lifetime = (gpo.parameters.block_interval * gpo.active_witnesses.size()) * 3;
            proposal_op.expiration_time = time_point_sec(database.head_block_time().sec_since_epoch() + lifetime);

            signed_transaction trx = plugin.database().create_signed_transaction(plugin.get_private_key(son_id), proposal_op);
            try {
               database.push_transaction(trx, database::validation_steps::skip_block_size_check);
               if (plugin.app().p2p_node())
                  plugin.app().p2p_node()->broadcast(net::trx_message(trx));
            } catch (fc::exception e) {
               ilog("sidechain_net_handler:  sending proposal for son wallet withdraw create operation by ${son} failed with exception ${e}", ("son", son_id)("e", e.what()));
            }
         }
      }
      return;
   }

   FC_ASSERT(false, "Invalid sidechain event");
}

void sidechain_net_handler::process_deposits() {
   const auto &idx = plugin.database().get_index_type<son_wallet_deposit_index>().indices().get<by_sidechain_and_processed>();
   const auto &idx_range = idx.equal_range(std::make_tuple(sidechain, false));

   std::for_each(idx_range.first, idx_range.second,
                 [&](const son_wallet_deposit_object &swdo) {
                    ilog("Deposit to process: ${swdo}", ("swdo", swdo));

                    process_deposit(swdo);

                    const chain::global_property_object &gpo = plugin.database().get_global_properties();

                    son_wallet_deposit_process_operation p_op;
                    p_op.payer = GRAPHENE_SON_ACCOUNT;
                    p_op.son_wallet_deposit_id = swdo.id;

                    proposal_create_operation proposal_op;
                    proposal_op.fee_paying_account = plugin.get_son_object(plugin.get_current_son_id()).son_account;
                    proposal_op.proposed_ops.emplace_back(op_wrapper(p_op));
                    uint32_t lifetime = (gpo.parameters.block_interval * gpo.active_witnesses.size()) * 3;
                    proposal_op.expiration_time = time_point_sec(plugin.database().head_block_time().sec_since_epoch() + lifetime);

                    signed_transaction trx = plugin.database().create_signed_transaction(plugin.get_private_key(plugin.get_current_son_id()), proposal_op);
                    trx.validate();
                    try {
                       plugin.database().push_transaction(trx, database::validation_steps::skip_block_size_check);
                       if (plugin.app().p2p_node())
                          plugin.app().p2p_node()->broadcast(net::trx_message(trx));
                    } catch (fc::exception e) {
                       ilog("sidechain_net_handler:  sending proposal for transfer operation failed with exception ${e}", ("e", e.what()));
                    }
                 });
}

void sidechain_net_handler::process_withdrawals() {
   const auto &idx = plugin.database().get_index_type<son_wallet_withdraw_index>().indices().get<by_withdraw_sidechain_and_processed>();
   const auto &idx_range = idx.equal_range(std::make_tuple(sidechain, false));

   std::for_each(idx_range.first, idx_range.second,
                 [&](const son_wallet_withdraw_object &swwo) {
                    ilog("Withdraw to process: ${swwo}", ("swwo", swwo));

                    process_withdrawal(swwo);

                    const chain::global_property_object &gpo = plugin.database().get_global_properties();

                    son_wallet_withdraw_process_operation p_op;
                    p_op.payer = GRAPHENE_SON_ACCOUNT;
                    p_op.son_wallet_withdraw_id = swwo.id;

                    proposal_create_operation proposal_op;
                    proposal_op.fee_paying_account = plugin.get_son_object(plugin.get_current_son_id()).son_account;
                    proposal_op.proposed_ops.emplace_back(op_wrapper(p_op));
                    uint32_t lifetime = (gpo.parameters.block_interval * gpo.active_witnesses.size()) * 3;
                    proposal_op.expiration_time = time_point_sec(plugin.database().head_block_time().sec_since_epoch() + lifetime);

                    signed_transaction trx = plugin.database().create_signed_transaction(plugin.get_private_key(plugin.get_current_son_id()), proposal_op);
                    trx.validate();
                    try {
                       plugin.database().push_transaction(trx, database::validation_steps::skip_block_size_check);
                       if (plugin.app().p2p_node())
                          plugin.app().p2p_node()->broadcast(net::trx_message(trx));
                    } catch (fc::exception e) {
                       ilog("sidechain_net_handler:  sending proposal for transfer operation failed with exception ${e}", ("e", e.what()));
                    }
                 });
}

void sidechain_net_handler::recreate_primary_wallet() {
   FC_ASSERT(false, "recreate_primary_wallet not implemented");
}

void sidechain_net_handler::process_deposit(const son_wallet_deposit_object &swdo) {
   FC_ASSERT(false, "process_deposit not implemented");
}

void sidechain_net_handler::process_withdrawal(const son_wallet_withdraw_object &swwo) {
   FC_ASSERT(false, "process_withdrawal not implemented");
}

}} // namespace graphene::peerplays_sidechain
