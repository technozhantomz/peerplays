#pragma once

#include <graphene/peerplays_sidechain/sidechain_net_handler.hpp>

#include <mutex>
#include <string>
#include <thread>
#include <zmq_addon.hpp>

#include <boost/signals2.hpp>

#include <fc/network/http/connection.hpp>

#include <graphene/peerplays_sidechain/bitcoin/bitcoin_address.hpp>
#include <graphene/peerplays_sidechain/bitcoin/estimate_fee_external.hpp>
#include <graphene/peerplays_sidechain/bitcoin/libbitcoin_client.hpp>
#include <graphene/peerplays_sidechain/common/rpc_client.hpp>

namespace graphene { namespace peerplays_sidechain {

class btc_txout {
public:
   std::string txid_;
   unsigned int out_num_;
   uint64_t amount_;
};

class btc_txin {
public:
   std::vector<std::string> tx_address;
   uint64_t tx_amount;
   uint64_t tx_vout;
};

class btc_tx {
public:
   std::string tx_txid;
   uint32_t tx_confirmations;
   std::vector<btc_txin> tx_in_list;
};

class block_data {
public:
   std::string block_hash;
   libbitcoin::chain::block block;
};

class bitcoin_client_base {
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

   virtual uint64_t estimatesmartfee(uint16_t conf_target = 1) = 0;
   virtual std::vector<info_for_vin> getblock(const block_data &block, int32_t verbosity = 2) = 0;
   virtual btc_tx getrawtransaction(const std::string &txid, const bool verbose = false) = 0;
   virtual void getnetworkinfo() = 0;
   virtual std::string getblockchaininfo() = 0;
   virtual std::vector<btc_txout> listunspent_by_address_and_amount(const std::string &address, double transfer_amount, const uint32_t minconf = 1, const uint32_t maxconf = 9999999) = 0;
   virtual std::string sendrawtransaction(const std::string &tx_hex) = 0;
   virtual void importmulti(const std::vector<multi_params> &address_or_script_array, const bool rescan = true) {
      ;
   };
   virtual std::string loadwallet(const std::string &filename) {
      return "";
   };
   virtual std::string walletlock() {
      return "";
   };
   virtual bool walletpassphrase(const std::string &passphrase, uint32_t timeout = 60) {
      return false;
   };

   void import_trx_to_memory_pool(const libbitcoin::chain::transaction &trx) {
      std::unique_lock<std::mutex> lck(libbitcoin_event_mutex);
      if (trx_memory_pool.size() < MAX_TRXS_IN_MEMORY_POOL) {
         trx_memory_pool.emplace_back(trx);
      }
   }

protected:
   std::vector<libbitcoin::chain::transaction> trx_memory_pool;
   std::mutex libbitcoin_event_mutex;
};

class bitcoin_rpc_client : public bitcoin_client_base, public rpc_client {
public:
public:
   bitcoin_rpc_client(const std::vector<rpc_credentials> &_credentials, bool _debug_rpc_calls, bool _simulate_connection_reselection);

   uint64_t estimatesmartfee(uint16_t conf_target = 1);
   std::vector<info_for_vin> getblock(const block_data &block, int32_t verbosity = 2);
   btc_tx getrawtransaction(const std::string &txid, const bool verbose = false);
   void getnetworkinfo();
   std::string getblockchaininfo();
   void importmulti(const std::vector<multi_params> &address_or_script_array, const bool rescan = true);
   std::vector<btc_txout> listunspent(const uint32_t minconf = 1, const uint32_t maxconf = 9999999);
   std::vector<btc_txout> listunspent_by_address_and_amount(const std::string &address, double transfer_amount, const uint32_t minconf = 1, const uint32_t maxconf = 9999999);
   std::string loadwallet(const std::string &filename);
   std::string sendrawtransaction(const std::string &tx_hex);
   std::string walletlock();
   bool walletpassphrase(const std::string &passphrase, uint32_t timeout = 60);

