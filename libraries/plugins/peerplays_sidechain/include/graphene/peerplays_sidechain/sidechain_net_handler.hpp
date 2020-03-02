#pragma once

#include <vector>

#include <boost/program_options.hpp>

#include <fc/signals.hpp>
#include <graphene/peerplays_sidechain/defs.hpp>
#include <graphene/peerplays_sidechain/peerplays_sidechain_plugin.hpp>

namespace graphene { namespace peerplays_sidechain {

class sidechain_net_handler {
public:
   sidechain_net_handler(peerplays_sidechain_plugin &_plugin, const boost::program_options::variables_map &options);
   virtual ~sidechain_net_handler();

   graphene::peerplays_sidechain::sidechain_type get_sidechain();
   std::vector<std::string> get_sidechain_deposit_addresses();
   std::vector<std::string> get_sidechain_withdraw_addresses();
   std::string get_private_key(std::string public_key);

   void sidechain_event_data_received(const sidechain_event_data &sed);
   void process_deposits();
   void process_withdrawals();

   virtual void recreate_primary_wallet() = 0;
   virtual void process_deposit(const son_wallet_deposit_object &swdo) = 0;
   virtual void process_withdrawal(const son_wallet_withdraw_object &swwo) = 0;

protected:
   peerplays_sidechain_plugin &plugin;
   graphene::chain::database &database;
   graphene::peerplays_sidechain::sidechain_type sidechain;

   std::map<std::string, std::string> private_keys;

   virtual std::string create_multisignature_wallet(const std::vector<std::string> public_keys) = 0;
   virtual std::string transfer(const std::string &from, const std::string &to, const uint64_t amount) = 0;
   virtual std::string sign_transaction(const std::string &transaction) = 0;
   virtual std::string send_transaction(const std::string &transaction) = 0;

private:
};

}} // namespace graphene::peerplays_sidechain
