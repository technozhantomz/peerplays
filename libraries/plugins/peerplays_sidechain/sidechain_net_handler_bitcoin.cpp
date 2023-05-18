#include <graphene/peerplays_sidechain/sidechain_net_handler_bitcoin.hpp>

#include <algorithm>

#include <boost/algorithm/hex.hpp>
#include <boost/property_tree/json_parser.hpp>
#include <boost/property_tree/ptree.hpp>

#include <fc/crypto/base64.hpp>
#include <fc/log/logger.hpp>
#include <fc/network/ip.hpp>

#include <graphene/chain/account_object.hpp>
#include <graphene/chain/protocol/son_wallet.hpp>
#include <graphene/chain/sidechain_transaction_object.hpp>
#include <graphene/chain/son_sidechain_info.hpp>
#include <graphene/chain/son_wallet_object.hpp>
#include <graphene/peerplays_sidechain/bitcoin/bitcoin_address.hpp>
#include <graphene/peerplays_sidechain/bitcoin/bitcoin_transaction.hpp>
#include <graphene/peerplays_sidechain/bitcoin/serialize.hpp>
#include <graphene/peerplays_sidechain/bitcoin/sign_bitcoin_transaction.hpp>
#include <graphene/utilities/key_conversion.hpp>

namespace graphene { namespace peerplays_sidechain {

// =============================================================================

bitcoin_rpc_client::bitcoin_rpc_client(std::string _url, std::string _user, std::string _password, bool _debug_rpc_calls) :
      rpc_client(_url, _user, _password, _debug_rpc_calls) {
}

uint64_t bitcoin_rpc_client::estimatesmartfee(uint16_t conf_target) {
   const std::string params = std::string("[") + std::to_string(conf_target) + std::string("]");
   const std::string str = send_post_request("estimatesmartfee", params, debug_rpc_calls);

   if (str.length() == 0) {
      wlog("Bitcoin RPC call ${function} failed", ("function", __FUNCTION__));
      return 0;
   }

   std::stringstream ss(str);
   boost::property_tree::ptree json;
   boost::property_tree::read_json(ss, json);
   if (json.find("result") != json.not_found()) {
      auto json_result = json.get_child("result");
      if (json_result.find("feerate") != json_result.not_found()) {
         auto feerate_str = json_result.get<std::string>("feerate");
         feerate_str.erase(std::remove(feerate_str.begin(), feerate_str.end(), '.'), feerate_str.end());
         return std::stoll(feerate_str);
      }
   }

   return 20000;
}

std::vector<info_for_vin> bitcoin_rpc_client::getblock(const block_data &block, int32_t verbosity) {
   std::string params = std::string("[\"") + block.block_hash + std::string("\",") + std::to_string(verbosity) + std::string("]");
   std::string str = send_post_request("getblock", params, debug_rpc_calls);
   std::vector<info_for_vin> result;

   if (str.empty()) {
      return result;
   }

   std::stringstream ss(str);
   boost::property_tree::ptree json;
   boost::property_tree::read_json(ss, json);

   auto json_result = json.get_child_optional("result");

   for (const auto &tx_child : json_result.get().get_child("tx")) {
      const auto &tx = tx_child.second;

      for (const auto &o : tx.get_child("vout")) {
         const auto script = o.second.get_child("scriptPubKey");
         std::vector<std::string> address_list;

         if (script.count("address")) {
            address_list.emplace_back(script.get<std::string>("address"));
         } else if (script.count("addresses")) {
            for (const auto &addr : script.get_child("addresses")) {
               address_list.emplace_back(addr.second.get_value<std::string>());
            }
         } else {
            continue;
         }

         for (auto &address : address_list) {
            const auto address_base58 = address;
            info_for_vin vin;
            vin.out.hash_tx = tx.get_child("txid").get_value<std::string>();
            string amount = o.second.get_child("value").get_value<std::string>();
            amount.erase(std::remove(amount.begin(), amount.end(), '.'), amount.end());
            vin.out.amount = std::stoll(amount);
            vin.out.n_vout = o.second.get_child("n").get_value<uint32_t>();
            vin.address = address_base58;
            result.push_back(vin);
         }
      }
   }

   return result;
}

void bitcoin_rpc_client::getnetworkinfo() {
   std::string params = std::string("[]");
   std::string str = send_post_request("getnetworkinfo", params, debug_rpc_calls);

   std::stringstream network_info_ss(str);
   boost::property_tree::ptree network_info_json;
   boost::property_tree::read_json(network_info_ss, network_info_json);

   bitcoin_major_version = network_info_json.get<uint32_t>("result.version") / 10000;
   ilog("Bitcoin major version is: '${version}'", ("version", bitcoin_major_version));
}

btc_tx bitcoin_rpc_client::getrawtransaction(const std::string &txid, const bool verbose) {
   std::string params = std::string("[\"") + txid + std::string("\",") + (verbose ? "true" : "false") + std::string("]");
   std::string str = send_post_request("getrawtransaction", params, debug_rpc_calls);

   btc_tx tx;

   std::stringstream tx_ss(str);
   boost::property_tree::ptree tx_json;
   boost::property_tree::read_json(tx_ss, tx_json);

   if (tx_json.count("error") && tx_json.get_child("error").empty()) {

      std::string tx_txid = tx_json.get<std::string>("result.txid");
      uint32_t tx_confirmations = tx_json.get<uint32_t>("result.confirmations");

      tx.tx_txid = tx_txid;
      tx.tx_confirmations = tx_confirmations;

      for (auto &input : tx_json.get_child("result.vout")) {
         btc_txin tx_in;
         std::string tx_vout_s = input.second.get<std::string>("n");
         tx_in.tx_vout = std::stoll(tx_vout_s);
         if (bitcoin_major_version > 21) {
            std::string address = input.second.get<std::string>("scriptPubKey.address");
            tx_in.tx_address.emplace_back(address);
         } else {
            for (auto &address : input.second.get_child("scriptPubKey.addresses")) {
               tx_in.tx_address.emplace_back(address.second.data());
            }
         }

         std::string tx_amount_s = input.second.get<std::string>("value");
         tx_amount_s.erase(std::remove(tx_amount_s.begin(), tx_amount_s.end(), '.'), tx_amount_s.end());
         tx_in.tx_amount = std::stoll(tx_amount_s);

         tx.tx_in_list.emplace_back(tx_in);
      }
   }

   return tx;
}

std::string bitcoin_rpc_client::getblockchaininfo() {
   static const std::string params = std::string("[]");
   const std::string str = send_post_request("getblockchaininfo", params, debug_rpc_calls);

   std::string result;

   if (str.length() > 0) {

      std::stringstream ss(str);
      boost::property_tree::ptree json;
      boost::property_tree::read_json(ss, json);

      if (json.find("result") != json.not_found()) {
         auto json_result = json.get_child("result");
         if (json_result.count("chain")) {
            result = json_result.get<std::string>("chain");
         }
      }
   }

   return result;
}

void bitcoin_rpc_client::importmulti(const std::vector<multi_params> &address_or_script_array, const bool rescan) {
   std::string params = std::string("[");
   std::string argument_1 = "[";
   for (const auto &param : address_or_script_array) {
      argument_1 += "{\"scriptPubKey\": ";
      if (param.type == multi_type::address) {
         argument_1 += "{\"address\": \"" + param.address_or_script + "\"}, \"label\": \"" + param.label + "\"";
      } else if (param.type == multi_type::script) {
         argument_1 += "\"" + param.address_or_script + "\", \"internal\": true";
      } else {
         FC_THROW("Invalid multi_type.");
      }
      argument_1 += ", \"timestamp\": " + std::to_string(fc::time_point_sec::from_iso_string("2022-01-01T00:00:00").sec_since_epoch()) + "}";

      //! Note
      /* Creation time of the key expressed in UNIX epoch time,
         or the string "now" to substitute the current synced blockchain time. The timestamp of the oldest
         key will determine how far back blockchain rescans need to begin for missing wallet transactions.
         "now" can be specified to bypass scanning, for keys which are known to never have been used, and
         0 can be specified to scan the entire blockchain. Blocks up to 2 hours before the earliest key
         creation time of all keys being imported by the importmulti call will be scanned.*/

      if (&param != &address_or_script_array.back()) {
         argument_1 += ", ";
      }
   }
   argument_1 += "]";

   std::string argument_2 = std::string{"{\"rescan\": "} + (rescan ? "true" : "false") + "}";
   params += argument_1 + "," + argument_2 + "]";

   send_post_request("importmulti", params, debug_rpc_calls);
}

std::vector<btc_txout> bitcoin_rpc_client::listunspent(const uint32_t minconf, const uint32_t maxconf) {
   std::vector<btc_txout> result;
   const std::string params = std::string("[") + std::to_string(minconf) + "," + std::to_string(maxconf) + std::string("]");
   const std::string str = send_post_request("listunspent", params, debug_rpc_calls);

   if (str.length() == 0) {
      wlog("Bitcoin RPC call ${function} failed", ("function", __FUNCTION__));
      return result;
   }

   std::stringstream ss(str);
   boost::property_tree::ptree json;
   boost::property_tree::read_json(ss, json);

   if (json.count("result")) {
      for (auto &entry : json.get_child("result")) {
         btc_txout txo;
         txo.txid_ = entry.second.get_child("txid").get_value<std::string>();
         txo.out_num_ = entry.second.get_child("vout").get_value<unsigned int>();
         string amount = entry.second.get_child("amount").get_value<std::string>();
         amount.erase(std::remove(amount.begin(), amount.end(), '.'), amount.end());
         txo.amount_ = std::stoll(amount);
         result.push_back(txo);
      }
   }

   return result;
}

std::vector<btc_txout> bitcoin_rpc_client::listunspent_by_address_and_amount(const std::string &address, double minimum_amount, const uint32_t minconf, const uint32_t maxconf) {

   std::string params = std::string("[") + std::to_string(minconf) + "," + std::to_string(maxconf) + ",";
   params += std::string("[\"");
   params += address;
   params += std::string("\"],true,{\"minimumAmount\":");
   params += std::to_string(minimum_amount);
   params += std::string("} ]");

   std::vector<btc_txout> result;
   const std::string str = send_post_request("listunspent", params, debug_rpc_calls);

   if (str.length() == 0) {
      wlog("Bitcoin RPC call ${function} failed", ("function", __FUNCTION__));
      return result;
   }

   std::stringstream ss(str);
   boost::property_tree::ptree json;
   boost::property_tree::read_json(ss, json);

   if (json.count("result")) {
      for (auto &entry : json.get_child("result")) {
         btc_txout txo;
         txo.txid_ = entry.second.get_child("txid").get_value<std::string>();
         txo.out_num_ = entry.second.get_child("vout").get_value<unsigned int>();
         string amount = entry.second.get_child("amount").get_value<std::string>();
         amount.erase(std::remove(amount.begin(), amount.end(), '.'), amount.end());
         txo.amount_ = std::stoll(amount);
         result.push_back(txo);
      }
   }

   return result;
}

std::string bitcoin_rpc_client::loadwallet(const std::string &filename) {
   const std::string params = std::string("[\"") + filename + std::string("\"]");
   return send_post_request("loadwallet", params, debug_rpc_calls);
}

std::string bitcoin_rpc_client::sendrawtransaction(const std::string &tx_hex) {
   const std::string params = std::string("[\"") + tx_hex + std::string("\"]");
   const std::string str = send_post_request("sendrawtransaction", params, debug_rpc_calls);

   if (str.length() > 0) {
      std::stringstream ss(str);
      boost::property_tree::ptree json;
      boost::property_tree::read_json(ss, json);

      if (json.count("error") && !json.get_child("error").empty()) {
         if (!json.count("error.code")) {
            if (json.get_child("error.code").get_value<int16_t>() == -27) {
               return tx_hex;
            }
         }
      }

      return json.get<std::string>("result");
   }

   return str;
}

std::string bitcoin_rpc_client::walletlock() {
   static const std::string params = std::string("[]");
   const std::string str = send_post_request("walletlock", params, debug_rpc_calls);

   if (str.length() == 0) {
      wlog("Bitcoin RPC call ${function} failed", ("function", __FUNCTION__));
      return "";
   }

   std::stringstream ss(str);
   boost::property_tree::ptree json;
   boost::property_tree::read_json(ss, json);

   ss.clear();
   boost::property_tree::json_parser::write_json(ss, json.get_child("result"));
   return ss.str();
}

bool bitcoin_rpc_client::walletpassphrase(const std::string &passphrase, uint32_t timeout) {
   const std::string params = std::string("[\"") + passphrase + std::string("\",") + std::to_string(timeout) + std::string("]");
   const std::string str = send_post_request("walletpassphrase", params, debug_rpc_calls);
   if (str.length() == 0)
      return false;
   else
      return true;
}
bitcoin_libbitcoin_client::bitcoin_libbitcoin_client(std::string url) :
      libbitcoin_client(url) {

   estimate_fee_ext = std::unique_ptr<estimate_fee_external>(new estimate_fee_external());
}

uint64_t bitcoin_libbitcoin_client::estimatesmartfee(uint16_t conf_target) {
   std::vector<std::pair<std::string, uint64_t>> fees = estimate_fee_ext->get_fee_external(conf_target);
   std::vector<uint64_t> accumulated_fees;
   for (auto &external_fees : fees) {
      if (external_fees.second != 0) {
         accumulated_fees.emplace_back(external_fees.second);
      }
   }

   // we will rather pick minimal fee from external sources than internal fee calculation
   if (accumulated_fees.empty()) {
      accumulated_fees.emplace_back(current_internal_fee);
   }

   return *std::min_element(accumulated_fees.begin(), accumulated_fees.end());
}

std::vector<info_for_vin> bitcoin_libbitcoin_client::getblock(const block_data &block, int32_t verbosity) {

   std::unique_lock<std::mutex> lck(libbitcoin_event_mutex);

   wlog("RECEIVED BLOCK FROM LIBBITCOIN : ${block_hash}", ("block_hash" , block.block_hash));
   
   // estimate fee
   const auto &block_trxs = block.block.transactions();
   std::vector<libbitcoin::chain::transaction> bucket_trxs;
   for (auto &mem_pool_trx : trx_memory_pool) {
      for (auto &trx : block_trxs) {
         if (mem_pool_trx.hash() == trx.hash()) {
            bucket_trxs.emplace_back(mem_pool_trx);
            break;
         }
      }
   }

   uint64_t average_fee = get_average_fee_from_trxs(bucket_trxs);
   if (average_fee > 0 && bucket_trxs.size() >= MIN_TRXS_IN_BUCKET) {
      current_internal_fee = average_fee;
   }

   // We could consider accumulation which could spread to multiple blocks for better metric
   // for now we only keep tracking for not confirmed transaction until we get next block
   trx_memory_pool.clear();

   std::vector<info_for_vin> result;

   const libbitcoin::chain::transaction::list trx_list = block.block.transactions();

   for (const auto &tx : trx_list) {
      uint32_t vout_seq = 0;
      for (const auto &o : tx.outputs()) {
         std::vector<std::string> address_list;

         libbitcoin::wallet::payment_address::list addresses;
         if (is_test_net) {
            addresses = o.addresses(libbitcoin::wallet::payment_address::testnet_p2kh,
                                    libbitcoin::wallet::payment_address::testnet_p2sh);
         } else {
            addresses = o.addresses();
         }

         for (auto &payment_address : addresses) {
            std::stringstream ss;
            ss << payment_address;
            address_list.emplace_back(ss.str());
         }

         // addres list consists usual of one element
         for (auto &address : address_list) {
            const auto address_base58 = address;
            info_for_vin vin;
            vin.out.hash_tx = libbitcoin::config::hash256(tx.hash()).to_string();
            vin.out.amount = std::floor(o.value());
            vin.out.n_vout = vout_seq;
            vin.address = address_base58;
            result.push_back(vin);
         }
         vout_seq++;
      }
   }

   return result;
}

btc_tx bitcoin_libbitcoin_client::getrawtransaction(const std::string &txid, const bool verbose) {
   btc_tx tx;

   std::string tx_hash;
   uint32_t confirmitions;

   libbitcoin::chain::output::list outs = get_transaction(txid, tx_hash, confirmitions);

   if (tx_hash.empty()) {
      return tx;
   }

   tx.tx_txid = tx_hash;
   tx.tx_confirmations = confirmitions;

   uint64_t tx_vout_sequence = 0;

   for (auto &out : outs) {
      btc_txin tx_in;
      tx_in.tx_vout = tx_vout_sequence++;

      libbitcoin::wallet::payment_address::list addresses;
      if (is_test_net) {
         addresses = out.addresses(libbitcoin::wallet::payment_address::testnet_p2kh,
                                   libbitcoin::wallet::payment_address::testnet_p2sh);
      } else {
         addresses = out.addresses();
      }

      for (auto &address : addresses) {

         std::stringstream ss;
         ss << address;
         tx_in.tx_address.emplace_back(ss.str());
      }

      tx_in.tx_amount = std::floor(out.value());
      tx.tx_in_list.emplace_back(tx_in);
   }

   return tx;
}

void bitcoin_libbitcoin_client::getnetworkinfo() {
   // This function is only used for bitcoind client in order of getting
   // version of bitcoin client. Version is used for extracting addresses or address
   // which is not important for libbitcoin client
}

std::string bitcoin_libbitcoin_client::getblockchaininfo() {
   if (get_is_test_net()) {
      is_test_net = true;
      return "regtest";
   }

   return "";
}

std::vector<btc_txout> bitcoin_libbitcoin_client::listunspent_by_address_and_amount(const std::string &address, double transfer_amount, const uint32_t minconf, const uint32_t maxconf) {

   std::vector<btc_txout> result;
   std::vector<list_unspent_replay> outputs = listunspent(address, transfer_amount);
   for (auto &output : outputs) {
      btc_txout txo;
      txo.txid_ = output.hash;
      txo.out_num_ = output.index;
      txo.amount_ = output.value;
      result.push_back(txo);
   }

   return result;
}

std::string bitcoin_libbitcoin_client::sendrawtransaction(const std::string &tx_hex) {
   std::string res = send_transaction(tx_hex);
   return res;
}

// =============================================================================

zmq_listener::zmq_listener(std::string _ip, uint32_t _zmq_block_port, uint32_t _zmq_trx_port) :
      zmq_listener_base(_ip, _zmq_block_port, _zmq_trx_port),
      ctx(1),
      socket(ctx, ZMQ_SUB) {
}

void zmq_listener::start() {
   int linger = 0;
   auto rc = zmq_setsockopt(socket, ZMQ_SUBSCRIBE, "hashblock", 9);
   FC_ASSERT(0 == rc);
   rc = zmq_setsockopt(socket, ZMQ_LINGER, &linger, sizeof(linger));
   FC_ASSERT(0 == rc);
   int timeout = 100; // millisec
   rc = zmq_setsockopt(socket, ZMQ_RCVTIMEO, &timeout, sizeof(timeout));
   // socket.setsockopt( ZMQ_SUBSCRIBE, "hashtx", 6 );
   // socket.setsockopt( ZMQ_SUBSCRIBE, "rawblock", 8 );
   // socket.setsockopt( ZMQ_SUBSCRIBE, "rawtx", 5 );
   socket.connect("tcp://" + ip + ":" + std::to_string(block_zmq_port));

   block_thr = std::thread(&zmq_listener::handle_zmq, this);

   ilog("zmq_listener thread started");
}

zmq_listener::~zmq_listener() {
   stopped = true;
   block_thr.join();
}

std::vector<zmq::message_t> zmq_listener::receive_multipart() {
   std::vector<zmq::message_t> msgs;

   auto res = zmq::recv_multipart(socket, std::back_inserter(msgs));
   FC_ASSERT(res);
   if (3 != *res) {
      elog("zmq::recv_multipart returned: ${res}", ("res", *res));
      throw zmq::error_t();
   }

   return msgs;
}

void zmq_listener::handle_zmq() {
   while (false == stopped) {
      try {
         std::vector<zmq::message_t> msg;
         auto res = zmq::recv_multipart(socket, std::back_inserter(msg));
         if (res.has_value()) {
            if (3 != *res) {
               elog("zmq::recv_multipart returned: ${res}", ("res", *res));
               throw zmq::error_t();
            }
            const auto header = std::string(static_cast<char *>(msg[0].data()), msg[0].size());
            const auto block_hash = boost::algorithm::hex(std::string(static_cast<char *>(msg[1].data()), msg[1].size()));
            block_data event_data;
            event_data.block_hash = block_hash;
            block_event_received(event_data);
         }
      } catch (zmq::error_t &e) {
         elog("handle_zmq recv_multipart exception ${str}", ("str", e.what()));
      }
   }

   ilog("zmq_listener thread finished");
}

// =============================================================================

// =============================================================================

zmq_listener_libbitcoin::zmq_listener_libbitcoin(std::string _ip, uint32_t _block_zmq_port, uint32_t _trx_zmq_port) :
      zmq_listener_base(_ip, _block_zmq_port, _trx_zmq_port),
      block_socket(block_context, libbitcoin::protocol::zmq::socket::role::subscriber),
      trx_socket(trx_context, libbitcoin::protocol::zmq::socket::role::subscriber) {
}

zmq_listener_libbitcoin::~zmq_listener_libbitcoin() {
   stopped.store(true);
   trx_socket.stop();
   block_socket.stop();
   block_thr.join();
   trx_thr.join();
}

void zmq_listener_libbitcoin::start() {
   std::string endpoint_address = "tcp://" + ip;

   libbitcoin::config::endpoint block_address(endpoint_address, block_zmq_port);
   libbitcoin::config::endpoint trx_address(endpoint_address, trx_zmq_port);

   block_socket.connect(block_address);
   trx_socket.connect(trx_address);

   common_poller.add(block_socket);
   common_poller.add(trx_socket);

   // block_thr = std::thread(&zmq_listener_libbitcoin::handle_block, this);
   trx_thr = std::thread(&zmq_listener_libbitcoin::handle_trx, this);
}

void zmq_listener_libbitcoin::handle_trx() {
   // trx_poller.add(trx_socket);

   while (!stopped.load()) {
      const auto identifiers = common_poller.wait(500);

      if (identifiers.contains(trx_socket.id())) {
         libbitcoin::protocol::zmq::message message;

         trx_socket.receive(message);

         std::vector<uint8_t> data;
         for (int i = 0; i < 2; i++) {
            data.clear();
            message.dequeue(data);
         }

         libbitcoin::chain::transaction trx;
         trx.from_data(data, true);

         trx_event_received(trx);
      } else if (identifiers.contains(block_socket.id())) {
         libbitcoin::protocol::zmq::message message;
         block_socket.receive(message);

         std::vector<uint8_t> data;
         for (int i = 0; i < 3; i++) {
            data.clear();
            message.dequeue(data);
         }

         libbitcoin::chain::block block;
         block.from_data(data, true);

         block_data event_data;
         event_data.block_hash = libbitcoin::config::hash256(block.hash()).to_string();
         event_data.block = std::move(block);
         block_event_received(event_data);
      }
   }

   ilog("zmq_listener_libbitcoin trx thread finished");
}

void zmq_listener_libbitcoin::handle_block() {
   // block_poller.add(block_socket);

   while (!stopped.load()) {
      const auto identifiers = common_poller.wait(500);

      if (identifiers.contains(block_socket.id())) {
         libbitcoin::protocol::zmq::message message;
         block_socket.receive(message);

         std::vector<uint8_t> data;
         for (int i = 0; i < 3; i++) {
            data.clear();
            message.dequeue(data);
         }

         libbitcoin::chain::block block;
         block.from_data(data, true);

         block_data event_data;
         event_data.block_hash = libbitcoin::config::hash256(block.hash()).to_string();
         event_data.block = std::move(block);
         block_event_received(event_data);
      }
   }

   ilog("zmq_listener_libbitcoin block thread finished");
}

// =============================================================================

sidechain_net_handler_bitcoin::sidechain_net_handler_bitcoin(peerplays_sidechain_plugin &_plugin, const boost::program_options::variables_map &options) :
      sidechain_net_handler(_plugin, options) {
   sidechain = sidechain_type::bitcoin;

   if (options.count("debug-rpc-calls")) {
      debug_rpc_calls = options.at("debug-rpc-calls").as<bool>();
   }

   if (options.count("use-bitcoind-client")) {
      use_bitcoind_client = options.at("use-bitcoind-client").as<bool>();
   }

   libbitcoin_server_ip = options.at("libbitcoin-server-ip").as<std::string>();
   libbitcoin_block_zmq_port = options.at("libbitcoin-server-block-zmq-port").as<uint32_t>();
   libbitcoin_trx_zmq_port = options.at("libbitcoin-server-trx-zmq-port").as<uint32_t>();

   bitcoin_node_ip = options.at("bitcoin-node-ip").as<std::string>();
   bitcoin_node_zmq_port = options.at("bitcoin-node-zmq-port").as<uint32_t>();
   rpc_port = options.at("bitcoin-node-rpc-port").as<uint32_t>();
   rpc_user = options.at("bitcoin-node-rpc-user").as<std::string>();
   rpc_password = options.at("bitcoin-node-rpc-password").as<std::string>();
   wallet_name = "";
   if (options.count("bitcoin-wallet-name")) {
      wallet_name = options.at("bitcoin-wallet-name").as<std::string>();
   }
   wallet_password = "";
   if (options.count("bitcoin-wallet-password")) {
      wallet_password = options.at("bitcoin-wallet-password").as<std::string>();
   }

   if (options.count("bitcoin-private-key")) {
      const std::vector<std::string> pub_priv_keys = options["bitcoin-private-key"].as<std::vector<std::string>>();
      for (const std::string &itr_key_pair : pub_priv_keys) {
         auto key_pair = graphene::app::dejsonify<std::pair<std::string, std::string>>(itr_key_pair, 5);
         ilog("Bitcoin Public Key: ${public}", ("public", key_pair.first));
         if (!key_pair.first.length() || !key_pair.second.length()) {
            FC_THROW("Invalid public private key pair.");
         }
         private_keys[key_pair.first] = key_pair.second;
      }
   }

   if (use_bitcoind_client) {
      std::string url = bitcoin_node_ip + ":" + std::to_string(rpc_port);
      if (!wallet_name.empty()) {
         url = url + "/wallet/" + wallet_name;
      }
      bitcoin_client = std::unique_ptr<bitcoin_rpc_client>(new bitcoin_rpc_client(url, rpc_user, rpc_password, debug_rpc_calls));
      if (!wallet_name.empty()) {
         bitcoin_client->loadwallet(wallet_name);
      }

      listener = std::unique_ptr<zmq_listener>(new zmq_listener(bitcoin_node_ip, bitcoin_node_zmq_port));

   } else {
      bitcoin_client = std::unique_ptr<bitcoin_libbitcoin_client>(new bitcoin_libbitcoin_client(libbitcoin_server_ip));

      listener = std::unique_ptr<zmq_listener_libbitcoin>(new zmq_listener_libbitcoin(libbitcoin_server_ip, libbitcoin_block_zmq_port, libbitcoin_trx_zmq_port));
   }

   std::string chain_info = bitcoin_client->getblockchaininfo();

   using namespace bitcoin;
   network_type = bitcoin_address::network::mainnet;

   if (chain_info == "test") {
      network_type = bitcoin_address::network::testnet;
   } else if (chain_info == "regtest") {
      network_type = bitcoin_address::network::regtest;
   }

   bitcoin_client->getnetworkinfo();

   listener->start();
   listener->block_event_received.connect([this](const block_data &block_event_data) {
      std::thread(&sidechain_net_handler_bitcoin::block_handle_event, this, block_event_data).detach();
   });

   listener->trx_event_received.connect([this](const libbitcoin::chain::transaction &trx_event_data) {
      std::thread(&sidechain_net_handler_bitcoin::trx_handle_event, this, trx_event_data).detach();
   });

   database.changed_objects.connect([this](const vector<object_id_type> &ids, const flat_set<account_id_type> &accounts) {
      on_changed_objects(ids, accounts);
   });
}

sidechain_net_handler_bitcoin::~sidechain_net_handler_bitcoin() {
   try {
      if (on_changed_objects_task.valid()) {
         on_changed_objects_task.cancel_and_wait(__FUNCTION__);
      }
   } catch (fc::canceled_exception &) {
      // Expected exception. Move along.
   } catch (fc::exception &e) {
      edump((e.to_detail_string()));
   }
}

bool sidechain_net_handler_bitcoin::process_proposal(const proposal_object &po) {

   // ilog("Proposal to process: ${po}, SON id ${son_id}", ("po", po.id)("son_id", plugin.get_current_son_id(sidechain)));

   bool should_approve = false;

   const chain::global_property_object &gpo = database.get_global_properties();

   int32_t op_idx_0 = -1;
   chain::operation op_obj_idx_0;

   if (po.proposed_transaction.operations.size() >= 1) {
      op_idx_0 = po.proposed_transaction.operations[0].which();
      op_obj_idx_0 = po.proposed_transaction.operations[0];
   }

   int32_t op_idx_1 = -1;
   chain::operation op_obj_idx_1;
   (void)op_idx_1;

   if (po.proposed_transaction.operations.size() >= 2) {
      op_idx_1 = po.proposed_transaction.operations[1].which();
      op_obj_idx_1 = po.proposed_transaction.operations[1];
   }

   switch (op_idx_0) {

   case chain::operation::tag<chain::son_wallet_update_operation>::value: {
      bool address_ok = false;
      bool transaction_ok = false;
      std::string new_pw_address = "";
      son_wallet_id_type swo_id = op_obj_idx_0.get<son_wallet_update_operation>().son_wallet_id;
      const auto ast = active_sidechain_types(database.head_block_time());
      const auto id = (swo_id.instance.value - std::distance(ast.begin(), ast.find(sidechain))) / ast.size();
      const son_wallet_id_type op_id{id};
      const auto &idx = database.get_index_type<son_wallet_index>().indices().get<by_id>();
      const auto swo = idx.find(op_id);
      if (swo != idx.end()) {

         const auto &active_sons = gpo.active_sons.at(sidechain);
         const auto &wallet_sons = swo->sons.at(sidechain);

         bool son_sets_equal = (active_sons.size() == wallet_sons.size());

         if (son_sets_equal) {
            for (size_t i = 0; i < active_sons.size(); i++) {
               son_sets_equal = son_sets_equal && active_sons.at(i) == wallet_sons.at(i);
            }
         }

         if (son_sets_equal) {
            const auto &active_sons = gpo.active_sons.at(sidechain);
            vector<string> son_pubkeys_bitcoin;
            for (const auto &si : active_sons) {
               son_pubkeys_bitcoin.push_back(si.public_key);
            }

            string reply_str = create_primary_wallet_address(active_sons);

            std::stringstream active_pw_ss(reply_str);
            boost::property_tree::ptree active_pw_pt;
            boost::property_tree::read_json(active_pw_ss, active_pw_pt);
            if (active_pw_pt.count("error") && active_pw_pt.get_child("error").empty()) {
               std::stringstream res;
               boost::property_tree::json_parser::write_json(res, active_pw_pt.get_child("result"));
               new_pw_address = active_pw_pt.get<std::string>("result.address");

               address_ok = (op_obj_idx_0.get<son_wallet_update_operation>().address == res.str());
            }
         }

         if (po.proposed_transaction.operations.size() >= 2) {
            const object_id_type object_id = op_obj_idx_1.get<sidechain_transaction_create_operation>().object_id;
            const auto ast = active_sidechain_types(database.head_block_time());
            const auto id = (object_id.instance() - std::distance(ast.begin(), ast.find(sidechain))) / ast.size();
            const object_id_type obj_id{object_id.space(), object_id.type(), id};
            std::string op_tx_str = op_obj_idx_1.get<sidechain_transaction_create_operation>().transaction;

            const auto &st_idx = database.get_index_type<sidechain_transaction_index>().indices().get<by_object_id>();
            const auto st = st_idx.find(obj_id);
            if (st == st_idx.end()) {

               std::string tx_str = "";

               if (obj_id.is<son_wallet_id_type>()) {
                  const auto &idx = database.get_index_type<son_wallet_index>().indices().get<by_id>();
                  const auto swo = idx.find(obj_id);
                  if (swo != idx.end()) {
                     tx_str = create_primary_wallet_transaction(*swo, new_pw_address);
                  }
               }

               transaction_ok = (op_tx_str == tx_str);
            }
         } else {
            transaction_ok = true;
         }
      }

      should_approve = address_ok &&
                       transaction_ok;
      break;
   }

   case chain::operation::tag<chain::son_wallet_deposit_process_operation>::value: {
      bool process_ok = false;
      bool transaction_ok = false;
      son_wallet_deposit_id_type swdo_id = op_obj_idx_0.get<son_wallet_deposit_process_operation>().son_wallet_deposit_id;
      const auto &idx = database.get_index_type<son_wallet_deposit_index>().indices().get<by_id>();
      const auto swdo = idx.find(swdo_id);
      if (swdo != idx.end()) {

         std::string swdo_txid = swdo->sidechain_transaction_id;
         std::string swdo_address = swdo->sidechain_from;
         uint64_t swdo_amount = swdo->sidechain_amount.value;
         uint64_t swdo_vout = std::stoll(swdo->sidechain_uid.substr(swdo->sidechain_uid.find_last_of("-") + 1));

         btc_tx tx = bitcoin_client->getrawtransaction(swdo_txid, true);

         if (!tx.tx_in_list.empty()) {

            std::string tx_txid = tx.tx_txid;
            uint32_t tx_confirmations = tx.tx_confirmations;
            std::string tx_address = "";
            uint64_t tx_amount = -1;
            uint64_t tx_vout = -1;

            for (auto &input : tx.tx_in_list) {
               tx_vout = input.tx_vout;
               if (tx_vout == swdo_vout) {
                  for (auto &address : input.tx_address) {
                     if (address == swdo_address) {
                        tx_address = address;
                        break;
                     }
                  }
                  tx_amount = input.tx_amount;
                  break;
               }
            }

            process_ok = (swdo_txid == tx_txid) &&
                         (swdo_address == tx_address) &&
                         (swdo_amount == tx_amount) &&
                         (swdo_vout == tx_vout) &&
                         (gpo.parameters.son_bitcoin_min_tx_confirmations() <= tx_confirmations);
         }

         object_id_type object_id = op_obj_idx_1.get<sidechain_transaction_create_operation>().object_id;
         std::string op_tx_str = op_obj_idx_1.get<sidechain_transaction_create_operation>().transaction;

         const auto &st_idx = database.get_index_type<sidechain_transaction_index>().indices().get<by_object_id>();
         const auto st = st_idx.find(object_id);
         if (st == st_idx.end()) {

            std::string tx_str = "";

            if (object_id.is<son_wallet_deposit_id_type>()) {
               const auto &idx = database.get_index_type<son_wallet_deposit_index>().indices().get<by_id>();
               const auto swdo = idx.find(object_id);
               if (swdo != idx.end()) {
                  tx_str = create_deposit_transaction(*swdo);
               }
            }

            transaction_ok = (op_tx_str == tx_str);
         }
      }

      should_approve = process_ok &&
                       transaction_ok;
      break;
   }

   case chain::operation::tag<chain::son_wallet_withdraw_process_operation>::value: {
      bool process_ok = false;
      bool transaction_ok = false;
      son_wallet_withdraw_id_type swwo_id = op_obj_idx_0.get<son_wallet_withdraw_process_operation>().son_wallet_withdraw_id;
      const auto &idx = database.get_index_type<son_wallet_withdraw_index>().indices().get<by_id>();
      const auto swwo = idx.find(swwo_id);
      if (swwo != idx.end()) {

         uint32_t swwo_block_num = swwo->block_num;
         std::string swwo_peerplays_transaction_id = swwo->peerplays_transaction_id;
         uint32_t swwo_op_idx = std::stoll(swwo->peerplays_uid.substr(swwo->peerplays_uid.find_last_of("-") + 1));

         const auto &block = database.fetch_block_by_number(swwo_block_num);

         for (const auto &tx : block->transactions) {
            if (tx.id().str() == swwo_peerplays_transaction_id) {
               operation op = tx.operations[swwo_op_idx];
               transfer_operation t_op = op.get<transfer_operation>();

               price asset_price = database.get<asset_object>(t_op.amount.asset_id).options.core_exchange_rate;
               asset peerplays_asset = asset(t_op.amount.amount * asset_price.base.amount / asset_price.quote.amount);

               process_ok = (t_op.to == gpo.parameters.son_account()) &&
                            (swwo->peerplays_from == t_op.from) &&
                            (swwo->peerplays_asset == peerplays_asset);
               break;
            }
         }

         object_id_type object_id = op_obj_idx_1.get<sidechain_transaction_create_operation>().object_id;
         std::string op_tx_str = op_obj_idx_1.get<sidechain_transaction_create_operation>().transaction;

         const auto &st_idx = database.get_index_type<sidechain_transaction_index>().indices().get<by_object_id>();
         const auto st = st_idx.find(object_id);
         if (st == st_idx.end()) {

            std::string tx_str = "";

            if (object_id.is<son_wallet_withdraw_id_type>()) {
               const auto &idx = database.get_index_type<son_wallet_withdraw_index>().indices().get<by_id>();
               const auto swwo = idx.find(object_id);
               if (swwo != idx.end()) {
                  tx_str = create_withdrawal_transaction(*swwo);
               }
            }

            transaction_ok = (op_tx_str == tx_str);
         }
      }

      should_approve = process_ok &&
                       transaction_ok;
      break;
   }

   case chain::operation::tag<chain::sidechain_transaction_sign_operation>::value: {
      using namespace bitcoin;
      should_approve = true;
      son_id_type signer = op_obj_idx_0.get<sidechain_transaction_sign_operation>().signer;
      std::string signature = op_obj_idx_0.get<sidechain_transaction_sign_operation>().signature;
      sidechain_transaction_id_type sidechain_transaction_id = op_obj_idx_0.get<sidechain_transaction_sign_operation>().sidechain_transaction_id;
      std::vector<uint64_t> in_amounts;
      std::string tx_hex;
      std::string redeem_script;
      const auto &st_idx = database.get_index_type<sidechain_transaction_index>().indices().get<by_id>();
      const auto sto = st_idx.find(sidechain_transaction_id);
      if (sto == st_idx.end()) {
         should_approve = false;
         break;
      }

      const auto &s_idx = database.get_index_type<son_index>().indices().get<by_id>();
      const auto son = s_idx.find(signer);
      if (son == s_idx.end()) {
         should_approve = false;
         break;
      }

      read_transaction_data(sto->transaction, tx_hex, in_amounts, redeem_script);
      bitcoin_transaction tx = unpack(parse_hex(tx_hex));
      bitcoin::bytes pubkey = parse_hex(son->sidechain_public_keys.at(sidechain));
      vector<bitcoin::bytes> sigs = read_byte_arrays_from_string(signature);
      for (size_t i = 0; i < tx.vin.size(); i++) {
         const auto &sighash_str = get_signature_hash(tx, parse_hex(redeem_script), static_cast<int64_t>(in_amounts[i]), i, 1, true).str();
         const bitcoin::bytes &sighash_hex = parse_hex(sighash_str);
         should_approve = should_approve && verify_sig(sigs[i], pubkey, sighash_hex, btc_context());
      }
      break;
   }

   case chain::operation::tag<chain::sidechain_transaction_settle_operation>::value: {
      should_approve = true;
      break;
   }

   default:
      should_approve = false;
      elog("==================================================");
      elog("Proposal not considered for approval ${po}", ("po", po));
      elog("==================================================");
   }

   return should_approve;
}

void sidechain_net_handler_bitcoin::process_primary_wallet() {
   const auto &swi = database.get_index_type<son_wallet_index>().indices().get<by_id>();
   const auto &active_sw = swi.rbegin();
   if (active_sw != swi.rend()) {

      const auto &prev_sw = std::next(active_sw);
      if (prev_sw != swi.rend() && active_sw->sons.at(sidechain) == prev_sw->sons.at(sidechain))
         return;

      if ((active_sw->addresses.find(sidechain) == active_sw->addresses.end()) ||
          (active_sw->addresses.at(sidechain).empty())) {

         const auto ast = active_sidechain_types(database.head_block_time());
         const auto id = active_sw->id.instance() * ast.size() + std::distance(ast.begin(), ast.find(sidechain));
         const object_id_type op_id{active_sw->id.space(), active_sw->id.type(), id};

         if (proposal_exists(chain::operation::tag<chain::son_wallet_update_operation>::value, op_id)) {
            return;
         }

         const chain::global_property_object &gpo = database.get_global_properties();

         const auto &active_sons = gpo.active_sons.at(sidechain);
         string reply_str = create_primary_wallet_address(active_sons);

         std::stringstream active_pw_ss(reply_str);
         boost::property_tree::ptree active_pw_pt;
         boost::property_tree::read_json(active_pw_ss, active_pw_pt);
         if (active_pw_pt.count("error") && active_pw_pt.get_child("error").empty()) {
            if (!plugin.can_son_participate(sidechain, chain::operation::tag<chain::son_wallet_update_operation>::value, op_id)) {
               return;
            }

            proposal_create_operation proposal_op;
            proposal_op.fee_paying_account = plugin.get_current_son_object(sidechain).son_account;
            uint32_t lifetime = (gpo.parameters.block_interval * gpo.active_witnesses.size()) * 3;
            proposal_op.expiration_time = time_point_sec(database.head_block_time().sec_since_epoch() + lifetime);

            std::stringstream res;
            boost::property_tree::json_parser::write_json(res, active_pw_pt.get_child("result"));

            son_wallet_update_operation swu_op;
            swu_op.payer = gpo.parameters.son_account();
            swu_op.son_wallet_id = op_id;
            swu_op.sidechain = sidechain;
            swu_op.address = res.str();

            proposal_op.proposed_ops.emplace_back(swu_op);

            if (prev_sw != swi.rend()) {
               std::string new_pw_address = active_pw_pt.get<std::string>("result.address");
               std::string tx_str = create_primary_wallet_transaction(*prev_sw, new_pw_address);
               if (!tx_str.empty()) {
                  const auto ast = active_sidechain_types(database.head_block_time());
                  const auto prev_id = prev_sw->id.instance() * ast.size() + std::distance(ast.begin(), ast.find(sidechain));
                  const object_id_type prev_op_id{prev_sw->id.space(), prev_sw->id.type(), prev_id};

                  sidechain_transaction_create_operation stc_op;
                  stc_op.payer = gpo.parameters.son_account();
                  stc_op.object_id = prev_op_id;
                  stc_op.sidechain = sidechain;
                  stc_op.transaction = tx_str;
                  for (const auto &signer : prev_sw->sons.at(sidechain)) {
                     son_info si;
                     si.son_id = signer.son_id;
                     si.weight = signer.weight;
                     si.signing_key = signer.signing_key;
                     si.sidechain_public_keys[sidechain] = signer.public_key;
                     stc_op.signers.emplace_back(std::move(si));
                  }
                  proposal_op.proposed_ops.emplace_back(stc_op);
               }
            }

            signed_transaction trx = database.create_signed_transaction(plugin.get_private_key(plugin.get_current_son_id(sidechain)), proposal_op);
            try {
               trx.validate();
               database.push_transaction(trx, database::validation_steps::skip_block_size_check);
               if (plugin.app().p2p_node())
                  plugin.app().p2p_node()->broadcast(net::trx_message(trx));
               plugin.log_son_proposal_retry(sidechain, chain::operation::tag<chain::son_wallet_update_operation>::value, op_id);
            } catch (fc::exception &e) {
               elog("Sending proposal for son wallet update operation failed with exception ${e}", ("e", e.what()));
               return;
            }
         }
      }
   }
}

void sidechain_net_handler_bitcoin::process_sidechain_addresses() {
   using namespace bitcoin;

   const chain::global_property_object &gpo = database.get_global_properties();
   std::vector<std::pair<fc::ecc::public_key, uint16_t>> pubkeys;
   for (auto &son : gpo.active_sons.at(sidechain)) {
      auto pubkey = fc::ecc::public_key(create_public_key_data(parse_hex(son.public_key)));
      pubkeys.push_back(std::make_pair(pubkey, son.weight));
   }

   const auto &sidechain_addresses_idx = database.get_index_type<sidechain_address_index>();
   const auto &sidechain_addresses_by_sidechain_idx = sidechain_addresses_idx.indices().get<by_sidechain>();
   const auto &sidechain_addresses_by_sidechain_range = sidechain_addresses_by_sidechain_idx.equal_range(sidechain);
   std::for_each(sidechain_addresses_by_sidechain_range.first, sidechain_addresses_by_sidechain_range.second,
                 [&](const sidechain_address_object &sao) {
                    bool retval = true;
                    try {
                       if (sao.expires == time_point_sec::maximum()) {
                          auto usr_pubkey = fc::ecc::public_key(create_public_key_data(parse_hex(sao.deposit_public_key)));

                          payment_type payment_type_address = payment_type::P2SH_WSH;
                          if (use_bitcoind_client) {
                             payment_type_address = payment_type::P2WSH;
                          }

                          btc_one_or_weighted_multisig_address addr(usr_pubkey, pubkeys, network_type, payment_type_address);
                          std::string address_data = "{ \"redeemScript\": \"" + fc::to_hex(addr.get_redeem_script()) +
                                                     "\", \"witnessScript\": \"" + fc::to_hex(addr.get_witness_script()) + "\" }";

                          if (addr.get_address() != sao.deposit_address) {
                             sidechain_address_update_operation op;
                             op.payer = plugin.get_current_son_object(sidechain).son_account;
                             op.sidechain_address_id = sao.id;
                             op.sidechain_address_account = sao.sidechain_address_account;
                             op.sidechain = sao.sidechain;
                             op.deposit_public_key = sao.deposit_public_key;
                             op.deposit_address = addr.get_address();
                             op.deposit_address_data = address_data;
                             op.withdraw_public_key = sao.withdraw_public_key;
                             op.withdraw_address = sao.withdraw_address;

                             signed_transaction trx = database.create_signed_transaction(plugin.get_private_key(plugin.get_current_son_id(sidechain)), op);
                             try {
                                trx.validate();
                                database.push_transaction(trx, database::validation_steps::skip_block_size_check);
                                if (plugin.app().p2p_node())
                                   plugin.app().p2p_node()->broadcast(net::trx_message(trx));
                                retval = true;
                             } catch (fc::exception &e) {
                                elog("Sending transaction for sidechain address update operation failed with exception ${e}", ("e", e.what()));
                                retval = false;
                             }
                          }
                       }
                    } catch (fc::exception &e) {
                       retval = false;
                    }
                    return retval;
                 });
}

bool sidechain_net_handler_bitcoin::process_deposit(const son_wallet_deposit_object &swdo) {

   if (proposal_exists(chain::operation::tag<chain::son_wallet_deposit_process_operation>::value, swdo.id)) {
      return false;
   }

   std::string tx_str = create_deposit_transaction(swdo);

   if (!tx_str.empty()) {
      const chain::global_property_object &gpo = database.get_global_properties();

      proposal_create_operation proposal_op;
      proposal_op.fee_paying_account = plugin.get_current_son_object(sidechain).son_account;
      uint32_t lifetime = (gpo.parameters.block_interval * gpo.active_witnesses.size()) * 3;
      proposal_op.expiration_time = time_point_sec(database.head_block_time().sec_since_epoch() + lifetime);

      son_wallet_deposit_process_operation swdp_op;
      swdp_op.payer = gpo.parameters.son_account();
      swdp_op.son_wallet_deposit_id = swdo.id;
      proposal_op.proposed_ops.emplace_back(swdp_op);

      sidechain_transaction_create_operation stc_op;
      stc_op.payer = gpo.parameters.son_account();
      stc_op.object_id = swdo.id;
      stc_op.sidechain = sidechain;
      stc_op.transaction = tx_str;
      for (const auto &signer : gpo.active_sons.at(sidechain)) {
         son_info si;
         si.son_id = signer.son_id;
         si.weight = signer.weight;
         si.signing_key = signer.signing_key;
         si.sidechain_public_keys[sidechain] = signer.public_key;
         stc_op.signers.emplace_back(std::move(si));
      }
      proposal_op.proposed_ops.emplace_back(stc_op);

      signed_transaction trx = database.create_signed_transaction(plugin.get_private_key(plugin.get_current_son_id(sidechain)), proposal_op);
      try {
         trx.validate();
         database.push_transaction(trx, database::validation_steps::skip_block_size_check);
         if (plugin.app().p2p_node())
            plugin.app().p2p_node()->broadcast(net::trx_message(trx));
         return true;
      } catch (fc::exception &e) {
         elog("Sending proposal for deposit sidechain transaction create operation failed with exception ${e}", ("e", e.what()));
         return false;
      }
   }
   return false;
}

bool sidechain_net_handler_bitcoin::process_withdrawal(const son_wallet_withdraw_object &swwo) {

   if (proposal_exists(chain::operation::tag<chain::son_wallet_withdraw_process_operation>::value, swwo.id)) {
      return false;
   }

   std::string tx_str = create_withdrawal_transaction(swwo);

   if (!tx_str.empty()) {
      const chain::global_property_object &gpo = database.get_global_properties();

      proposal_create_operation proposal_op;
      proposal_op.fee_paying_account = plugin.get_current_son_object(sidechain).son_account;
      uint32_t lifetime = (gpo.parameters.block_interval * gpo.active_witnesses.size()) * 3;
      proposal_op.expiration_time = time_point_sec(database.head_block_time().sec_since_epoch() + lifetime);

      son_wallet_withdraw_process_operation swwp_op;
      swwp_op.payer = gpo.parameters.son_account();
      swwp_op.son_wallet_withdraw_id = swwo.id;
      proposal_op.proposed_ops.emplace_back(swwp_op);

      sidechain_transaction_create_operation stc_op;
      stc_op.payer = gpo.parameters.son_account();
      stc_op.object_id = swwo.id;
      stc_op.sidechain = sidechain;
      stc_op.transaction = tx_str;
      for (const auto &signer : gpo.active_sons.at(sidechain)) {
         son_info si;
         si.son_id = signer.son_id;
         si.weight = signer.weight;
         si.signing_key = signer.signing_key;
         si.sidechain_public_keys[sidechain] = signer.public_key;
         stc_op.signers.emplace_back(std::move(si));
      }
      proposal_op.proposed_ops.emplace_back(stc_op);

      signed_transaction trx = database.create_signed_transaction(plugin.get_private_key(plugin.get_current_son_id(sidechain)), proposal_op);
      try {
         trx.validate();
         database.push_transaction(trx, database::validation_steps::skip_block_size_check);
         if (plugin.app().p2p_node())
            plugin.app().p2p_node()->broadcast(net::trx_message(trx));
         return true;
      } catch (fc::exception &e) {
         elog("Sending proposal for withdraw sidechain transaction create operation failed with exception ${e}", ("e", e.what()));
         return false;
      }
   }
   return false;
}

std::string sidechain_net_handler_bitcoin::process_sidechain_transaction(const sidechain_transaction_object &sto) {
   return sign_transaction(sto);
}

std::string sidechain_net_handler_bitcoin::send_sidechain_transaction(const sidechain_transaction_object &sto) {
   return send_transaction(sto);
}

bool sidechain_net_handler_bitcoin::settle_sidechain_transaction(const sidechain_transaction_object &sto, asset &settle_amount) {

   if (sto.object_id.is<son_wallet_id_type>()) {
      settle_amount = asset(0, database.get_global_properties().parameters.btc_asset());
      return true;
   }

   if (sto.sidechain_transaction.empty()) {
      return false;
   }

   btc_tx tx = bitcoin_client->getrawtransaction(sto.sidechain_transaction, true);

   if (tx.tx_in_list.empty()) {
      // This case will result with segmentation fault.
      // FIXME check if that happened before introducing libbitcoin
      return false;
   }

   const chain::global_property_object &gpo = database.get_global_properties();

   using namespace bitcoin;
   std::vector<std::pair<fc::ecc::public_key, uint16_t>> pubkey_weights;
   for (auto si : sto.signers) {
      auto pub_key = fc::ecc::public_key(create_public_key_data(parse_hex(si.public_key)));
      pubkey_weights.push_back(std::make_pair(pub_key, si.weight));
   }

   payment_type payment_type_address = payment_type::P2SH_WSH;
   if (use_bitcoind_client) {
      payment_type_address = payment_type::P2WSH;
   }

   btc_weighted_multisig_address addr(pubkey_weights, network_type, payment_type_address);

   std::string tx_txid = tx.tx_txid;
   uint32_t tx_confirmations = tx.tx_confirmations;
   std::string tx_address = addr.get_address();
   int64_t tx_amount = -1;

   if (tx_confirmations >= gpo.parameters.son_bitcoin_min_tx_confirmations()) {
      if (sto.object_id.is<son_wallet_deposit_id_type>()) {
         for (auto &input : tx.tx_in_list) {
            for (auto &address : input.tx_address) {
               if (address == tx_address) {
                  tx_amount = input.tx_amount;
                  break;
               }
            }
            settle_amount = asset(tx_amount, database.get_global_properties().parameters.btc_asset());
            return true;
         }
      }

      if (sto.object_id.is<son_wallet_withdraw_id_type>()) {
         auto swwo = database.get<son_wallet_withdraw_object>(sto.object_id);
         settle_amount = asset(swwo.withdraw_amount, database.get_global_properties().parameters.btc_asset());
         return true;
      }
   }
   return false;
}

optional<asset> sidechain_net_handler_bitcoin::estimate_withdrawal_transaction_fee() const {
   wlog("estimate_withdrawal_transaction_fee not implemented for sidechain: ${sidechain}", ("sidechain", sidechain));
   return optional<asset>{};
}

std::string sidechain_net_handler_bitcoin::create_primary_wallet_address(const std::vector<son_sidechain_info> &son_pubkeys) {
   using namespace bitcoin;

   std::vector<std::pair<fc::ecc::public_key, uint16_t>> pubkey_weights;
   for (auto &son : son_pubkeys) {
      auto pub_key = fc::ecc::public_key(create_public_key_data(parse_hex(son.public_key)));
      pubkey_weights.push_back(std::make_pair(pub_key, son.weight));
   }

   payment_type payment_type_address = payment_type::P2SH_WSH;
   if (use_bitcoind_client) {
      payment_type_address = payment_type::P2WSH;
   }
   btc_weighted_multisig_address addr(pubkey_weights, network_type, payment_type_address);

   std::stringstream ss;

   ss << "{\"result\": {\"address\": \"" << addr.get_address() << "\", \"redeemScript\": \"" << fc::to_hex(addr.get_redeem_script()) << "\""
      << "}, \"error\":null}";

   std::string res = ss.str();
   return res;
}

std::string sidechain_net_handler_bitcoin::create_primary_wallet_transaction(const son_wallet_object &prev_swo, std::string new_sw_address) {

   const auto &address_data = prev_swo.addresses.find(sidechain);
   if (address_data == prev_swo.addresses.end()) {
      return "";
   }

   std::string s = address_data->second;
   if (s.empty()) {
      return "";
   }

   std::stringstream prev_sw_ss(s);
   boost::property_tree::ptree prev_sw_pt;
   boost::property_tree::read_json(prev_sw_ss, prev_sw_pt);
   std::string prev_pw_address = prev_sw_pt.get<std::string>("address");
   std::string prev_redeem_script = prev_sw_pt.get<std::string>("redeemScript");

   if (prev_pw_address == new_sw_address) {
      wlog("BTC previous and new primary wallet addresses are same. No funds moving needed [from ${prev_sw} to ${new_sw_address}]", ("prev_swo", prev_swo.id)("active_sw", new_sw_address));
      return "";
   }

   uint64_t fee_rate = bitcoin_client->estimatesmartfee();
   const uint64_t min_fee_rate = 1000;
   fee_rate = std::max(fee_rate, min_fee_rate);

   uint64_t total_amount = 0.0;
   const std::vector<btc_txout> inputs = bitcoin_client->listunspent_by_address_and_amount(prev_pw_address, 0);

   if (inputs.size() == 0) {
      elog("Failed to find UTXOs to spend for ${pw}", ("pw", prev_pw_address));
      return "";
   } else {
      for (const auto &utx : inputs) {
         total_amount += utx.amount_;
      }

      if (fee_rate >= total_amount) {
         elog("Not enough BTC to pay the transfer fee, address ${fa}", ("fa", prev_pw_address));
         return "";
      }
   }

   fc::flat_map<std::string, double> outputs;
   outputs[new_sw_address] = double(total_amount - fee_rate) / 100000000.0;

   return create_transaction(inputs, outputs, prev_redeem_script);
}

std::string sidechain_net_handler_bitcoin::create_deposit_transaction(const son_wallet_deposit_object &swdo) {
   const auto &idx = database.get_index_type<son_wallet_index>().indices().get<by_id>();
   auto obj = idx.rbegin();
   if (obj == idx.rend() || obj->addresses.find(sidechain) == obj->addresses.end()) {
      return "";
   }
   // Get redeem script for deposit address
   std::string redeem_script = get_redeemscript_for_userdeposit(swdo.sidechain_from);
   std::string pw_address_json = obj->addresses.find(sidechain)->second;

   std::stringstream ss(pw_address_json);
   boost::property_tree::ptree json;
   boost::property_tree::read_json(ss, json);

   std::string pw_address = json.get<std::string>("address");

   std::string txid = swdo.sidechain_transaction_id;
   std::string suid = swdo.sidechain_uid;
   std::string nvout = suid.substr(suid.find_last_of("-") + 1);
   uint64_t deposit_amount = swdo.sidechain_amount.value;
   uint64_t fee_rate = bitcoin_client->estimatesmartfee();
   const uint64_t min_fee_rate = 1000;
   fee_rate = std::max(fee_rate, min_fee_rate);

   if (fee_rate >= deposit_amount) {
      elog("Not enough BTC to pay the transfer fee, address ${da}", ("da", swdo.sidechain_from));
      return "";
   }

   deposit_amount -= fee_rate; // Deduct minimum relay fee
   double transfer_amount = (double)deposit_amount / 100000000.0;

   std::vector<btc_txout> inputs;
   fc::flat_map<std::string, double> outputs;

   btc_txout utxo;
   utxo.txid_ = txid;
   utxo.out_num_ = std::stoul(nvout);
   utxo.amount_ = swdo.sidechain_amount.value;

   inputs.push_back(utxo);

   outputs[pw_address] = transfer_amount;

   return create_transaction(inputs, outputs, redeem_script);
}

std::string sidechain_net_handler_bitcoin::create_withdrawal_transaction(const son_wallet_withdraw_object &swwo) {
   const auto &idx = database.get_index_type<son_wallet_index>().indices().get<by_id>();
   auto obj = idx.rbegin();
   if (obj == idx.rend() || obj->addresses.find(sidechain) == obj->addresses.end()) {
      return "";
   }

   std::string pw_address_json = obj->addresses.find(sidechain)->second;

   std::stringstream ss(pw_address_json);
   boost::property_tree::ptree json;
   boost::property_tree::read_json(ss, json);

   std::string pw_address = json.get<std::string>("address");
   std::string redeem_script = json.get<std::string>("redeemScript");

   int64_t fee_rate = bitcoin_client->estimatesmartfee();
   const int64_t min_fee_rate = 1000;
   fee_rate = std::max(fee_rate, min_fee_rate);

   int64_t total_amount = 0;
   const std::vector<btc_txout> inputs = bitcoin_client->listunspent_by_address_and_amount(pw_address, 0);

   if (inputs.size() == 0) {
      elog("Failed to find UTXOs to spend for ${pw}", ("pw", pw_address));
      return "";
   } else {
      for (const auto &utx : inputs) {
         total_amount += utx.amount_;
      }

      if ((swwo.withdraw_amount.value > total_amount) || (fee_rate >= swwo.withdraw_amount.value)) {
         elog("Not enough BTC to pay the transfer fee, address ${pw}", ("pw", pw_address));
         return "";
      }
   }

   fc::flat_map<std::string, double> outputs;
   outputs[swwo.withdraw_address] = (swwo.withdraw_amount.value - fee_rate) / 100000000.0;
   if ((total_amount - swwo.withdraw_amount.value) > 0.0) {
      outputs[pw_address] = double(total_amount - swwo.withdraw_amount.value) / 100000000.0;
   }

   return create_transaction(inputs, outputs, redeem_script);
}

std::string sidechain_net_handler_bitcoin::create_transaction(const std::vector<btc_txout> &inputs, const fc::flat_map<std::string, double> outputs, std::string &redeem_script) {
   using namespace bitcoin;

   bitcoin_transaction_builder tb;
   std::vector<uint64_t> in_amounts;

   tb.set_version(2);
   // Set vins
   for (auto in : inputs) {
      tb.add_in(fc::sha256(in.txid_), in.out_num_, bitcoin::bytes());
      in_amounts.push_back(in.amount_);
   }
   // Set vouts
   for (auto out : outputs) {
      uint64_t satoshis = out.second * 100000000.0;
      tb.add_out_all_type(satoshis, out.first);
   }

   const auto tx = tb.get_transaction();
   std::string hex_tx = fc::to_hex(pack(tx));
   std::string tx_raw = write_transaction_data(hex_tx, in_amounts, redeem_script);
   return tx_raw;
}

std::string sidechain_net_handler_bitcoin::sign_transaction(const sidechain_transaction_object &sto) {
   using namespace bitcoin;
   std::string pubkey = plugin.get_current_son_object(sidechain).sidechain_public_keys.at(sidechain);
   std::string prvkey = get_private_key(pubkey);
   std::vector<uint64_t> in_amounts;
   std::string tx_hex;
   std::string redeem_script;

   fc::optional<fc::ecc::private_key> btc_private_key = graphene::utilities::wif_to_key(prvkey);
   if (!btc_private_key) {
      elog("Invalid private key ${pk}", ("pk", prvkey));
      return "";
   }
   const auto secret = btc_private_key->get_secret();
   bitcoin::bytes privkey_signing(secret.data(), secret.data() + secret.data_size());

   read_transaction_data(sto.transaction, tx_hex, in_amounts, redeem_script);

   bitcoin_transaction tx = unpack(parse_hex(tx_hex));
   std::vector<bitcoin::bytes> redeem_scripts(tx.vin.size(), parse_hex(redeem_script));
   auto sigs = sign_witness_transaction_part(tx, redeem_scripts, in_amounts, privkey_signing, btc_context(), 1);
   std::string tx_signature = write_transaction_signatures(sigs);

   return tx_signature;
}

std::string sidechain_net_handler_bitcoin::send_transaction(const sidechain_transaction_object &sto) {
   using namespace bitcoin;
   std::vector<uint64_t> in_amounts;
   std::string tx_hex;
   std::string redeem_script;

   read_transaction_data(sto.transaction, tx_hex, in_amounts, redeem_script);

   bitcoin_transaction tx = unpack(parse_hex(tx_hex));

   std::vector<bitcoin::bytes> redeem_scripts(tx.vin.size(), parse_hex(redeem_script));

   uint32_t inputs_number = in_amounts.size();
   vector<bitcoin::bytes> dummy;
   dummy.resize(inputs_number);
   // Organise weighted address signatures
   // Add dummies for empty signatures
   vector<vector<bitcoin::bytes>> signatures;
   for (unsigned idx = 0; idx < sto.signatures.size(); ++idx) {
      if (sto.signatures[idx].second.empty())
         signatures.push_back(dummy);
      else
         signatures.push_back(read_byte_arrays_from_string(sto.signatures[idx].second));
   }
   // Add empty sig for user signature for Deposit transaction
   if (sto.object_id.type() == son_wallet_deposit_object::type_id) {
      add_signatures_to_transaction_user_weighted_multisig(tx, signatures);
   } else {
      add_signatures_to_transaction_weighted_multisig(tx, signatures);
   }
   // Add redeemscripts to vins and make tx ready for sending
   sign_witness_transaction_finalize(tx, redeem_scripts, false);

   if (!use_bitcoind_client) {
      // get witness script from redeem script
      bitcoin::bytes redeem_bytes = parse_hex(redeem_script);
      fc::sha256 sha = fc::sha256::hash(&redeem_bytes[0], redeem_bytes.size());
      std::string witness_script(sha.str());
      witness_script.insert(0, std::string("220020"));
      for (size_t i = 0; i < tx.vin.size(); i++) {
         tx.vin[i].scriptSig = parse_hex(witness_script);
      }
   }

   std::string final_tx_hex = fc::to_hex(pack(tx));
   std::string res = bitcoin_client->sendrawtransaction(final_tx_hex);

   if (res.empty()) {
      return res;
   }

   return tx.get_txid().str();
}

void sidechain_net_handler_bitcoin::block_handle_event(const block_data &event_data) {

   auto vins = bitcoin_client->getblock(event_data);

   add_to_son_listener_log("BLOCK   : " + event_data.block_hash);

   scoped_lock interlock(event_handler_mutex);
   const auto &sidechain_addresses_idx = database.get_index_type<sidechain_address_index>().indices().get<by_sidechain_and_deposit_address_and_expires>();

   for (const auto &v : vins) {
      // !!! EXTRACT DEPOSIT ADDRESS FROM SIDECHAIN ADDRESS OBJECT
      const auto &addr_itr = sidechain_addresses_idx.find(std::make_tuple(sidechain, v.address, time_point_sec::maximum()));
      if (addr_itr == sidechain_addresses_idx.end())
         continue;

      std::stringstream ss;
      ss << "bitcoin"
         << "-" << v.out.hash_tx << "-" << v.out.n_vout;
      std::string sidechain_uid = ss.str();

      sidechain_event_data sed;
      sed.timestamp = database.head_block_time();
      sed.block_num = database.head_block_num();
      sed.sidechain = addr_itr->sidechain;
      sed.type = sidechain_event_type::deposit;
      sed.sidechain_uid = sidechain_uid;
      sed.sidechain_transaction_id = v.out.hash_tx;
      sed.sidechain_from = v.address;
      sed.sidechain_to = "";
      sed.sidechain_currency = "BTC";
      sed.sidechain_amount = v.out.amount;
      sed.peerplays_from = addr_itr->sidechain_address_account;
      sed.peerplays_to = database.get_global_properties().parameters.son_account();
      price btc_price = database.get<asset_object>(database.get_global_properties().parameters.btc_asset()).options.core_exchange_rate;
      sed.peerplays_asset = asset(sed.sidechain_amount * btc_price.base.amount / btc_price.quote.amount);

      add_to_son_listener_log("TRX     : " + sed.sidechain_transaction_id);

      sidechain_event_data_received(sed);
   }
}

void sidechain_net_handler_bitcoin::trx_handle_event(const libbitcoin::chain::transaction &trx_data) {
   bitcoin_client->import_trx_to_memory_pool(trx_data);
}

std::string sidechain_net_handler_bitcoin::get_redeemscript_for_userdeposit(const std::string &user_address) {
   using namespace bitcoin;
   const auto &sidechain_addresses_idx = database.get_index_type<sidechain_address_index>().indices().get<by_sidechain_and_deposit_address_and_expires>();
   const auto &addr_itr = sidechain_addresses_idx.find(std::make_tuple(sidechain, user_address, time_point_sec::maximum()));
   if (addr_itr == sidechain_addresses_idx.end()) {
      return "";
   }

   const auto &idx = database.get_index_type<son_wallet_index>().indices().get<by_id>();
   auto obj = idx.rbegin();
   if (obj == idx.rend() || obj->addresses.find(sidechain) == obj->addresses.end()) {
      return "";
   }

   std::vector<std::pair<fc::ecc::public_key, uint16_t>> pubkey_weights;
   for (auto &son : obj->sons.at(sidechain)) {
      auto pub_key = fc::ecc::public_key(create_public_key_data(parse_hex(son.public_key)));
      pubkey_weights.push_back(std::make_pair(pub_key, son.weight));
   }
   auto user_pub_key = fc::ecc::public_key(create_public_key_data(parse_hex(addr_itr->deposit_public_key)));

   payment_type payment_type_address = payment_type::P2SH_WSH;
   if (use_bitcoind_client) {
      payment_type_address = payment_type::P2WSH;
   }
   btc_one_or_weighted_multisig_address deposit_addr(user_pub_key, pubkey_weights, network_type, payment_type_address);

   return fc::to_hex(deposit_addr.get_redeem_script());
}

void sidechain_net_handler_bitcoin::on_changed_objects(const vector<object_id_type> &ids, const flat_set<account_id_type> &accounts) {
   fc::time_point now = fc::time_point::now();
   int64_t time_to_next_changed_objects_processing = 5000;

   fc::time_point next_wakeup(now + fc::microseconds(time_to_next_changed_objects_processing));

   on_changed_objects_task = fc::schedule([this, ids, accounts] {
      on_changed_objects_cb(ids, accounts);
   },
                                          next_wakeup, "SON Processing");
}

void sidechain_net_handler_bitcoin::on_changed_objects_cb(const vector<object_id_type> &ids, const flat_set<account_id_type> &accounts) {
   std::vector<bitcoin_rpc_client::multi_params> address_or_script_array;

   for (auto id : ids) {
      if (id.is<son_wallet_object>()) {
         const auto &swi = database.get_index_type<son_wallet_index>().indices().get<by_id>();
         auto swo = swi.find(id);
         if (swo != swi.end()) {
            std::stringstream pw_ss(swo->addresses.at(sidechain));
            boost::property_tree::ptree pw_pt;
            boost::property_tree::read_json(pw_ss, pw_pt);

            if (pw_pt.count("address")) {
               std::string pw_address = pw_pt.get<std::string>("address");
               address_or_script_array.emplace_back(bitcoin_rpc_client::multi_params{bitcoin_rpc_client::multi_type::address, pw_address});
            }

            if (pw_pt.count("redeemScript")) {
               std::string pw_redeem_script = pw_pt.get<std::string>("redeemScript");
               address_or_script_array.emplace_back(bitcoin_rpc_client::multi_params{bitcoin_rpc_client::multi_type::script, pw_redeem_script});
            }
         }
      }
   }

   //! importmulti all addreses in one bulk
   if (!address_or_script_array.empty()) {
      //! Unlock wallet
      if (!wallet_password.empty()) {
         if (!bitcoin_client->walletpassphrase(wallet_password))
            return;
      }

      //! importmulti
      bitcoin_client->importmulti(address_or_script_array);

      //! Lock wallet
      bitcoin_client->walletlock();
   }
}

// =============================================================================
}
} // namespace graphene::peerplays_sidechain
