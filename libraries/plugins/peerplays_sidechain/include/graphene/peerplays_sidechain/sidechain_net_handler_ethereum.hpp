#pragma once

#include <graphene/peerplays_sidechain/sidechain_net_handler.hpp>

#include <string>

#include <boost/signals2.hpp>

#include <graphene/peerplays_sidechain/common/rpc_client.hpp>
#include <graphene/peerplays_sidechain/ethereum/types.hpp>

namespace graphene { namespace peerplays_sidechain {

class ethereum_rpc_client : public rpc_client {
public:
   ethereum_rpc_client(const std::string &url, const std::string &user_name, const std::string &password, bool debug_rpc_calls);

   std::string eth_get_block_by_number(std::string block_number, bool full_block);
   std::string eth_get_logs(std::string wallet_contract_address);
   std::string eth_chainId();
   std::string net_version();
   std::string eth_get_transaction_count(const std::string &params);
   std::string eth_gas_price();

   std::string get_chain_id();
   std::string get_network_id();
   std::string get_nonce(const std::string &address);
   std::string get_gas_price();
   std::string get_gas_limit();

   std::string eth_send_transaction(const std::string &params);
   std::string eth_send_raw_transaction(const std::string &params);
   std::string eth_get_transaction_receipt(const std::string &params);
};

class sidechain_net_handler_ethereum : public sidechain_net_handler {
public:
   sidechain_net_handler_ethereum(peerplays_sidechain_plugin &_plugin, const boost::program_options::variables_map &options);
   virtual ~sidechain_net_handler_ethereum();

   bool process_proposal(const proposal_object &po);
   void process_primary_wallet();
   void process_sidechain_addresses();
   bool process_deposit(const son_wallet_deposit_object &swdo);
   bool process_withdrawal(const son_wallet_withdraw_object &swwo);
   std::string process_sidechain_transaction(const sidechain_transaction_object &sto);
   std::string send_sidechain_transaction(const sidechain_transaction_object &sto);
   bool settle_sidechain_transaction(const sidechain_transaction_object &sto, asset &settle_amount);

private:
   std::string rpc_url;
   std::string rpc_user;
   std::string rpc_password;
   std::string wallet_contract_address;

   ethereum_rpc_client *rpc_client;

   ethereum::chain_id_type chain_id;
   ethereum::network_id_type network_id;

   std::string create_primary_wallet_transaction(const std::vector<son_info> &son_pubkeys, const std::string &object_id);
   std::string create_deposit_transaction(const son_wallet_deposit_object &swdo);
   std::string create_withdrawal_transaction(const son_wallet_withdraw_object &swwo);

   std::string sign_transaction(const sidechain_transaction_object &sto);

   uint64_t last_block_received;
   fc::future<void> _listener_task;
   boost::signals2::signal<void(const std::string &)> event_received;
   void schedule_ethereum_listener();
   void ethereum_listener_loop();
   void handle_event(const std::string &event_data);
};

}} // namespace graphene::peerplays_sidechain
