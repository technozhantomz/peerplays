#pragma once

#include <graphene/peerplays_sidechain/sidechain_net_handler.hpp>
#include <graphene/peerplays_sidechain/common/rpc_client.hpp>

#include <string>
#include <thread>
#include <zmq_addon.hpp>

#include <boost/signals2.hpp>

#include <mutex>

#include <fc/network/http/connection.hpp>
#include <graphene/peerplays_sidechain/bitcoin/bitcoin_address.hpp>

namespace graphene { namespace peerplays_sidechain {

class btc_txout {
public:
   std::string txid_;
   unsigned int out_num_;
   uint64_t amount_;
};

class bitcoin_rpc_client: public rpc_client  {
public:
   enum class multi_type {
      script,
      address
   };
   struct multi_params {
      multi_params(multi_type _type, const std::string &_address_or_script, const std::string &_label = "") :
            type{_type},
            address_or_script{_address_or_script},
            label{_label} {
      }

      multi_type type;
      std::string address_or_script;
      std::string label;
   };

public:
   bitcoin_rpc_client(std::string _url, std::string _user, std::string _password, bool _debug_rpc_calls);

   std::string createwallet(const std::string &wallet_name);
   uint64_t estimatesmartfee(uint16_t conf_target = 128);
   std::string getblock(const std::string &block_hash, int32_t verbosity = 2);
   std::string getrawtransaction(const std::string &txid, const bool verbose = false);
   std::string getnetworkinfo();
   std::string getblockchaininfo();
   void importmulti(const std::vector<multi_params> &address_or_script_array, const bool rescan = true);
   std::vector<btc_txout> listunspent(const uint32_t minconf = 1, const uint32_t maxconf = 9999999);
   std::vector<btc_txout> listunspent_by_address_and_amount(const std::string &address, double transfer_amount, const uint32_t minconf = 1, const uint32_t maxconf = 9999999);
   std::string loadwallet(const std::string &filename);
   std::string sendrawtransaction(const std::string &tx_hex);
   std::string walletlock();
   bool walletpassphrase(const std::string &passphrase, uint32_t timeout = 60);

private:
   std::string ip;
   uint32_t rpc_port;
   std::string user;
   std::string password;
   std::string wallet;
   std::string wallet_password;

};

// =============================================================================

class zmq_listener {
public:
   zmq_listener(std::string _ip, uint32_t _zmq);
   virtual ~zmq_listener();

   void start();
   boost::signals2::signal<void(const std::string &)> event_received;

private:
   void handle_zmq();
   std::vector<zmq::message_t> receive_multipart();

   std::string ip;
   uint32_t zmq_port;

   zmq::context_t ctx;
   zmq::socket_t socket;

   std::atomic_bool stopped;
   std::thread thr;
};

// =============================================================================

class sidechain_net_handler_bitcoin : public sidechain_net_handler {
public:
   sidechain_net_handler_bitcoin(peerplays_sidechain_plugin &_plugin, const boost::program_options::variables_map &options);
   virtual ~sidechain_net_handler_bitcoin();

   bool process_proposal(const proposal_object &po);
   void process_primary_wallet();
   void process_sidechain_addresses();
   bool process_deposit(const son_wallet_deposit_object &swdo);
   bool process_withdrawal(const son_wallet_withdraw_object &swwo);
   std::string process_sidechain_transaction(const sidechain_transaction_object &sto);
   std::string send_sidechain_transaction(const sidechain_transaction_object &sto);
   bool settle_sidechain_transaction(const sidechain_transaction_object &sto, asset &settle_amount);

private:
   std::string ip;
   uint32_t zmq_port;
   uint32_t rpc_port;
   uint32_t bitcoin_major_version;
   std::string rpc_user;
   std::string rpc_password;
   std::string wallet;
   std::string wallet_password;

   std::unique_ptr<bitcoin_rpc_client> bitcoin_client;
   std::unique_ptr<zmq_listener> listener;

   fc::future<void> on_changed_objects_task;
   bitcoin::bitcoin_address::network network_type;

   std::mutex event_handler_mutex;
   typedef std::lock_guard<decltype(event_handler_mutex)> scoped_lock;

   std::string create_primary_wallet_address(const std::vector<son_info> &son_pubkeys);

   std::string create_primary_wallet_transaction(const son_wallet_object &prev_swo, std::string new_sw_address);
   std::string create_deposit_transaction(const son_wallet_deposit_object &swdo);
   std::string create_withdrawal_transaction(const son_wallet_withdraw_object &swwo);

   std::string create_transaction(const std::vector<btc_txout> &inputs, const fc::flat_map<std::string, double> outputs, std::string &redeem_script);
   std::string sign_transaction(const sidechain_transaction_object &sto);
   std::string send_transaction(const sidechain_transaction_object &sto);

   void handle_event(const std::string &event_data);
   std::string get_redeemscript_for_userdeposit(const std::string &user_address);
   std::vector<info_for_vin> extract_info_from_block(const std::string &_block);
   void on_changed_objects(const vector<object_id_type> &ids, const flat_set<account_id_type> &accounts);
   void on_changed_objects_cb(const vector<object_id_type> &ids, const flat_set<account_id_type> &accounts);
};

}} // namespace graphene::peerplays_sidechain
