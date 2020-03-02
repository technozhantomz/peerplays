#pragma once

#include <graphene/peerplays_sidechain/sidechain_net_handler.hpp>

#include <string>
#include <zmq.hpp>

#include <fc/network/http/connection.hpp>
#include <fc/signals.hpp>
#include <graphene/chain/son_wallet_deposit_object.hpp>
#include <graphene/chain/son_wallet_withdraw_object.hpp>

namespace graphene { namespace peerplays_sidechain {

class btc_txout {
public:
   std::string txid_;
   unsigned int out_num_;
   double amount_;
};

class bitcoin_rpc_client {
public:
   bitcoin_rpc_client(std::string _ip, uint32_t _rpc, std::string _user, std::string _password, std::string _wallet, std::string _wallet_password);
   bool connection_is_not_defined() const;

   std::string addmultisigaddress(const std::vector<std::string> public_keys);
   std::string createrawtransaction(const std::vector<btc_txout> &ins, const fc::flat_map<std::string, double> outs);
   std::string createwallet(const std::string &wallet_name);
   std::string encryptwallet(const std::string &passphrase);
   uint64_t estimatesmartfee();
   std::string getblock(const std::string &block_hash, int32_t verbosity = 2);
   void importaddress(const std::string &address_or_script);
   std::vector<btc_txout> listunspent();
   std::vector<btc_txout> listunspent_by_address_and_amount(const std::string &address, double transfer_amount);
   std::string loadwallet(const std::string &filename);
   void sendrawtransaction(const std::string &tx_hex);
   std::string signrawtransactionwithkey(const std::string &tx_hash, const std::string &private_key);
   std::string signrawtransactionwithwallet(const std::string &tx_hash);
   std::string unloadwallet(const std::string &filename);
   std::string walletlock();
   bool walletpassphrase(const std::string &passphrase, uint32_t timeout = 60);

private:
   fc::http::reply send_post_request(std::string body, bool show_log = false);

   std::string ip;
   uint32_t rpc_port;
   std::string user;
   std::string password;
   std::string wallet;
   std::string wallet_password;

   fc::http::header authorization;
};

// =============================================================================

class zmq_listener {
public:
   zmq_listener(std::string _ip, uint32_t _zmq);
   bool connection_is_not_defined() const {
      return zmq_port == 0;
   }

   fc::signal<void(const std::string &)> event_received;

private:
   void handle_zmq();
   std::vector<zmq::message_t> receive_multipart();

   std::string ip;
   uint32_t zmq_port;

   zmq::context_t ctx;
   zmq::socket_t socket;
};

// =============================================================================

class sidechain_net_handler_bitcoin : public sidechain_net_handler {
public:
   sidechain_net_handler_bitcoin(peerplays_sidechain_plugin &_plugin, const boost::program_options::variables_map &options);
   virtual ~sidechain_net_handler_bitcoin();

   void recreate_primary_wallet();
   void process_deposit(const son_wallet_deposit_object &swdo);
   void process_withdrawal(const son_wallet_withdraw_object &swwo);

private:
   std::string ip;
   uint32_t zmq_port;
   uint32_t rpc_port;
   std::string rpc_user;
   std::string rpc_password;
   std::string wallet;
   std::string wallet_password;

   std::unique_ptr<bitcoin_rpc_client> bitcoin_client;
   std::unique_ptr<zmq_listener> listener;

   std::string create_multisignature_wallet(const std::vector<std::string> public_keys);
   std::string transfer(const std::string &from, const std::string &to, const uint64_t amount);
   std::string sign_transaction(const std::string &transaction);
   std::string send_transaction(const std::string &transaction);
   std::string sign_and_send_transaction_with_wallet(const std::string &tx_json);
   std::string transfer_all_btc(const std::string &from_address, const std::string &to_address);
   std::string transfer_deposit_to_primary_wallet(const son_wallet_deposit_object &swdo);
   std::string transfer_withdrawal_from_primary_wallet(const son_wallet_withdraw_object &swwo);

   void handle_event(const std::string &event_data);
   std::vector<info_for_vin> extract_info_from_block(const std::string &_block);
};

}} // namespace graphene::peerplays_sidechain
