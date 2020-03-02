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
#include <graphene/chain/sidechain_address_object.hpp>
#include <graphene/chain/son_info.hpp>
#include <graphene/chain/son_wallet_object.hpp>

namespace graphene { namespace peerplays_sidechain {

sidechain_net_handler_peerplays::sidechain_net_handler_peerplays(peerplays_sidechain_plugin &_plugin, const boost::program_options::variables_map &options) :
      sidechain_net_handler(_plugin, options) {
   sidechain = sidechain_type::peerplays;
   plugin.database().applied_block.connect([&](const signed_block &b) {
      on_applied_block(b);
   });
}

sidechain_net_handler_peerplays::~sidechain_net_handler_peerplays() {
}

void sidechain_net_handler_peerplays::recreate_primary_wallet() {
}

void sidechain_net_handler_peerplays::process_deposit(const son_wallet_deposit_object &swdo) {
}

void sidechain_net_handler_peerplays::process_withdrawal(const son_wallet_withdraw_object &swwo) {
}

std::string sidechain_net_handler_peerplays::create_multisignature_wallet(const std::vector<std::string> public_keys) {
   return "";
}

std::string sidechain_net_handler_peerplays::transfer(const std::string &from, const std::string &to, const uint64_t amount) {
   return "";
}

std::string sidechain_net_handler_peerplays::sign_transaction(const std::string &transaction) {
   return "";
}

std::string sidechain_net_handler_peerplays::send_transaction(const std::string &transaction) {
   return "";
}

void sidechain_net_handler_peerplays::on_applied_block(const signed_block &b) {
   for (const auto &trx : b.transactions) {
      size_t operation_index = -1;
      for (auto op : trx.operations) {
         operation_index = operation_index + 1;
         if (op.which() == operation::tag<transfer_operation>::value) {
            transfer_operation transfer_op = op.get<transfer_operation>();
            if (transfer_op.to != GRAPHENE_SON_ACCOUNT) {
               continue;
            }

            std::stringstream ss;
            ss << "peerplays"
               << "-" << trx.id().str() << "-" << operation_index;
            std::string sidechain_uid = ss.str();

            sidechain_event_data sed;
            sed.timestamp = plugin.database().head_block_time();
            sed.sidechain = sidechain_type::peerplays;
            sed.sidechain_uid = sidechain_uid;
            sed.sidechain_transaction_id = trx.id().str();
            sed.sidechain_from = fc::to_string(transfer_op.from.space_id) + "." + fc::to_string(transfer_op.from.type_id) + "." + fc::to_string((uint64_t)transfer_op.from.instance);
            sed.sidechain_to = fc::to_string(transfer_op.to.space_id) + "." + fc::to_string(transfer_op.to.type_id) + "." + fc::to_string((uint64_t)transfer_op.to.instance);
            sed.sidechain_currency = fc::to_string(transfer_op.amount.asset_id.space_id) + "." + fc::to_string(transfer_op.amount.asset_id.type_id) + "." + fc::to_string((uint64_t)transfer_op.amount.asset_id.instance); //transfer_op.amount.asset_id(plugin.database()).symbol;
            sed.sidechain_amount = transfer_op.amount.amount;
            sed.peerplays_from = transfer_op.from;
            sed.peerplays_to = transfer_op.to;
            // We should calculate exchange rate between CORE/TEST and other Peerplays asset
            sed.peerplays_asset = asset(transfer_op.amount.amount / transfer_op.amount.asset_id(plugin.database()).options.core_exchange_rate.quote.amount);
            sidechain_event_data_received(sed);
         }
      }
   }
}

}} // namespace graphene::peerplays_sidechain
