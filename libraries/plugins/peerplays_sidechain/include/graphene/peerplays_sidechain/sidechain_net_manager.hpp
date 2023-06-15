#pragma once

#include <graphene/chain/sidechain_defs.hpp>
#include <graphene/peerplays_sidechain/peerplays_sidechain_plugin.hpp>
#include <graphene/peerplays_sidechain/sidechain_net_handler.hpp>

#include <vector>

#include <boost/program_options.hpp>

namespace graphene { namespace peerplays_sidechain {

class sidechain_net_manager {
public:
   sidechain_net_manager(peerplays_sidechain_plugin &_plugin);
   virtual ~sidechain_net_manager();

   bool create_handler(sidechain_type sidechain, const boost::program_options::variables_map &options);
   void process_proposals();
   void process_active_sons_change();
   void create_deposit_addresses();
   void process_deposits();
   void process_withdrawals();
   void process_sidechain_transactions();
   void send_sidechain_transactions();
   void settle_sidechain_transactions();

   std::map<sidechain_type, std::vector<std::string>> get_son_listener_log();

private:
   peerplays_sidechain_plugin &plugin;
   graphene::chain::database &database;
   std::vector<std::unique_ptr<sidechain_net_handler>> net_handlers;

   void on_applied_block(const signed_block &b);
};

}} // namespace graphene::peerplays_sidechain