   virtual uint64_t ping(rpc_connection &conn) const override;

private:
   std::string ip;
   std::string user;
   std::string password;
   std::string wallet_name;
   std::string wallet_password;
   uint32_t bitcoin_major_version;
};

class bitcoin_libbitcoin_client : public bitcoin_client_base, public libbitcoin_client {
public:
   bitcoin_libbitcoin_client(std::string url);
   uint64_t estimatesmartfee(uint16_t conf_target = 1);
   std::vector<info_for_vin> getblock(const block_data &block, int32_t verbosity = 2);
   btc_tx getrawtransaction(const std::string &txid, const bool verbose = false);
   void getnetworkinfo();
   std::string getblockchaininfo();
   std::vector<btc_txout> listunspent_by_address_and_amount(const std::string &address, double transfer_amount, const uint32_t minconf = 1, const uint32_t maxconf = 9999999);
   std::string sendrawtransaction(const std::string &tx_hex);

private:
   bool is_test_net = false;
   std::unique_ptr<estimate_fee_external> estimate_fee_ext;
   uint64_t current_internal_fee = DEAFULT_LIBBITCOIN_TRX_FEE;
};

// =============================================================================

class zmq_listener_base {
public:
   virtual ~zmq_listener_base(){};
   zmq_listener_base(std::string _ip, uint32_t _block_zmq_port, uint32_t _trx_zmq_port = 0) {
      ip = _ip;
      block_zmq_port = _block_zmq_port;
      trx_zmq_port = _trx_zmq_port;
      stopped = false;
   };
   virtual void start() = 0;
   boost::signals2::signal<void(const block_data &)> block_event_received;
   boost::signals2::signal<void(const libbitcoin::chain::transaction &)> trx_event_received;

protected:
   std::string ip;
   uint32_t block_zmq_port;
   uint32_t trx_zmq_port;
   std::atomic_bool stopped;
   std::thread block_thr;
   std::thread trx_thr;
};

class zmq_listener : public zmq_listener_base {
public:
   zmq_listener(std::string _ip, uint32_t _block_zmq_port, uint32_t _trx_zmq_port = 0);
   virtual ~zmq_listener();
   void start();

private:
   void handle_zmq();
   std::vector<zmq::message_t> receive_multipart();

   zmq::context_t ctx;
   zmq::socket_t socket;
};

class zmq_listener_libbitcoin : public zmq_listener_base {
public:
   zmq_listener_libbitcoin(std::string _ip, uint32_t _block_zmq_port = 9093, uint32_t _trx_zmq_port = 9094);
   virtual ~zmq_listener_libbitcoin();
   void start();

private:
   void handle_block();
   void handle_trx();

   libbitcoin::protocol::zmq::context block_context;
   libbitcoin::protocol::zmq::socket block_socket;
   libbitcoin::protocol::zmq::poller block_poller;
   libbitcoin::protocol::zmq::context trx_context;
   libbitcoin::protocol::zmq::socket trx_socket;
   libbitcoin::protocol::zmq::poller trx_poller;
   libbitcoin::protocol::zmq::poller common_poller;
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
   virtual optional<asset> estimate_withdrawal_transaction_fee() const override;

private:
   std::vector<rpc_credentials> _rpc_credentials;
   std::string libbitcoin_server_ip;
   uint32_t libbitcoin_block_zmq_port;
   uint32_t libbitcoin_trx_zmq_port;
   uint32_t bitcoin_node_zmq_port;
   std::string wallet_name;
   std::string wallet_password;

   std::unique_ptr<bitcoin_client_base> bitcoin_client;
   std::unique_ptr<zmq_listener_base> listener;

   fc::future<void> on_changed_objects_task;

   bitcoin::bitcoin_address::network network_type;
   uint32_t bitcoin_major_version;

   std::mutex event_handler_mutex;
   typedef std::lock_guard<decltype(event_handler_mutex)> scoped_lock;

   std::string create_primary_wallet_address(const std::vector<son_sidechain_info> &son_pubkeys);

   std::string create_primary_wallet_transaction(const son_wallet_object &prev_swo, std::string new_sw_address);
   std::string create_deposit_transaction(const son_wallet_deposit_object &swdo);
   std::string create_withdrawal_transaction(const son_wallet_withdraw_object &swwo);

   std::string create_transaction(const std::vector<btc_txout> &inputs, const fc::flat_map<std::string, double> outputs, std::string &redeem_script);
   std::string sign_transaction(const sidechain_transaction_object &sto);
   std::string send_transaction(const sidechain_transaction_object &sto);

   void block_handle_event(const block_data &event_data);
   void trx_handle_event(const libbitcoin::chain::transaction &event_data);
   std::string get_redeemscript_for_userdeposit(const std::string &user_address);
   void on_changed_objects(const vector<object_id_type> &ids, const flat_set<account_id_type> &accounts);
   void on_changed_objects_cb(const vector<object_id_type> &ids, const flat_set<account_id_type> &accounts);
};

}} // namespace graphene::peerplays_sidechain
