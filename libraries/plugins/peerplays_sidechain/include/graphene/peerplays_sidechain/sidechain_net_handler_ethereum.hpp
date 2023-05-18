#pragma once

#include <graphene/peerplays_sidechain/sidechain_net_handler.hpp>

#include <string>

#include <boost/bimap.hpp>
#include <boost/signals2.hpp>

#include <graphene/peerplays_sidechain/common/rpc_client.hpp>
#include <graphene/peerplays_sidechain/ethereum/types.hpp>

namespace graphene { namespace peerplays_sidechain {

class ethereum_rpc_client : public rpc_client {
public:
   ethereum_rpc_client(const std::vector<rpc_credentials> &credentials, bool debug_rpc_calls, bool simulate_connection_reselection);

   std::string eth_blockNumber();
   std::string eth_get_block_by_number(std::string block_number, bool full_block);
   std::string eth_get_logs(std::string wallet_contract_address);
   std::string eth_chainId();
   std::string net_version();
   std::string eth_get_transaction_count(const std::string &params);
   std::string eth_gas_price();
   std::string eth_estimateGas(const std::string &params);

   std::string get_chain_id();
   std::string get_network_id();
   std::string get_nonce(const std::string &address);
   std::string get_gas_price();
   std::string get_gas_limit();
   std::string get_estimate_gas(const std::string &params);

   std::string eth_send_transaction(const std::string &params);
   std::string eth_send_raw_transaction(const std::string &params);
   std::string eth_get_transaction_receipt(const std::string &params);
   std::string eth_get_transaction_by_hash(const std::string &params);

   virtual uint64_t ping(rpc_connection &conn) const override;
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
   virtual optional<asset> estimate_withdrawal_transaction_fee() const override;

private:
   std::vector<rpc_credentials> _rpc_credentials;
   std::string wallet_contract_address;
   using bimap_type = boost::bimap<std::string, std::string>;
   bimap_type erc20_addresses;

   ethereum_rpc_client *rpc_client;

   ethereum::chain_id_type chain_id;
   ethereum::network_id_type network_id;

   std::string create_primary_wallet_transaction(const std::vector<son_sidechain_info> &son_pubkeys, const std::string &object_id);
   std::string create_deposit_transaction(const son_wallet_deposit_object &swdo);
   std::string create_withdrawal_transaction(const son_wallet_withdraw_object &swwo);

   std::string sign_transaction(const sidechain_transaction_object &sto);

   uint64_t last_block_received;
   fc::future<void> _listener_task;
   boost::signals2::signal<void(const std::string &)> event_received;
   void schedule_ethereum_listener();
   void ethereum_listener_loop();
   void handle_event(const std::string &block_number);
};

}} // namespace graphene::peerplays_sidechain
