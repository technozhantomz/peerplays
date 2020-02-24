#include <graphene/peerplays_sidechain/sidechain_net_handler_bitcoin.hpp>

#include <algorithm>
#include <thread>

#include <boost/algorithm/hex.hpp>
#include <boost/property_tree/ptree.hpp>
#include <boost/property_tree/json_parser.hpp>

#include <fc/crypto/base64.hpp>
#include <fc/log/logger.hpp>
#include <fc/network/ip.hpp>

#include <graphene/chain/account_object.hpp>
#include <graphene/chain/sidechain_address_object.hpp>
#include <graphene/chain/son_info.hpp>
#include <graphene/chain/son_wallet_object.hpp>
#include <graphene/chain/protocol/son_wallet.hpp>

namespace graphene { namespace peerplays_sidechain {

// =============================================================================

bitcoin_rpc_client::bitcoin_rpc_client( std::string _ip, uint32_t _rpc, std::string _user, std::string _password ):
                      ip( _ip ), rpc_port( _rpc ), user( _user ), password( _password )
{
   authorization.key = "Authorization";
   authorization.val = "Basic " + fc::base64_encode( user + ":" + password );
}

bool bitcoin_rpc_client::connection_is_not_defined() const {
   return ip.empty() || rpc_port == 0 || user.empty() || password.empty();
}

std::string bitcoin_rpc_client::addmultisigaddress(const std::vector<std::string> public_keys) {
   std::string body = std::string("{\"jsonrpc\": \"1.0\", \"id\":\"addmultisigaddress\", "
                                  "\"method\": \"addmultisigaddress\", \"params\": [");
   std::string params = "2, [";
   std::string pubkeys = "";
   for (std::string pubkey : public_keys) {
      if (!pubkeys.empty()) {
         pubkeys = pubkeys + ",";
      }
      pubkeys = pubkeys + std::string("\"") + pubkey + std::string("\"");
   }
   params = params + pubkeys + std::string("]");
   body = body + params + std::string("] }");

   const auto reply = send_post_request(body);

   if (reply.body.empty()) {
      wlog("Bitcoin RPC call ${function} failed", ("function", __FUNCTION__));
      return std::string();
   }

   std::stringstream ss(std::string(reply.body.begin(), reply.body.end()));
   boost::property_tree::ptree json;
   boost::property_tree::read_json(ss, json);

   if (reply.status == 200) {
      return ss.str();
   }

   if (json.count("error") && !json.get_child("error").empty()) {
      wlog("BTC multisig address creation failed! Reply: ${msg}", ("msg", ss.str()));
   }
   return "";
}

std::string bitcoin_rpc_client::createrawtransaction(const std::vector<btc_txout> &ins, const fc::flat_map<std::string, double> outs) {
   std::string body("{\"jsonrpc\": \"1.0\", \"id\":\"createrawtransaction\", "
                    "\"method\": \"createrawtransaction\", \"params\": [");
   body += "[";
   bool first = true;
   for (const auto &entry : ins) {
      if (!first)
         body += ",";
      body += "{\"txid\":\"" + entry.txid_ + "\",\"vout\":" + std::to_string(entry.out_num_) + "}";
      first = false;
   }
   body += "],[";
   first = true;
   for (const auto &entry : outs) {
      if (!first)
         body += ",";
      body += "{\"" + entry.first + "\":" + std::to_string(entry.second) + "}";
      first = false;
   }
   body += std::string("]] }");

   ilog(body);

   const auto reply = send_post_request(body);

   if (reply.body.empty()) {
      wlog("Bitcoin RPC call ${function} failed", ("function", __FUNCTION__));
      return std::string();
   }

   std::stringstream ss(std::string(reply.body.begin(), reply.body.end()));
   boost::property_tree::ptree json;
   boost::property_tree::read_json(ss, json);

   if (reply.status == 200) {
      if (json.count("result"))
         return ss.str();
   } else if (json.count("error") && !json.get_child("error").empty()) {
      wlog("Failed to create raw transaction: [${body}]! Reply: ${msg}", ("body", body)("msg", ss.str()));
   }
   return std::string();
}

uint64_t bitcoin_rpc_client::estimatesmartfee() {
   static const auto confirmation_target_blocks = 6;

   const auto body = std::string("{\"jsonrpc\": \"1.0\", \"id\":\"estimatesmartfee\", "
                                 "\"method\": \"estimatesmartfee\", \"params\": [") +
                     std::to_string(confirmation_target_blocks) + std::string("] }");

   const auto reply = send_post_request(body);

   if (reply.body.empty()) {
      wlog("Bitcoin RPC call ${function} failed", ("function", __FUNCTION__));
      return 0;
   }

   std::stringstream ss(std::string(reply.body.begin(), reply.body.end()));
   boost::property_tree::ptree json;
   boost::property_tree::read_json(ss, json);

   if (json.count("result"))
      if (json.get_child("result").count("feerate")) {
         auto feerate_str = json.get_child("result").get_child("feerate").get_value<std::string>();
         feerate_str.erase(std::remove(feerate_str.begin(), feerate_str.end(), '.'), feerate_str.end());
         return std::stoll(feerate_str);
      }
   return 0;
}

std::string bitcoin_rpc_client::getblock(const std::string &block_hash, int32_t verbosity) {
   std::string body = std::string("{\"jsonrpc\": \"1.0\", \"id\":\"getblock\", \"method\": "
                                  "\"getblock\", \"params\": [\"" +
                                  block_hash + "\", " + std::to_string(verbosity) + "] }");

   const auto reply = send_post_request(body);

   if (reply.body.empty()) {
      wlog("Bitcoin RPC call ${function} failed", ("function", __FUNCTION__));
      return std::string();
   }

   std::stringstream ss(std::string(reply.body.begin(), reply.body.end()));
   boost::property_tree::ptree json;
   boost::property_tree::read_json(ss, json);

   if (reply.status == 200) {
      std::stringstream ss;
      boost::property_tree::json_parser::write_json(ss, json.get_child("result"));
      return ss.str();
   }

   if (json.count("error") && !json.get_child("error").empty()) {
      wlog("Bitcoin RPC call ${function} failed with reply '${msg}'", ("function", __FUNCTION__)("msg", ss.str()));
   }
   return "";
}

void bitcoin_rpc_client::importaddress(const std::string &address_or_script) {
   const auto body = std::string("{\"jsonrpc\": \"1.0\", \"id\":\"importaddress\", "
                                 "\"method\": \"importaddress\", \"params\": [") +
                     std::string("\"") + address_or_script + std::string("\"") + std::string("] }");

   const auto reply = send_post_request(body);

   if (reply.body.empty()) {
      wlog("Bitcoin RPC call ${function} failed", ("function", __FUNCTION__));
      return;
   }

   std::stringstream ss(std::string(reply.body.begin(), reply.body.end()));
   boost::property_tree::ptree json;
   boost::property_tree::read_json(ss, json);

   if (reply.status == 200) {
      idump((address_or_script)(ss.str()));
      return;
   } else if (json.count("error") && !json.get_child("error").empty()) {
      wlog("Failed to import address [${addr}]! Reply: ${msg}", ("addr", address_or_script)("msg", ss.str()));
   }
}

std::vector<btc_txout> bitcoin_rpc_client::listunspent() {
   const auto body = std::string("{\"jsonrpc\": \"1.0\", \"id\":\"pp_plugin\", \"method\": "
                                 "\"listunspent\", \"params\": [] }");

   const auto reply = send_post_request(body);

   std::vector<btc_txout> result;

   if (reply.body.empty()) {
      wlog("Bitcoin RPC call ${function} failed", ("function", __FUNCTION__));
      return result;
   }

   std::stringstream ss(std::string(reply.body.begin(), reply.body.end()));
   boost::property_tree::ptree json;
   boost::property_tree::read_json(ss, json);

   if (reply.status == 200) {
      if (json.count("result")) {
         for (auto &entry : json.get_child("result")) {
            btc_txout txo;
            txo.txid_ = entry.second.get_child("txid").get_value<std::string>();
            txo.out_num_ = entry.second.get_child("vout").get_value<unsigned int>();
            txo.amount_ = entry.second.get_child("amount").get_value<double>();
            result.push_back(txo);
         }
      }
   } else if (json.count("error") && !json.get_child("error").empty()) {
      wlog("Failed to list unspent txo! Reply: ${msg}", ("msg", ss.str()));
   }
   return result;
}

std::vector<btc_txout> bitcoin_rpc_client::listunspent_by_address_and_amount(const std::string &address, double minimum_amount) {
   std::string body = std::string("{\"jsonrpc\": \"1.0\", \"id\":\"pp_plugin\", \"method\": "
                                  "\"listunspent\", \"params\": [");
   body += std::string("1,999999,[\"");
   body += address;
   body += std::string("\"],true,{\"minimumAmount\":");
   body += std::to_string(minimum_amount);
   body += std::string("}] }");

   const auto reply = send_post_request(body);

   std::vector<btc_txout> result;
   if (reply.body.empty()) {
      wlog("Bitcoin RPC call ${function} failed", ("function", __FUNCTION__));
      return result;
   }

   std::stringstream ss(std::string(reply.body.begin(), reply.body.end()));
   boost::property_tree::ptree json;
   boost::property_tree::read_json(ss, json);

   if (reply.status == 200) {
      if (json.count("result")) {
         for (auto &entry : json.get_child("result")) {
            btc_txout txo;
            txo.txid_ = entry.second.get_child("txid").get_value<std::string>();
            txo.out_num_ = entry.second.get_child("vout").get_value<unsigned int>();
            txo.amount_ = entry.second.get_child("amount").get_value<double>();
            result.push_back(txo);
         }
      }
   } else if (json.count("error") && !json.get_child("error").empty()) {
      wlog("Failed to list unspent txo! Reply: ${msg}", ("msg", ss.str()));
   }
   return result;
}

void bitcoin_rpc_client::sendrawtransaction(const std::string &tx_hex) {
   const auto body = std::string("{\"jsonrpc\": \"1.0\", \"id\":\"sendrawtransaction\", "
                                 "\"method\": \"sendrawtransaction\", \"params\": [") +
                     std::string("\"") + tx_hex + std::string("\"") + std::string("] }");

   ilog(body);

   const auto reply = send_post_request(body);

   if (reply.body.empty()) {
      wlog("Bitcoin RPC call ${function} failed", ("function", __FUNCTION__));
      return;
   }

   std::stringstream ss(std::string(reply.body.begin(), reply.body.end()));
   boost::property_tree::ptree json;
   boost::property_tree::read_json(ss, json);

   if (reply.status == 200) {
      return;
   } else if (json.count("error") && !json.get_child("error").empty()) {
      const auto error_code = json.get_child("error").get_child("code").get_value<int>();
      if (error_code == -27) // transaction already in block chain
         return;

      wlog("BTC tx is not sent! Reply: ${msg}", ("msg", ss.str()));
   }
}

std::string bitcoin_rpc_client::signrawtransactionwithkey(const std::string &tx_hash, const std::string &private_key) {
   return "";
}

std::string bitcoin_rpc_client::signrawtransactionwithwallet(const std::string &tx_hash) {
   std::string body = std::string("{\"jsonrpc\": \"1.0\", \"id\":\"signrawtransactionwithwallet\", "
                                  "\"method\": \"signrawtransactionwithwallet\", \"params\": [");
   std::string params = "\"" + tx_hash + "\"";
   body = body + params + std::string("]}");

   ilog(body);

   const auto reply = send_post_request(body);

   if (reply.body.empty()) {
      wlog("Bitcoin RPC call ${function} failed", ("function", __FUNCTION__));
      return std::string();
   }

   std::stringstream ss(std::string(reply.body.begin(), reply.body.end()));
   boost::property_tree::ptree json;
   boost::property_tree::read_json(ss, json);

   if (reply.status == 200) {
      return ss.str();
   }

   if (json.count("error") && !json.get_child("error").empty()) {
      wlog("BTC sign_raw_transaction_with_wallet failed! Reply: ${msg}", ("msg", ss.str()));
   }
   return "";
}

fc::http::reply bitcoin_rpc_client::send_post_request( std::string body )
{
   fc::http::connection conn;
   conn.connect_to( fc::ip::endpoint( fc::ip::address( ip ), rpc_port ) );

   const auto url = "http://" + ip + ":" + std::to_string( rpc_port );

   return conn.request( "POST", url, body, fc::http::headers{authorization} );
}

// =============================================================================

zmq_listener::zmq_listener( std::string _ip, uint32_t _zmq ): ip( _ip ), zmq_port( _zmq ), ctx( 1 ), socket( ctx, ZMQ_SUB ) {
   std::thread( &zmq_listener::handle_zmq, this ).detach();
}

std::vector<zmq::message_t> zmq_listener::receive_multipart() {
   std::vector<zmq::message_t> msgs;

   int32_t more;
   size_t more_size = sizeof( more );
   while ( true ) {
      zmq::message_t msg;
      socket.recv( &msg, 0 );
      socket.getsockopt( ZMQ_RCVMORE, &more, &more_size );

      if ( !more )
         break;
      msgs.push_back( std::move(msg) );
   }

   return msgs;
}

void zmq_listener::handle_zmq() {
   socket.setsockopt( ZMQ_SUBSCRIBE, "hashblock", 9 );
   //socket.setsockopt( ZMQ_SUBSCRIBE, "hashtx", 6 );
   //socket.setsockopt( ZMQ_SUBSCRIBE, "rawblock", 8 );
   //socket.setsockopt( ZMQ_SUBSCRIBE, "rawtx", 5 );
   socket.connect( "tcp://" + ip + ":" + std::to_string( zmq_port ) );

   while ( true ) {
      auto msg = receive_multipart();
      const auto header = std::string( static_cast<char*>( msg[0].data() ), msg[0].size() );
      const auto block_hash = boost::algorithm::hex( std::string( static_cast<char*>( msg[1].data() ), msg[1].size() ) );

      event_received( block_hash );
   }
}

// =============================================================================

sidechain_net_handler_bitcoin::sidechain_net_handler_bitcoin(peerplays_sidechain_plugin& _plugin, const boost::program_options::variables_map& options) :
      sidechain_net_handler(_plugin, options) {
   sidechain = sidechain_type::bitcoin;

   ip = options.at("bitcoin-node-ip").as<std::string>();
   zmq_port = options.at("bitcoin-node-zmq-port").as<uint32_t>();
   rpc_port = options.at("bitcoin-node-rpc-port").as<uint32_t>();
   rpc_user = options.at("bitcoin-node-rpc-user").as<std::string>();
   rpc_password = options.at("bitcoin-node-rpc-password").as<std::string>();

   if( options.count("bitcoin-private-keys") )
   {
      const std::vector<std::string> pub_priv_keys = options["bitcoin-private-keys"].as<std::vector<std::string>>();
      for (const std::string& itr_key_pair : pub_priv_keys)
      {
         auto key_pair = graphene::app::dejsonify<std::pair<std::string, std::string> >(itr_key_pair, 5);
         ilog("Public Key: ${public}", ("public", key_pair.first));
         if(!key_pair.first.length() || !key_pair.second.length())
         {
            FC_THROW("Invalid public private key pair.");
         }
         _private_keys[key_pair.first] = key_pair.second;
      }
   }

   fc::http::connection conn;
   try {
      conn.connect_to( fc::ip::endpoint( fc::ip::address( ip ), rpc_port ) );
   } catch ( fc::exception e ) {
      elog( "No BTC node running at ${ip} or wrong rpc port: ${port}", ("ip", ip) ("port", rpc_port) );
      FC_ASSERT( false );
   }

   listener = std::unique_ptr<zmq_listener>( new zmq_listener( ip, zmq_port ) );
   bitcoin_client = std::unique_ptr<bitcoin_rpc_client>( new bitcoin_rpc_client( ip, rpc_port, rpc_user, rpc_password ) );

   listener->event_received.connect([this]( const std::string& event_data ) {
      std::thread( &sidechain_net_handler_bitcoin::handle_event, this, event_data ).detach();
   } );
}

sidechain_net_handler_bitcoin::~sidechain_net_handler_bitcoin() {
}

void sidechain_net_handler_bitcoin::recreate_primary_wallet() {
   const auto& swi = database.get_index_type<son_wallet_index>().indices().get<by_id>();
   const auto &active_sw = swi.rbegin();
   if (active_sw != swi.rend()) {

      if ((active_sw->addresses.find(sidechain_type::bitcoin) == active_sw->addresses.end()) ||
          (active_sw->addresses.at(sidechain_type::bitcoin).empty())) {

         const chain::global_property_object& gpo = database.get_global_properties();

         auto active_sons = gpo.active_sons;
         vector<string> son_pubkeys_bitcoin;
         for ( const son_info& si : active_sons ) {
            son_pubkeys_bitcoin.push_back(si.sidechain_public_keys.at(sidechain_type::bitcoin));
         }
         string reply_str = create_multisignature_wallet(son_pubkeys_bitcoin);

         ilog(reply_str);

         std::stringstream active_pw_ss(reply_str);
         boost::property_tree::ptree active_pw_pt;
         boost::property_tree::read_json( active_pw_ss, active_pw_pt );
         if( active_pw_pt.count( "error" ) && active_pw_pt.get_child( "error" ).empty() ) {

            std::stringstream res;
            boost::property_tree::json_parser::write_json(res, active_pw_pt.get_child("result"));

            son_wallet_update_operation op;
            op.payer = GRAPHENE_SON_ACCOUNT;
            op.son_wallet_id = (*active_sw).id;
            op.sidechain = sidechain_type::bitcoin;
            op.address = res.str();

            proposal_create_operation proposal_op;
            proposal_op.fee_paying_account = plugin.get_son_object(plugin.get_current_son_id()).son_account;
            proposal_op.proposed_ops.emplace_back( op_wrapper( op ) );
            uint32_t lifetime = ( gpo.parameters.block_interval * gpo.active_witnesses.size() ) * 3;
            proposal_op.expiration_time = time_point_sec( database.head_block_time().sec_since_epoch() + lifetime );

            signed_transaction trx = database.create_signed_transaction(plugin.get_private_key(plugin.get_current_son_id()), proposal_op);
            try {
               database.push_transaction(trx, database::validation_steps::skip_block_size_check);
               if(plugin.app().p2p_node())
                  plugin.app().p2p_node()->broadcast(net::trx_message(trx));
            } catch(fc::exception e){
               ilog("sidechain_net_handler:  sending proposal for son wallet update operation failed with exception ${e}",("e", e.what()));
               return;
            }

            const auto &prev_sw = std::next(active_sw);
            if (prev_sw != swi.rend()) {
               std::stringstream prev_sw_ss(prev_sw->addresses.at(sidechain_type::bitcoin));
               boost::property_tree::ptree prev_sw_pt;
               boost::property_tree::read_json( prev_sw_ss, prev_sw_pt );

               std::string active_pw_address = active_pw_pt.get_child("result").get<std::string>("address");
               std::string prev_pw_address = prev_sw_pt.get<std::string>("address");

               transfer_all_btc(prev_pw_address, active_pw_address);
            }
         }
      }
   }
}

void sidechain_net_handler_bitcoin::process_deposits() {
   sidechain_net_handler::process_deposits();
}

void sidechain_net_handler_bitcoin::process_deposit(const son_wallet_deposit_object &swdo) {
   ilog(__FUNCTION__);
   transfer_deposit_to_primary_wallet(swdo);
}

void sidechain_net_handler_bitcoin::process_withdrawals() {
   sidechain_net_handler::process_withdrawals();
}

void sidechain_net_handler_bitcoin::process_withdrawal(const son_wallet_withdraw_object &swwo) {
   ilog(__FUNCTION__);
   transfer_withdrawal_from_primary_wallet(swwo);
}

std::string sidechain_net_handler_bitcoin::create_multisignature_wallet( const std::vector<std::string> public_keys ) {
   return bitcoin_client->addmultisigaddress(public_keys);
}

std::string sidechain_net_handler_bitcoin::transfer( const std::string& from, const std::string& to, const uint64_t amount ) {
   return "";
}

std::string sidechain_net_handler_bitcoin::sign_transaction( const std::string& transaction ) {
   return "";
}

std::string sidechain_net_handler_bitcoin::send_transaction( const std::string& transaction ) {
   return "";
}

std::string sidechain_net_handler_bitcoin::sign_and_send_transaction_with_wallet ( const std::string& tx_json )
{
   std::string reply_str = tx_json;

   ilog(reply_str);

   std::stringstream ss_utx(reply_str);
   boost::property_tree::ptree pt;
   boost::property_tree::read_json( ss_utx, pt );

   if( !(pt.count( "error" ) && pt.get_child( "error" ).empty()) || !pt.count("result") ) {
      return "";
   }

   std::string unsigned_tx_hex = pt.get<std::string>("result");

   reply_str = bitcoin_client->signrawtransactionwithwallet(unsigned_tx_hex);
   ilog(reply_str);
   std::stringstream ss_stx(reply_str);
   boost::property_tree::ptree stx_json;
   boost::property_tree::read_json( ss_stx, stx_json );

   if( !(stx_json.count( "error" ) && stx_json.get_child( "error" ).empty()) || !stx_json.count("result") || !stx_json.get_child("result").count("hex") ) {
      return "";
   }

   std::string signed_tx_hex = stx_json.get<std::string>("result.hex");

   bitcoin_client->sendrawtransaction(signed_tx_hex);

   return reply_str;
}

std::string sidechain_net_handler_bitcoin::transfer_all_btc(const std::string& from_address, const std::string& to_address)
{
   uint64_t fee_rate = bitcoin_client->estimatesmartfee();
   uint64_t min_fee_rate = 1000;
   fee_rate = std::max(fee_rate, min_fee_rate);

   double min_amount = ((double)fee_rate/100000000.0); // Account only for relay fee for now
   double total_amount = 0.0;
   std::vector<btc_txout> unspent_utxo = bitcoin_client->listunspent_by_address_and_amount(from_address, 0);

   if(unspent_utxo.size() == 0)
   {
      wlog("Failed to find UTXOs to spend for ${pw}",("pw", from_address));
      return "";
   }
   else
   {
      for(const auto& utx: unspent_utxo)
      {
         total_amount += utx.amount_;
      }

      if(min_amount >= total_amount)
      {
         wlog("Failed not enough BTC to transfer from ${fa}",("fa", from_address));
         return "";
      }
   }

   fc::flat_map<std::string, double> outs;
   outs[to_address] = total_amount - min_amount;

   std::string reply_str = bitcoin_client->createrawtransaction(unspent_utxo, outs);
   return sign_and_send_transaction_with_wallet(reply_str);
}

std::string sidechain_net_handler_bitcoin::transfer_deposit_to_primary_wallet (const son_wallet_deposit_object &swdo)
{
   const auto& idx = database.get_index_type<son_wallet_index>().indices().get<by_id>();
   auto obj = idx.rbegin();
   if (obj == idx.rend() || obj->addresses.find(sidechain_type::bitcoin) == obj->addresses.end()) {
      return "";
   }

   std::string pw_address_json = obj->addresses.find(sidechain_type::bitcoin)->second;

   std::stringstream ss(pw_address_json);
   boost::property_tree::ptree json;
   boost::property_tree::read_json( ss, json );

   std::string pw_address = json.get<std::string>("address");

   std::string txid = swdo.sidechain_transaction_id;
   std::string suid = swdo.sidechain_uid;
   std::string nvout = suid.substr(suid.find_last_of("-")+1);
   uint64_t deposit_amount = swdo.sidechain_amount.value;
   uint64_t fee_rate = bitcoin_client->estimatesmartfee();
   uint64_t min_fee_rate = 1000;
   fee_rate = std::max(fee_rate, min_fee_rate);
   deposit_amount -= fee_rate; // Deduct minimum relay fee
   double transfer_amount = (double)deposit_amount/100000000.0;

   std::vector<btc_txout> ins;
   fc::flat_map<std::string, double> outs;

   btc_txout utxo;
   utxo.txid_ = txid;
   utxo.out_num_ = std::stoul(nvout);

   ins.push_back(utxo);

   outs[pw_address] = transfer_amount;

   std::string reply_str = bitcoin_client->createrawtransaction(ins, outs);
   return sign_and_send_transaction_with_wallet(reply_str);
}

std::string sidechain_net_handler_bitcoin::transfer_withdrawal_from_primary_wallet(const son_wallet_withdraw_object &swwo) {
   const auto& idx = database.get_index_type<son_wallet_index>().indices().get<by_id>();
   auto obj = idx.rbegin();
   if (obj == idx.rend() || obj->addresses.find(sidechain_type::bitcoin) == obj->addresses.end())
   {
      return "";
   }

   std::string pw_address_json = obj->addresses.find(sidechain_type::bitcoin)->second;

   std::stringstream ss(pw_address_json);
   boost::property_tree::ptree json;
   boost::property_tree::read_json( ss, json );

   std::string pw_address = json.get<std::string>("address");

   uint64_t fee_rate = bitcoin_client->estimatesmartfee();
   uint64_t min_fee_rate = 1000;
   fee_rate = std::max(fee_rate, min_fee_rate);

   double min_amount = ((double)(swwo.withdraw_amount.value + fee_rate) / 100000000.0); // Account only for relay fee for now
   double total_amount = 0.0;
   std::vector<btc_txout> unspent_utxo = bitcoin_client->listunspent_by_address_and_amount(pw_address, 0);

   if(unspent_utxo.size() == 0)
   {
      wlog("Failed to find UTXOs to spend for ${pw}",("pw", pw_address));
      return "";
   }
   else
   {
      for(const auto& utx: unspent_utxo)
      {
         total_amount += utx.amount_;
      }

      if(min_amount > total_amount)
      {
         wlog("Failed not enough BTC to spend for ${pw}",("pw", pw_address));
         return "";
      }
   }

   fc::flat_map<std::string, double> outs;
   outs[swwo.withdraw_address] = swwo.withdraw_amount.value / 100000000.0;
   if((total_amount - min_amount) > 0.0)
   {
      outs[pw_address] = total_amount - min_amount;
   }

   std::string reply_str = bitcoin_client->createrawtransaction(unspent_utxo, outs);
   return sign_and_send_transaction_with_wallet(reply_str);
}

void sidechain_net_handler_bitcoin::handle_event( const std::string& event_data ) {
   std::string block = bitcoin_client->getblock(event_data);
   if( block != "" ) {
      const auto& vins = extract_info_from_block( block );

      const auto& sidechain_addresses_idx = database.get_index_type<sidechain_address_index>().indices().get<by_sidechain_and_deposit_address>();

      for( const auto& v : vins ) {
         const auto& addr_itr = sidechain_addresses_idx.find(std::make_tuple(sidechain, v.address));
         if ( addr_itr == sidechain_addresses_idx.end() )
            continue;

         std::stringstream ss;
         ss << "bitcoin" << "-" << v.out.hash_tx << "-" << v.out.n_vout;
         std::string sidechain_uid = ss.str();

         sidechain_event_data sed;
         sed.timestamp = plugin.database().head_block_time();
         sed.sidechain = addr_itr->sidechain;
         sed.sidechain_uid = sidechain_uid;
         sed.sidechain_transaction_id = v.out.hash_tx;
         sed.sidechain_from = "";
         sed.sidechain_to = v.address;
         sed.sidechain_currency = "BTC";
         sed.sidechain_amount = v.out.amount;
         sed.peerplays_from = addr_itr->sidechain_address_account;
         sed.peerplays_to = GRAPHENE_SON_ACCOUNT;
         sed.peerplays_asset = asset(sed.sidechain_amount / 1000); // For Bitcoin, the exchange rate is 1:1, for others, get the exchange rate from market
         sidechain_event_data_received(sed);
      }
   }
}

std::vector<info_for_vin> sidechain_net_handler_bitcoin::extract_info_from_block( const std::string& _block ) {
   std::stringstream ss( _block );
   boost::property_tree::ptree block;
   boost::property_tree::read_json( ss, block );

   std::vector<info_for_vin> result;

   for (const auto& tx_child : block.get_child("tx")) {
      const auto& tx = tx_child.second;

      for ( const auto& o : tx.get_child("vout") ) {
         const auto script = o.second.get_child("scriptPubKey");

         if( !script.count("addresses") ) continue;

         for (const auto& addr : script.get_child("addresses")) { // in which cases there can be more addresses?
            const auto address_base58 = addr.second.get_value<std::string>();
            info_for_vin vin;
            vin.out.hash_tx = tx.get_child("txid").get_value<std::string>();
            string amount = o.second.get_child( "value" ).get_value<std::string>();
            amount.erase(std::remove(amount.begin(), amount.end(), '.'), amount.end());
            vin.out.amount = std::stoll(amount);
            vin.out.n_vout = o.second.get_child( "n" ).get_value<uint32_t>();
            vin.address = address_base58;
            result.push_back( vin );
         }
      }
   }

   return result;
}

// =============================================================================

} } // graphene::peerplays_sidechain

