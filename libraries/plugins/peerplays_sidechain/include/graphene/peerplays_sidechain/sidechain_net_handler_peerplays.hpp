#pragma once

#include <graphene/peerplays_sidechain/sidechain_net_handler.hpp>

#include <string>

#include <fc/signals.hpp>

namespace graphene { namespace peerplays_sidechain {

class sidechain_net_handler_peerplays : public sidechain_net_handler {
public:
   sidechain_net_handler_peerplays(peerplays_sidechain_plugin &_plugin, const boost::program_options::variables_map &options);
   virtual ~sidechain_net_handler_peerplays();

   bool process_proposal(const proposal_object &po);
   void process_primary_wallet();
   void process_sidechain_addresses();
   bool process_deposit(const son_wallet_deposit_object &swdo);
   bool process_withdrawal(const son_wallet_withdraw_object &swwo);
   std::string process_sidechain_transaction(const sidechain_transaction_object &sto);
   std::string send_sidechain_transaction(const sidechain_transaction_object &sto);

private:
   void on_applied_block(const signed_block &b);
};

}} // namespace graphene::peerplays_sidechain
