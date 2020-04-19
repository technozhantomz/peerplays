#pragma once

#include <vector>

#include <boost/program_options.hpp>

#include <fc/signals.hpp>
#include <graphene/chain/proposal_object.hpp>
#include <graphene/chain/sidechain_address_object.hpp>
#include <graphene/chain/sidechain_transaction_object.hpp>
#include <graphene/chain/son_wallet_deposit_object.hpp>
#include <graphene/chain/son_wallet_withdraw_object.hpp>
#include <graphene/peerplays_sidechain/defs.hpp>
#include <graphene/peerplays_sidechain/peerplays_sidechain_plugin.hpp>

namespace graphene { namespace peerplays_sidechain {

class sidechain_net_handler {
public:
   sidechain_net_handler(peerplays_sidechain_plugin &_plugin, const boost::program_options::variables_map &options);
   virtual ~sidechain_net_handler();

   sidechain_type get_sidechain();
   std::vector<std::string> get_sidechain_deposit_addresses();
   std::vector<std::string> get_sidechain_withdraw_addresses();
   std::string get_private_key(std::string public_key);

   bool proposal_exists(int32_t operation_tag, const object_id_type &object_id);
   bool approve_proposal(const proposal_id_type &proposal_id, const son_id_type &son_id);
   void sidechain_event_data_received(const sidechain_event_data &sed);

   void process_proposals();
   void process_active_sons_change();
   void process_deposits();
   void process_withdrawals();
   void process_sidechain_transactions();
   void send_sidechain_transactions();
   void settle_sidechain_transactions();

   virtual bool process_proposal(const proposal_object &po) = 0;
   virtual void process_primary_wallet() = 0;
   virtual void process_sidechain_addresses() = 0;
   virtual bool process_deposit(const son_wallet_deposit_object &swdo) = 0;
   virtual bool process_withdrawal(const son_wallet_withdraw_object &swwo) = 0;
   virtual std::string process_sidechain_transaction(const sidechain_transaction_object &sto) = 0;
   virtual std::string send_sidechain_transaction(const sidechain_transaction_object &sto) = 0;
   virtual int64_t settle_sidechain_transaction(const sidechain_transaction_object &sto) = 0;

protected:
   peerplays_sidechain_plugin &plugin;
   graphene::chain::database &database;
   sidechain_type sidechain;

   std::map<std::string, std::string> private_keys;

private:
};

}} // namespace graphene::peerplays_sidechain
