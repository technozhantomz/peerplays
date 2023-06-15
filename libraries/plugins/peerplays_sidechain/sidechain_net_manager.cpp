#include <graphene/peerplays_sidechain/sidechain_net_manager.hpp>

#include <fc/log/logger.hpp>
#include <graphene/chain/son_wallet_object.hpp>
#include <graphene/peerplays_sidechain/sidechain_net_handler_bitcoin.hpp>
#include <graphene/peerplays_sidechain/sidechain_net_handler_hive.hpp>
#include <graphene/peerplays_sidechain/sidechain_net_handler_peerplays.hpp>

namespace graphene { namespace peerplays_sidechain {

sidechain_net_manager::sidechain_net_manager(peerplays_sidechain_plugin &_plugin) :
      plugin(_plugin),
      database(_plugin.database()) {

   //database.applied_block.connect([&](const signed_block &b) {
   //   on_applied_block(b);
   //});
}

sidechain_net_manager::~sidechain_net_manager() {
}

bool sidechain_net_manager::create_handler(sidechain_type sidechain, const boost::program_options::variables_map &options) {

   bool ret_val = false;

   switch (sidechain) {
   case sidechain_type::bitcoin: {
      std::unique_ptr<sidechain_net_handler> h = std::unique_ptr<sidechain_net_handler>(new sidechain_net_handler_bitcoin(plugin, options));
      net_handlers.push_back(std::move(h));
      ret_val = true;
      break;
   }
   case sidechain_type::hive: {
      std::unique_ptr<sidechain_net_handler> h = std::unique_ptr<sidechain_net_handler>(new sidechain_net_handler_hive(plugin, options));
      net_handlers.push_back(std::move(h));
      ret_val = true;
      break;
   }
   case sidechain_type::peerplays: {
      std::unique_ptr<sidechain_net_handler> h = std::unique_ptr<sidechain_net_handler>(new sidechain_net_handler_peerplays(plugin, options));
      net_handlers.push_back(std::move(h));
      ret_val = true;
      break;
   }
   default:
      assert(false);
   }

   return ret_val;
}

void sidechain_net_manager::process_proposals() {
   for (size_t i = 0; i < net_handlers.size(); i++) {
      net_handlers.at(i)->process_proposals();
   }
}

void sidechain_net_manager::process_active_sons_change() {
   for (size_t i = 0; i < net_handlers.size(); i++) {
      net_handlers.at(i)->process_active_sons_change();
   }
}

void sidechain_net_manager::create_deposit_addresses() {
   for (size_t i = 0; i < net_handlers.size(); i++) {
      net_handlers.at(i)->create_deposit_addresses();
   }
}

void sidechain_net_manager::process_deposits() {
   for (size_t i = 0; i < net_handlers.size(); i++) {
      net_handlers.at(i)->process_deposits();
   }
}

void sidechain_net_manager::process_withdrawals() {
   for (size_t i = 0; i < net_handlers.size(); i++) {
      net_handlers.at(i)->process_withdrawals();
   }
}

void sidechain_net_manager::process_sidechain_transactions() {
   for (size_t i = 0; i < net_handlers.size(); i++) {
      net_handlers.at(i)->process_sidechain_transactions();
   }
}

void sidechain_net_manager::send_sidechain_transactions() {
   for (size_t i = 0; i < net_handlers.size(); i++) {
      net_handlers.at(i)->send_sidechain_transactions();
   }
}

void sidechain_net_manager::settle_sidechain_transactions() {
   for (size_t i = 0; i < net_handlers.size(); i++) {
      net_handlers.at(i)->settle_sidechain_transactions();
   }
}

std::map<sidechain_type, std::vector<std::string>> sidechain_net_manager::get_son_listener_log() {
   std::map<sidechain_type, std::vector<std::string>> result;
   for (size_t i = 0; i < net_handlers.size(); i++) {
      result[net_handlers.at(i)->get_sidechain()] = net_handlers.at(i)->get_son_listener_log();
   }
   return result;
}

void sidechain_net_manager::on_applied_block(const signed_block &b) {
}

}} // namespace graphene::peerplays_sidechain
