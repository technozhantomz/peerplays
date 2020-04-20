#include <graphene/peerplays_sidechain/sidechain_net_handler_bitcoin.hpp>

#include <algorithm>
#include <thread>

#include <boost/algorithm/hex.hpp>
#include <boost/property_tree/json_parser.hpp>
#include <boost/property_tree/ptree.hpp>

#include <fc/crypto/base64.hpp>
#include <fc/log/logger.hpp>
#include <fc/network/ip.hpp>

#include <graphene/chain/account_object.hpp>
#include <graphene/chain/protocol/son_wallet.hpp>
#include <graphene/chain/sidechain_transaction_object.hpp>
#include <graphene/chain/son_info.hpp>
#include <graphene/chain/son_wallet_object.hpp>
#include <graphene/peerplays_sidechain/bitcoin/bitcoin_address.hpp>
#include <graphene/peerplays_sidechain/bitcoin/bitcoin_transaction.hpp>
#include <graphene/peerplays_sidechain/bitcoin/serialize.hpp>
#include <graphene/peerplays_sidechain/bitcoin/sign_bitcoin_transaction.hpp>
#include <graphene/utilities/key_conversion.hpp>

namespace graphene { namespace peerplays_sidechain {

// =============================================================================

bitcoin_rpc_client::bitcoin_rpc_client(std::string _ip, uint32_t _rpc, std::string _user, std::string _password, std::string _wallet, std::string _wallet_password) :
      ip(_ip),
      rpc_port(_rpc),
      user(_user),
      password(_password),
      wallet(_wallet),
      wallet_password(_wallet_password) {
   authorization.key = "Authorization";
   authorization.val = "Basic " + fc::base64_encode(user + ":" + password);
}

std::string bitcoin_rpc_client::addmultisigaddress(const uint32_t nrequired, const std::vector<std::string> public_keys) {
   std::string body = std::string("{\"jsonrpc\": \"1.0\", \"id\":\"addmultisigaddress\", "
                                  "\"method\": \"addmultisigaddress\", \"params\": [");
   std::string params = std::to_string(nrequired) + ", [";
   std::string pubkeys = "";
   for (std::string pubkey : public_keys) {
      if (!pubkeys.empty()) {
         pubkeys = pubkeys + ",";
      }
      pubkeys = pubkeys + std::string("\"") + pubkey + std::string("\"");
   }
   params = params + pubkeys + std::string("]");
   body = body + params + std::string(", null, \"bech32\"] }");

   const auto reply = send_post_request(body);

   if (reply.body.empty()) {
      wlog("Bitcoin RPC call ${function} failed", ("function", __FUNCTION__));
      return "";
   }

   std::stringstream ss(std::string(reply.body.begin(), reply.body.end()));
   boost::property_tree::ptree json;
   boost::property_tree::read_json(ss, json);

   if (reply.status == 200) {
      return ss.str();
   }

   if (json.count("error") && !json.get_child("error").empty()) {
      wlog("Bitcoin RPC call ${function} with body ${body} failed with reply '${msg}'", ("function", __FUNCTION__)("body", body)("msg", ss.str()));
   }
   return "";
}

std::string bitcoin_rpc_client::combinepsbt(const vector<std::string> &psbts) {
   std::string body = std::string("{\"jsonrpc\": \"1.0\", \"id\":\"combinepsbt\", \"method\": "
                                  "\"combinepsbt\", \"params\": [[");
   std::string params = "";
   for (std::string psbt : psbts) {
      if (!params.empty()) {
         params = params + ",";
      }
      params = params + std::string("\"") + psbt + std::string("\"");
   }
   body = body + params + std::string("]] }");
   const auto reply = send_post_request(body);

   if (reply.body.empty()) {
      wlog("Bitcoin RPC call ${function} failed", ("function", __FUNCTION__));
      return "";
   }

   std::stringstream ss(std::string(reply.body.begin(), reply.body.end()));
   boost::property_tree::ptree json;
   boost::property_tree::read_json(ss, json);

   if (reply.status == 200) {
      std::stringstream ss;
      boost::property_tree::json_parser::write_json(ss, json);
      return ss.str();
   }

   if (json.count("error") && !json.get_child("error").empty()) {
      wlog("Bitcoin RPC call ${function} with body ${body} failed with reply '${msg}'", ("function", __FUNCTION__)("body", body)("msg", ss.str()));
   }
   return "";
}

std::string bitcoin_rpc_client::createmultisig(const uint32_t nrequired, const std::vector<std::string> public_keys) {
   std::string body = std::string("{\"jsonrpc\": \"1.0\", \"id\":\"createmultisig\", "
                                  "\"method\": \"createmultisig\", \"params\": [");
   std::string params = std::to_string(nrequired) + ", [";
   std::string pubkeys = "";
   for (std::string pubkey : public_keys) {
      if (!pubkeys.empty()) {
         pubkeys = pubkeys + ",";
      }
      pubkeys = pubkeys + std::string("\"") + pubkey + std::string("\"");
   }
   params = params + pubkeys + std::string("]");
   body = body + params + std::string(", \"p2sh-segwit\" ] }");

   const auto reply = send_post_request(body);

   if (reply.body.empty()) {
      wlog("Bitcoin RPC call ${function} failed", ("function", __FUNCTION__));
      return "";
   }

   std::stringstream ss(std::string(reply.body.begin(), reply.body.end()));
   boost::property_tree::ptree json;
   boost::property_tree::read_json(ss, json);

   if (reply.status == 200) {
      return ss.str();
   }

   if (json.count("error") && !json.get_child("error").empty()) {
      wlog("Bitcoin RPC call ${function} with body ${body} failed with reply '${msg}'", ("function", __FUNCTION__)("body", body)("msg", ss.str()));
   }
   return "";
}

std::string bitcoin_rpc_client::createpsbt(const std::vector<btc_txout> &ins, const fc::flat_map<std::string, double> outs) {
   std::string body("{\"jsonrpc\": \"1.0\", \"id\":\"createpsbt\", "
                    "\"method\": \"createpsbt\", \"params\": [");
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

   const auto reply = send_post_request(body);

   if (reply.body.empty()) {
      wlog("Bitcoin RPC call ${function} failed", ("function", __FUNCTION__));
      return "";
   }

   std::stringstream ss(std::string(reply.body.begin(), reply.body.end()));
   boost::property_tree::ptree json;
   boost::property_tree::read_json(ss, json);

   if (reply.status == 200) {
      if (json.find("result") != json.not_found()) {
         return json.get<std::string>("result");
      }
   }

   if (json.count("error") && !json.get_child("error").empty()) {
      wlog("Bitcoin RPC call ${function} with body ${body} failed with reply '${msg}'", ("function", __FUNCTION__)("body", body)("msg", ss.str()));
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

   const auto reply = send_post_request(body);

   if (reply.body.empty()) {
      wlog("Bitcoin RPC call ${function} failed", ("function", __FUNCTION__));
      return "";
   }

   std::stringstream ss(std::string(reply.body.begin(), reply.body.end()));
   boost::property_tree::ptree json;
   boost::property_tree::read_json(ss, json);

   if (reply.status == 200) {
      if (json.find("result") != json.not_found()) {
         return json.get<std::string>("result");
      }
   }

   if (json.count("error") && !json.get_child("error").empty()) {
      wlog("Bitcoin RPC call ${function} with body ${body} failed with reply '${msg}'", ("function", __FUNCTION__)("body", body)("msg", ss.str()));
   }
   return "";
}

std::string bitcoin_rpc_client::createwallet(const std::string &wallet_name) {
   std::string body = std::string("{\"jsonrpc\": \"1.0\", \"id\":\"createwallet\", \"method\": "
                                  "\"createwallet\", \"params\": [\"" +
                                  wallet_name + "\"] }");

   const auto reply = send_post_request(body);

   if (reply.body.empty()) {
      wlog("Bitcoin RPC call ${function} failed", ("function", __FUNCTION__));
      return "";
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
      wlog("Bitcoin RPC call ${function} with body ${body} failed with reply '${msg}'", ("function", __FUNCTION__)("body", body)("msg", ss.str()));
   }
   return "";
}

std::string bitcoin_rpc_client::decodepsbt(std::string const &tx_psbt) {
   std::string body = std::string("{\"jsonrpc\": \"1.0\", \"id\":\"decodepsbt\", \"method\": "
                                  "\"decodepsbt\", \"params\": [\"" +
                                  tx_psbt + "\"] }");

   const auto reply = send_post_request(body);

   if (reply.body.empty()) {
      wlog("Bitcoin RPC call ${function} failed", ("function", __FUNCTION__));
      return "";
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
      wlog("Bitcoin RPC call ${function} with body ${body} failed with reply '${msg}'", ("function", __FUNCTION__)("body", body)("msg", ss.str()));
   }
   return "";
}

std::string bitcoin_rpc_client::decoderawtransaction(std::string const &tx_hex) {
   std::string body = std::string("{\"jsonrpc\": \"1.0\", \"id\":\"decoderawtransaction\", \"method\": "
                                  "\"decoderawtransaction\", \"params\": [\"" +
                                  tx_hex + "\"] }");

   const auto reply = send_post_request(body);

   if (reply.body.empty()) {
      wlog("Bitcoin RPC call ${function} failed", ("function", __FUNCTION__));
      return "";
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
      wlog("Bitcoin RPC call ${function} with body ${body} failed with reply '${msg}'", ("function", __FUNCTION__)("body", body)("msg", ss.str()));
   }
   return "";
}

std::string bitcoin_rpc_client::encryptwallet(const std::string &passphrase) {
   std::string body = std::string("{\"jsonrpc\": \"1.0\", \"id\":\"encryptwallet\", \"method\": "
                                  "\"encryptwallet\", \"params\": [\"" +
                                  passphrase + "\"] }");

   const auto reply = send_post_request(body);

   if (reply.body.empty()) {
      wlog("Bitcoin RPC call ${function} failed", ("function", __FUNCTION__));
      return "";
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
      wlog("Bitcoin RPC call ${function} with body ${body} failed with reply '${msg}'", ("function", __FUNCTION__)("body", body)("msg", ss.str()));
   }
   return "";
}

uint64_t bitcoin_rpc_client::estimatesmartfee(uint16_t conf_target) {
   const auto body = std::string("{\"jsonrpc\": \"1.0\", \"id\":\"estimatesmartfee\", "
                                 "\"method\": \"estimatesmartfee\", \"params\": [" +
                                 std::to_string(conf_target) + std::string("] }"));

   const auto reply = send_post_request(body);

   if (reply.body.empty()) {
      wlog("Bitcoin RPC call ${function} failed", ("function", __FUNCTION__));
      return 0;
   }

   std::stringstream ss(std::string(reply.body.begin(), reply.body.end()));
   boost::property_tree::ptree json;
   boost::property_tree::read_json(ss, json);

   if (reply.status == 200) {
      if (json.find("result") != json.not_found()) {
         auto json_result = json.get_child("result");
         if (json_result.find("feerate") != json_result.not_found()) {
            auto feerate_str = json_result.get<std::string>("feerate");
            feerate_str.erase(std::remove(feerate_str.begin(), feerate_str.end(), '.'), feerate_str.end());
            return std::stoll(feerate_str);
         }

         if (json_result.find("errors") != json_result.not_found()) {
            wlog("Bitcoin RPC call ${function} with body ${body} executed with reply '${msg}'", ("function", __FUNCTION__)("body", body)("msg", ss.str()));
         }
      }
   }

   if (json.count("error") && !json.get_child("error").empty()) {
      wlog("Bitcoin RPC call ${function} with body ${body} failed with reply '${msg}'", ("function", __FUNCTION__)("body", body)("msg", ss.str()));
   }
   return 20000;
}

std::string bitcoin_rpc_client::finalizepsbt(std::string const &tx_psbt) {
   std::string body = std::string("{\"jsonrpc\": \"1.0\", \"id\":\"finalizepsbt\", \"method\": "
                                  "\"finalizepsbt\", \"params\": [\"" +
                                  tx_psbt + "\"] }");

   const auto reply = send_post_request(body);

   if (reply.body.empty()) {
      wlog("Bitcoin RPC call ${function} failed", ("function", __FUNCTION__));
      return "";
   }

   std::stringstream ss(std::string(reply.body.begin(), reply.body.end()));
   boost::property_tree::ptree json;
   boost::property_tree::read_json(ss, json);

   if (reply.status == 200) {
      return ss.str();
   }

   if (json.count("error") && !json.get_child("error").empty()) {
      wlog("Bitcoin RPC call ${function} with body ${body} failed with reply '${msg}'", ("function", __FUNCTION__)("body", body)("msg", ss.str()));
   }
   return "";
}

std::string bitcoin_rpc_client::getaddressinfo(const std::string &address) {
   std::string body = std::string("{\"jsonrpc\": \"1.0\", \"id\":\"getaddressinfo\", \"method\": "
                                  "\"getaddressinfo\", \"params\": [\"" +
                                  address + "\"] }");

   const auto reply = send_post_request(body);

   if (reply.body.empty()) {
      wlog("Bitcoin RPC call ${function} failed", ("function", __FUNCTION__));
      return "";
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
      wlog("Bitcoin RPC call ${function} with body ${body} failed with reply '${msg}'", ("function", __FUNCTION__)("body", body)("msg", ss.str()));
   }
   return "";
}

std::string bitcoin_rpc_client::getblock(const std::string &block_hash, int32_t verbosity) {
   std::string body = std::string("{\"jsonrpc\": \"1.0\", \"id\":\"getblock\", \"method\": "
                                  "\"getblock\", \"params\": [\"" +
                                  block_hash + "\", " + std::to_string(verbosity) + "] }");

   const auto reply = send_post_request(body);

   if (reply.body.empty()) {
      wlog("Bitcoin RPC call ${function} failed", ("function", __FUNCTION__));
      return "";
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
      wlog("Bitcoin RPC call ${function} with body ${body} failed with reply '${msg}'", ("function", __FUNCTION__)("body", body)("msg", ss.str()));
   }
   return "";
}

std::string bitcoin_rpc_client::gettransaction(const std::string &txid, const bool include_watch_only) {
   std::string body = std::string("{\"jsonrpc\": \"1.0\", \"id\":\"gettransaction\", \"method\": "
                                  "\"gettransaction\", \"params\": [");

   std::string params = "\"" + txid + "\", " + (include_watch_only ? "true" : "false");
   body = body + params + "] }";

   const auto reply = send_post_request(body);

   if (reply.body.empty()) {
      wlog("Bitcoin RPC call ${function} failed", ("function", __FUNCTION__));
      return "";
   }

   std::stringstream ss(std::string(reply.body.begin(), reply.body.end()));
   boost::property_tree::ptree json;
   boost::property_tree::read_json(ss, json);

   if (reply.status == 200) {
      return ss.str();
   }

   if (json.count("error") && !json.get_child("error").empty()) {
      wlog("Bitcoin RPC call ${function} with body ${body} failed with reply '${msg}'", ("function", __FUNCTION__)("body", body)("msg", ss.str()));
   }
   return "";
}

void bitcoin_rpc_client::importaddress(const std::string &address_or_script, const std::string &label, const bool rescan, const bool p2sh) {
   std::string body = std::string("{\"jsonrpc\": \"1.0\", \"id\":\"importaddress\", "
                                  "\"method\": \"importaddress\", \"params\": [");

   std::string params = "\"" + address_or_script + "\", " +
                        "\"" + label + "\", " +
                        (rescan ? "true" : "false") + ", " +
                        (p2sh ? "true" : "false");
   body = body + params + "] }";

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
      wlog("Bitcoin RPC call ${function} with body ${body} failed with reply '${msg}'", ("function", __FUNCTION__)("body", body)("msg", ss.str()));
   }
}

std::vector<btc_txout> bitcoin_rpc_client::listunspent(const uint32_t minconf, const uint32_t maxconf) {
   const auto body = std::string("{\"jsonrpc\": \"1.0\", \"id\":\"pp_plugin\", \"method\": "
                                 "\"listunspent\", \"params\": [" +
                                 std::to_string(minconf) + "," + std::to_string(maxconf) + "] }");

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
            string amount = entry.second.get_child("amount").get_value<std::string>();
            amount.erase(std::remove(amount.begin(), amount.end(), '.'), amount.end());
            txo.amount_ = std::stoll(amount);
            result.push_back(txo);
         }
      }
   } else if (json.count("error") && !json.get_child("error").empty()) {
      wlog("Bitcoin RPC call ${function} with body ${body} failed with reply '${msg}'", ("function", __FUNCTION__)("body", body)("msg", ss.str()));
   }
   return result;
}

std::vector<btc_txout> bitcoin_rpc_client::listunspent_by_address_and_amount(const std::string &address, double minimum_amount, const uint32_t minconf, const uint32_t maxconf) {
   std::string body = std::string("{\"jsonrpc\": \"1.0\", \"id\":\"pp_plugin\", \"method\": "
                                  "\"listunspent\", \"params\": [" +
                                  std::to_string(minconf) + "," + std::to_string(maxconf) + ",");
   body += std::string("[\"");
   body += address;
   body += std::string("\"],true,{\"minimumAmount\":");
   body += std::to_string(minimum_amount);
   body += std::string("} ] }");

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
            string amount = entry.second.get_child("amount").get_value<std::string>();
            amount.erase(std::remove(amount.begin(), amount.end(), '.'), amount.end());
            txo.amount_ = std::stoll(amount);
            result.push_back(txo);
         }
      }
   } else if (json.count("error") && !json.get_child("error").empty()) {
      wlog("Bitcoin RPC call ${function} with body ${body} failed with reply '${msg}'", ("function", __FUNCTION__)("body", body)("msg", ss.str()));
   }
   return result;
}

std::string bitcoin_rpc_client::loadwallet(const std::string &filename) {
   std::string body = std::string("{\"jsonrpc\": \"1.0\", \"id\":\"loadwallet\", \"method\": "
                                  "\"loadwallet\", \"params\": [\"" +
                                  filename + "\"] }");

   const auto reply = send_post_request(body);

   if (reply.body.empty()) {
      wlog("Bitcoin RPC call ${function} failed", ("function", __FUNCTION__));
      return "";
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
      wlog("Bitcoin RPC call ${function} with body ${body} failed with reply '${msg}'", ("function", __FUNCTION__)("body", body)("msg", ss.str()));
   }
   return "";
}

std::string bitcoin_rpc_client::sendrawtransaction(const std::string &tx_hex) {
   const auto body = std::string("{\"jsonrpc\": \"1.0\", \"id\":\"sendrawtransaction\", "
                                 "\"method\": \"sendrawtransaction\", \"params\": [") +
                     std::string("\"") + tx_hex + std::string("\"") + std::string("] }");

   const auto reply = send_post_request(body);

   if (reply.body.empty()) {
      wlog("Bitcoin RPC call ${function} failed", ("function", __FUNCTION__));
      return "";
   }

   std::stringstream ss(std::string(reply.body.begin(), reply.body.end()));
   boost::property_tree::ptree json;
   boost::property_tree::read_json(ss, json);

   if (reply.status == 200) {
      return json.get<std::string>("result");
   }

   if (json.count("error") && !json.get_child("error").empty()) {
      wlog("Bitcoin RPC call ${function} with body ${body} failed with reply '${msg}'", ("function", __FUNCTION__)("body", body)("msg", ss.str()));
   }
   return "";
}

std::string bitcoin_rpc_client::signrawtransactionwithwallet(const std::string &tx_hash) {
   std::string body = std::string("{\"jsonrpc\": \"1.0\", \"id\":\"signrawtransactionwithwallet\", "
                                  "\"method\": \"signrawtransactionwithwallet\", \"params\": [");
   std::string params = "\"" + tx_hash + "\"";
   body = body + params + std::string("]}");

   const auto reply = send_post_request(body);

   if (reply.body.empty()) {
      wlog("Bitcoin RPC call ${function} failed", ("function", __FUNCTION__));
      return "";
   }

   std::stringstream ss(std::string(reply.body.begin(), reply.body.end()));
   boost::property_tree::ptree json;
   boost::property_tree::read_json(ss, json);

   if (reply.status == 200) {
      return ss.str();
   }

   if (json.count("error") && !json.get_child("error").empty()) {
      wlog("Bitcoin RPC call ${function} with body ${body} failed with reply '${msg}'", ("function", __FUNCTION__)("body", body)("msg", ss.str()));
   }
   return "";
}

std::string bitcoin_rpc_client::unloadwallet(const std::string &filename) {
   std::string body = std::string("{\"jsonrpc\": \"1.0\", \"id\":\"unloadwallet\", \"method\": "
                                  "\"unloadwallet\", \"params\": [\"" +
                                  filename + "\"] }");

   const auto reply = send_post_request(body);

   if (reply.body.empty()) {
      wlog("Bitcoin RPC call ${function} failed", ("function", __FUNCTION__));
      return "";
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
      wlog("Bitcoin RPC call ${function} with body ${body} failed with reply '${msg}'", ("function", __FUNCTION__)("body", body)("msg", ss.str()));
   }
   return "";
}

//std::string bitcoin_rpc_client::walletlock() {
//   std::string body = std::string("{\"jsonrpc\": \"1.0\", \"id\":\"walletlock\", \"method\": "
//                                  "\"walletlock\", \"params\": [] }");
//
//   const auto reply = send_post_request(body);
//
//   if (reply.body.empty()) {
//      wlog("Bitcoin RPC call ${function} failed", ("function", __FUNCTION__));
//      return "";
//   }
//
//   std::stringstream ss(std::string(reply.body.begin(), reply.body.end()));
//   boost::property_tree::ptree json;
//   boost::property_tree::read_json(ss, json);
//
//   if (reply.status == 200) {
//      std::stringstream ss;
//      boost::property_tree::json_parser::write_json(ss, json.get_child("result"));
//      return ss.str();
//   }
//
//   if (json.count("error") && !json.get_child("error").empty()) {
//      wlog("Bitcoin RPC call ${function} with body ${body} failed with reply '${msg}'", ("function", __FUNCTION__)("body", body)("msg", ss.str()));
//   }
//   return "";
//}

std::string bitcoin_rpc_client::walletprocesspsbt(std::string const &tx_psbt) {
   std::string body = std::string("{\"jsonrpc\": \"1.0\", \"id\":\"walletprocesspsbt\", \"method\": "
                                  "\"walletprocesspsbt\", \"params\": [\"" +
                                  tx_psbt + "\"] }");

   const auto reply = send_post_request(body);

   if (reply.body.empty()) {
      wlog("Bitcoin RPC call ${function} failed", ("function", __FUNCTION__));
      return "";
   }

   std::stringstream ss(std::string(reply.body.begin(), reply.body.end()));
   boost::property_tree::ptree json;
   boost::property_tree::read_json(ss, json);

   if (reply.status == 200) {
      return ss.str();
   }

   if (json.count("error") && !json.get_child("error").empty()) {
      wlog("Bitcoin RPC call ${function} with body ${body} failed with reply '${msg}'", ("function", __FUNCTION__)("body", body)("msg", ss.str()));
   }
   return "";
}

//bool bitcoin_rpc_client::walletpassphrase(const std::string &passphrase, uint32_t timeout) {
//   std::string body = std::string("{\"jsonrpc\": \"1.0\", \"id\":\"walletpassphrase\", \"method\": "
//                                  "\"walletpassphrase\", \"params\": [\"" +
//                                  passphrase + "\", " + std::to_string(timeout) + "] }");
//
//   const auto reply = send_post_request(body);
//
//   if (reply.body.empty()) {
//      wlog("Bitcoin RPC call ${function} failed", ("function", __FUNCTION__));
//      return false;
//   }
//
//   std::stringstream ss(std::string(reply.body.begin(), reply.body.end()));
//   boost::property_tree::ptree json;
//   boost::property_tree::read_json(ss, json);
//
//   if (reply.status == 200) {
//      return true;
//   }
//
//   if (json.count("error") && !json.get_child("error").empty()) {
//      wlog("Bitcoin RPC call ${function} with body ${body} failed with reply '${msg}'", ("function", __FUNCTION__)("body", body)("msg", ss.str()));
//   }
//   return false;
//}

fc::http::reply bitcoin_rpc_client::send_post_request(std::string body, bool show_log) {
   fc::http::connection conn;
   conn.connect_to(fc::ip::endpoint(fc::ip::address(ip), rpc_port));

   std::string url = "http://" + ip + ":" + std::to_string(rpc_port);

   if (wallet.length() > 0) {
      url = url + "/wallet/" + wallet;
   }

   fc::http::reply reply = conn.request("POST", url, body, fc::http::headers{authorization});

   if (show_log) {
      ilog("### Request URL:    ${url}", ("url", url));
      ilog("### Request:        ${body}", ("body", body));
      std::stringstream ss(std::string(reply.body.begin(), reply.body.end()));
      ilog("### Response:       ${ss}", ("ss", ss.str()));
   }

   return reply;
}

// =============================================================================

zmq_listener::zmq_listener(std::string _ip, uint32_t _zmq) :
      ip(_ip),
      zmq_port(_zmq),
      ctx(1),
      socket(ctx, ZMQ_SUB) {
   std::thread(&zmq_listener::handle_zmq, this).detach();
}

std::vector<zmq::message_t> zmq_listener::receive_multipart() {
   std::vector<zmq::message_t> msgs;

   int32_t more;
   size_t more_size = sizeof(more);
   while (true) {
      zmq::message_t msg;
      socket.recv(&msg, 0);
      socket.getsockopt(ZMQ_RCVMORE, &more, &more_size);

      if (!more)
         break;
      msgs.push_back(std::move(msg));
   }

   return msgs;
}

void zmq_listener::handle_zmq() {
   int linger = 0;
   socket.setsockopt(ZMQ_SUBSCRIBE, "hashblock", 9);
   socket.setsockopt(ZMQ_LINGER, &linger, sizeof(linger));
   //socket.setsockopt( ZMQ_SUBSCRIBE, "hashtx", 6 );
   //socket.setsockopt( ZMQ_SUBSCRIBE, "rawblock", 8 );
   //socket.setsockopt( ZMQ_SUBSCRIBE, "rawtx", 5 );
   socket.connect("tcp://" + ip + ":" + std::to_string(zmq_port));

   while (true) {
      try {
         auto msg = receive_multipart();
         const auto header = std::string(static_cast<char *>(msg[0].data()), msg[0].size());
         const auto block_hash = boost::algorithm::hex(std::string(static_cast<char *>(msg[1].data()), msg[1].size()));
         event_received(block_hash);
      } catch (zmq::error_t &e) {
      }
   }
}

// =============================================================================

sidechain_net_handler_bitcoin::sidechain_net_handler_bitcoin(peerplays_sidechain_plugin &_plugin, const boost::program_options::variables_map &options) :
      sidechain_net_handler(_plugin, options) {
   sidechain = sidechain_type::bitcoin;

   ip = options.at("bitcoin-node-ip").as<std::string>();
   zmq_port = options.at("bitcoin-node-zmq-port").as<uint32_t>();
   rpc_port = options.at("bitcoin-node-rpc-port").as<uint32_t>();
   rpc_user = options.at("bitcoin-node-rpc-user").as<std::string>();
   rpc_password = options.at("bitcoin-node-rpc-password").as<std::string>();
   wallet = "";
   if (options.count("bitcoin-wallet")) {
      wallet = options.at("bitcoin-wallet").as<std::string>();
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

   fc::http::connection conn;
   try {
      conn.connect_to(fc::ip::endpoint(fc::ip::address(ip), rpc_port));
   } catch (fc::exception e) {
      elog("No BTC node running at ${ip} or wrong rpc port: ${port}", ("ip", ip)("port", rpc_port));
      FC_ASSERT(false);
   }

   bitcoin_client = std::unique_ptr<bitcoin_rpc_client>(new bitcoin_rpc_client(ip, rpc_port, rpc_user, rpc_password, wallet, wallet_password));
   if (!wallet.empty()) {
      bitcoin_client->loadwallet(wallet);
   }

   listener = std::unique_ptr<zmq_listener>(new zmq_listener(ip, zmq_port));
   listener->event_received.connect([this](const std::string &event_data) {
      std::thread(&sidechain_net_handler_bitcoin::handle_event, this, event_data).detach();
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
      //Expected exception. Move along.
   } catch (fc::exception &e) {
      edump((e.to_detail_string()));
   }
}

bool sidechain_net_handler_bitcoin::process_proposal(const proposal_object &po) {

   ilog("Proposal to process: ${po}, SON id ${son_id}", ("po", po.id)("son_id", plugin.get_current_son_id()));

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
      const auto &idx = database.get_index_type<son_wallet_index>().indices().get<by_id>();
      const auto swo = idx.find(swo_id);
      if (swo != idx.end()) {

         auto active_sons = gpo.active_sons;
         vector<son_info> wallet_sons = swo->sons;

         bool son_sets_equal = (active_sons.size() == wallet_sons.size());

         if (son_sets_equal) {
            for (size_t i = 0; i < active_sons.size(); i++) {
               son_sets_equal = son_sets_equal && active_sons.at(i) == wallet_sons.at(i);
            }
         }

         if (son_sets_equal) {
            auto active_sons = gpo.active_sons;
            vector<string> son_pubkeys_bitcoin;
            for (const son_info &si : active_sons) {
               son_pubkeys_bitcoin.push_back(si.sidechain_public_keys.at(sidechain_type::bitcoin));
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
            object_id_type object_id = op_obj_idx_1.get<sidechain_transaction_create_operation>().object_id;
            std::string op_tx_str = op_obj_idx_1.get<sidechain_transaction_create_operation>().transaction;

            const auto &st_idx = database.get_index_type<sidechain_transaction_index>().indices().get<by_object_id>();
            const auto st = st_idx.find(object_id);
            if (st == st_idx.end()) {

               std::string tx_str = "";

               if (object_id.is<son_wallet_id_type>()) {
                  const auto &idx = database.get_index_type<son_wallet_index>().indices().get<by_id>();
                  const auto swo = idx.find(object_id);
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
         std::string swdo_address = swdo->sidechain_to;
         uint64_t swdo_amount = swdo->sidechain_amount.value;
         uint64_t swdo_vout = std::stoll(swdo->sidechain_uid.substr(swdo->sidechain_uid.find_last_of("-") + 1));

         std::string tx_str = bitcoin_client->gettransaction(swdo_txid, true);
         std::stringstream tx_ss(tx_str);
         boost::property_tree::ptree tx_json;
         boost::property_tree::read_json(tx_ss, tx_json);

         if (tx_json.count("error") && tx_json.get_child("error").empty()) {

            std::string tx_txid = tx_json.get<std::string>("result.txid");
            uint32_t tx_confirmations = tx_json.get<uint32_t>("result.confirmations");
            std::string tx_address = "";
            int64_t tx_amount = -1;
            int64_t tx_vout = -1;

            for (auto &input : tx_json.get_child("result.details")) {
               tx_address = input.second.get<std::string>("address");
               if ((tx_address == swdo_address) && (input.second.get<std::string>("category") == "receive")) {
                  std::string tx_amount_s = input.second.get<std::string>("amount");
                  tx_amount_s.erase(std::remove(tx_amount_s.begin(), tx_amount_s.end(), '.'), tx_amount_s.end());
                  tx_amount = std::stoll(tx_amount_s);
                  std::string tx_vout_s = input.second.get<std::string>("vout");
                  tx_vout = std::stoll(tx_vout_s);
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

      if ((active_sw->addresses.find(sidechain_type::bitcoin) == active_sw->addresses.end()) ||
          (active_sw->addresses.at(sidechain_type::bitcoin).empty())) {

         if (proposal_exists(chain::operation::tag<chain::son_wallet_update_operation>::value, active_sw->id)) {
            return;
         }

         const chain::global_property_object &gpo = database.get_global_properties();

         auto active_sons = gpo.active_sons;
         string reply_str = create_primary_wallet_address(active_sons);

         std::stringstream active_pw_ss(reply_str);
         boost::property_tree::ptree active_pw_pt;
         boost::property_tree::read_json(active_pw_ss, active_pw_pt);
         if (active_pw_pt.count("error") && active_pw_pt.get_child("error").empty()) {

            proposal_create_operation proposal_op;
            proposal_op.fee_paying_account = plugin.get_current_son_object().son_account;
            uint32_t lifetime = (gpo.parameters.block_interval * gpo.active_witnesses.size()) * 3;
            proposal_op.expiration_time = time_point_sec(database.head_block_time().sec_since_epoch() + lifetime);

            std::stringstream res;
            boost::property_tree::json_parser::write_json(res, active_pw_pt.get_child("result"));

            son_wallet_update_operation swu_op;
            swu_op.payer = gpo.parameters.son_account();
            swu_op.son_wallet_id = active_sw->id;
            swu_op.sidechain = sidechain_type::bitcoin;
            swu_op.address = res.str();

            proposal_op.proposed_ops.emplace_back(swu_op);

            const auto &prev_sw = std::next(active_sw);
            if (prev_sw != swi.rend()) {
               std::string new_pw_address = active_pw_pt.get<std::string>("result.address");
               std::string tx_str = create_primary_wallet_transaction(*prev_sw, new_pw_address);
               if (!tx_str.empty()) {
                  sidechain_transaction_create_operation stc_op;
                  stc_op.payer = gpo.parameters.son_account();
                  stc_op.object_id = prev_sw->id;
                  stc_op.sidechain = sidechain;
                  stc_op.transaction = tx_str;
                  stc_op.signers = prev_sw->sons;
                  proposal_op.proposed_ops.emplace_back(stc_op);
               }
            }

            signed_transaction trx = database.create_signed_transaction(plugin.get_private_key(plugin.get_current_son_id()), proposal_op);
            try {
               trx.validate();
               database.push_transaction(trx, database::validation_steps::skip_block_size_check);
               if (plugin.app().p2p_node())
                  plugin.app().p2p_node()->broadcast(net::trx_message(trx));
            } catch (fc::exception e) {
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
   for (auto &son : gpo.active_sons) {
      std::string pub_key_str = son.sidechain_public_keys.at(sidechain_type::bitcoin);
      auto pubkey = fc::ecc::public_key(create_public_key_data(parse_hex(pub_key_str)));
      pubkeys.push_back(std::make_pair(pubkey, son.weight));
   }

   const auto &sidechain_addresses_idx = database.get_index_type<sidechain_address_index>();
   const auto &sidechain_addresses_by_sidechain_idx = sidechain_addresses_idx.indices().get<by_sidechain>();
   const auto &sidechain_addresses_by_sidechain_range = sidechain_addresses_by_sidechain_idx.equal_range(sidechain);
   std::for_each(sidechain_addresses_by_sidechain_range.first, sidechain_addresses_by_sidechain_range.second,
                 [&](const sidechain_address_object &sao) {
                    auto usr_pubkey = fc::ecc::public_key(create_public_key_data(parse_hex(sao.deposit_public_key)));

                    btc_one_or_weighted_multisig_address addr(usr_pubkey, pubkeys);

                    if (addr.get_address() != sao.deposit_address) {
                       sidechain_address_update_operation op;
                       op.payer = plugin.get_current_son_object().son_account;
                       op.sidechain_address_id = sao.id;
                       op.sidechain_address_account = sao.sidechain_address_account;
                       op.sidechain = sao.sidechain;
                       op.deposit_public_key = sao.deposit_public_key;
                       op.deposit_address = addr.get_address();
                       op.withdraw_public_key = sao.withdraw_public_key;
                       op.withdraw_address = sao.withdraw_address;

                       signed_transaction trx = database.create_signed_transaction(plugin.get_private_key(plugin.get_current_son_id()), op);
                       try {
                          trx.validate();
                          database.push_transaction(trx, database::validation_steps::skip_block_size_check);
                          if (plugin.app().p2p_node())
                             plugin.app().p2p_node()->broadcast(net::trx_message(trx));
                          return true;
                       } catch (fc::exception e) {
                          elog("Sending proposal for deposit sidechain transaction create operation failed with exception ${e}", ("e", e.what()));
                          return false;
                       }
                    }
                 });
}

bool sidechain_net_handler_bitcoin::process_deposit(const son_wallet_deposit_object &swdo) {

   if (proposal_exists(chain::operation::tag<chain::sidechain_transaction_create_operation>::value, swdo.id)) {
      return false;
   }

   std::string tx_str = create_deposit_transaction(swdo);

   if (!tx_str.empty()) {
      const chain::global_property_object &gpo = database.get_global_properties();

      proposal_create_operation proposal_op;
      proposal_op.fee_paying_account = plugin.get_current_son_object().son_account;
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
      stc_op.signers = gpo.active_sons;
      proposal_op.proposed_ops.emplace_back(stc_op);

      signed_transaction trx = database.create_signed_transaction(plugin.get_private_key(plugin.get_current_son_id()), proposal_op);
      try {
         trx.validate();
         database.push_transaction(trx, database::validation_steps::skip_block_size_check);
         if (plugin.app().p2p_node())
            plugin.app().p2p_node()->broadcast(net::trx_message(trx));
         return true;
      } catch (fc::exception e) {
         elog("Sending proposal for deposit sidechain transaction create operation failed with exception ${e}", ("e", e.what()));
         return false;
      }
   }
   return false;
}

bool sidechain_net_handler_bitcoin::process_withdrawal(const son_wallet_withdraw_object &swwo) {

   if (proposal_exists(chain::operation::tag<chain::sidechain_transaction_create_operation>::value, swwo.id)) {
      return false;
   }

   std::string tx_str = create_withdrawal_transaction(swwo);

   if (!tx_str.empty()) {
      const chain::global_property_object &gpo = database.get_global_properties();

      proposal_create_operation proposal_op;
      proposal_op.fee_paying_account = plugin.get_current_son_object().son_account;
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
      stc_op.signers = gpo.active_sons;
      proposal_op.proposed_ops.emplace_back(stc_op);

      signed_transaction trx = database.create_signed_transaction(plugin.get_private_key(plugin.get_current_son_id()), proposal_op);
      try {
         trx.validate();
         database.push_transaction(trx, database::validation_steps::skip_block_size_check);
         if (plugin.app().p2p_node())
            plugin.app().p2p_node()->broadcast(net::trx_message(trx));
         return true;
      } catch (fc::exception e) {
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

int64_t sidechain_net_handler_bitcoin::settle_sidechain_transaction(const sidechain_transaction_object &sto) {

   if (sto.object_id.is<son_wallet_id_type>()) {
      return 0;
   }

   int64_t settle_amount = -1;

   if (sto.sidechain_transaction.empty()) {
      return settle_amount;
   }

   std::string tx_str = bitcoin_client->gettransaction(sto.sidechain_transaction, true);
   std::stringstream tx_ss(tx_str);
   boost::property_tree::ptree tx_json;
   boost::property_tree::read_json(tx_ss, tx_json);

   if ((tx_json.count("error")) && (!tx_json.get_child("error").empty())) {
      return settle_amount;
   }

   std::string tx_txid = tx_json.get<std::string>("result.txid");
   uint32_t tx_confirmations = tx_json.get<uint32_t>("result.confirmations");
   std::string tx_address = "";
   int64_t tx_amount = -1;

   const chain::global_property_object &gpo = database.get_global_properties();

   if (tx_confirmations >= gpo.parameters.son_bitcoin_min_tx_confirmations()) {
      if (sto.object_id.is<son_wallet_deposit_id_type>()) {
         for (auto &input : tx_json.get_child("result.details")) {
            if (input.second.get<std::string>("category") == "receive") {
               std::string tx_amount_s = input.second.get<std::string>("amount");
               tx_amount_s.erase(std::remove(tx_amount_s.begin(), tx_amount_s.end(), '.'), tx_amount_s.end());
               tx_amount = std::stoll(tx_amount_s);
               break;
            }
         }
         settle_amount = tx_amount;
      }

      if (sto.object_id.is<son_wallet_withdraw_id_type>()) {
         auto swwo = database.get<son_wallet_withdraw_object>(sto.object_id);
         settle_amount = swwo.withdraw_amount.value;
      }
   }
   return settle_amount;
}

std::string sidechain_net_handler_bitcoin::create_primary_wallet_address(const std::vector<son_info> &son_pubkeys) {
   using namespace bitcoin;

   std::vector<std::pair<fc::ecc::public_key, uint16_t>> pubkey_weights;
   for (auto &son : son_pubkeys) {
      std::string pub_key_str = son.sidechain_public_keys.at(sidechain_type::bitcoin);
      auto pub_key = fc::ecc::public_key(create_public_key_data(parse_hex(pub_key_str)));
      pubkey_weights.push_back(std::make_pair(pub_key, son.weight));
   }

   btc_weighted_multisig_address addr(pubkey_weights);

   std::stringstream ss;

   ss << "{\"result\": {\"address\": \"" << addr.get_address() << "\", \"redeemScript\": \"" << fc::to_hex(addr.get_redeem_script()) << "\""
      << "}, \"error\":null}";

   std::string res = ss.str();
   return res;
}

std::string sidechain_net_handler_bitcoin::create_primary_wallet_transaction(const son_wallet_object &prev_swo, std::string new_sw_address) {

   std::stringstream prev_sw_ss(prev_swo.addresses.find(sidechain_type::bitcoin)->second);
   boost::property_tree::ptree prev_sw_pt;
   boost::property_tree::read_json(prev_sw_ss, prev_sw_pt);
   std::string prev_pw_address = prev_sw_pt.get<std::string>("address");
   std::string prev_redeem_script = prev_sw_pt.get<std::string>("redeemScript");

   if (prev_pw_address == new_sw_address) {
      wlog("BTC previous and new primary wallet addresses are same. No funds moving needed [from ${prev_sw} to ${new_sw_address}]", ("prev_swo", prev_swo.id)("active_sw", new_sw_address));
      return "";
   }

   uint64_t fee_rate = bitcoin_client->estimatesmartfee();
   uint64_t min_fee_rate = 1000;
   fee_rate = std::max(fee_rate, min_fee_rate);

   uint64_t total_amount = 0.0;
   std::vector<btc_txout> inputs = bitcoin_client->listunspent_by_address_and_amount(prev_pw_address, 0);

   if (inputs.size() == 0) {
      elog("Failed to find UTXOs to spend for ${pw}", ("pw", prev_pw_address));
      return "";
   } else {
      for (const auto &utx : inputs) {
         total_amount += utx.amount_;
      }

      if (fee_rate >= total_amount) {
         elog("Failed not enough BTC to transfer from ${fa}", ("fa", prev_pw_address));
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
   if (obj == idx.rend() || obj->addresses.find(sidechain_type::bitcoin) == obj->addresses.end()) {
      return "";
   }
   //Get redeem script for deposit address
   std::string redeem_script = get_redeemscript_for_userdeposit(swdo.sidechain_to);
   std::string pw_address_json = obj->addresses.find(sidechain_type::bitcoin)->second;

   std::stringstream ss(pw_address_json);
   boost::property_tree::ptree json;
   boost::property_tree::read_json(ss, json);

   std::string pw_address = json.get<std::string>("address");

   std::string txid = swdo.sidechain_transaction_id;
   std::string suid = swdo.sidechain_uid;
   std::string nvout = suid.substr(suid.find_last_of("-") + 1);
   uint64_t deposit_amount = swdo.sidechain_amount.value;
   uint64_t fee_rate = bitcoin_client->estimatesmartfee();
   uint64_t min_fee_rate = 1000;
   fee_rate = std::max(fee_rate, min_fee_rate);
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
   if (obj == idx.rend() || obj->addresses.find(sidechain_type::bitcoin) == obj->addresses.end()) {
      return "";
   }

   std::string pw_address_json = obj->addresses.find(sidechain_type::bitcoin)->second;

   std::stringstream ss(pw_address_json);
   boost::property_tree::ptree json;
   boost::property_tree::read_json(ss, json);

   std::string pw_address = json.get<std::string>("address");
   std::string redeem_script = json.get<std::string>("redeemScript");

   uint64_t fee_rate = bitcoin_client->estimatesmartfee();
   uint64_t min_fee_rate = 1000;
   fee_rate = std::max(fee_rate, min_fee_rate);

   uint64_t total_amount = 0;
   std::vector<btc_txout> inputs = bitcoin_client->listunspent_by_address_and_amount(pw_address, 0);

   if (inputs.size() == 0) {
      elog("Failed to find UTXOs to spend for ${pw}", ("pw", pw_address));
      return "";
   } else {
      for (const auto &utx : inputs) {
         total_amount += utx.amount_;
      }

      if ((swwo.withdraw_amount.value > total_amount) || (fee_rate >= swwo.withdraw_amount.value)) {
         elog("Failed not enough BTC to spend for ${pw}", ("pw", pw_address));
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
   std::string pubkey = plugin.get_current_son_object().sidechain_public_keys.at(sidechain);
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
   //Organise weighted address signatures
   //Add dummies for empty signatures
   vector<vector<bitcoin::bytes>> signatures;
   for (unsigned idx = 0; idx < sto.signatures.size(); ++idx) {
      if (sto.signatures[idx].second.empty())
         signatures.push_back(dummy);
      else
         signatures.push_back(read_byte_arrays_from_string(sto.signatures[idx].second));
   }
   //Add empty sig for user signature for Deposit transaction
   if (sto.object_id.type() == son_wallet_deposit_object::type_id) {
      add_signatures_to_transaction_user_weighted_multisig(tx, signatures);
   } else {
      add_signatures_to_transaction_weighted_multisig(tx, signatures);
   }
   //Add redeemscripts to vins and make tx ready for sending
   sign_witness_transaction_finalize(tx, redeem_scripts, false);
   std::string final_tx_hex = fc::to_hex(pack(tx));
   std::string res = bitcoin_client->sendrawtransaction(final_tx_hex);

   return res;
}

void sidechain_net_handler_bitcoin::handle_event(const std::string &event_data) {
   std::string block = bitcoin_client->getblock(event_data);
   if (block != "") {
      const auto &vins = extract_info_from_block(block);

      const auto &sidechain_addresses_idx = database.get_index_type<sidechain_address_index>().indices().get<by_sidechain_and_deposit_address>();

      for (const auto &v : vins) {
         // !!! EXTRACT DEPOSIT ADDRESS FROM SIDECHAIN ADDRESS OBJECT
         const auto &addr_itr = sidechain_addresses_idx.find(std::make_tuple(sidechain, v.address));
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
         sed.sidechain_uid = sidechain_uid;
         sed.sidechain_transaction_id = v.out.hash_tx;
         sed.sidechain_from = "";
         sed.sidechain_to = v.address;
         sed.sidechain_currency = "BTC";
         sed.sidechain_amount = v.out.amount;
         sed.peerplays_from = addr_itr->sidechain_address_account;
         sed.peerplays_to = database.get_global_properties().parameters.son_account();
         price btc_price = database.get<asset_object>(database.get_global_properties().parameters.btc_asset()).options.core_exchange_rate;
         sed.peerplays_asset = asset(sed.sidechain_amount * btc_price.base.amount / btc_price.quote.amount);
         sidechain_event_data_received(sed);
      }
   }
}

std::string sidechain_net_handler_bitcoin::get_redeemscript_for_userdeposit(const std::string &user_address) {
   using namespace bitcoin;
   const auto &sidechain_addresses_idx = database.get_index_type<sidechain_address_index>().indices().get<by_sidechain_and_deposit_address>();
   const auto &addr_itr = sidechain_addresses_idx.find(std::make_tuple(sidechain, user_address));
   if (addr_itr == sidechain_addresses_idx.end()) {
      return "";
   }

   const auto &idx = database.get_index_type<son_wallet_index>().indices().get<by_id>();
   auto obj = idx.rbegin();
   if (obj == idx.rend() || obj->addresses.find(sidechain_type::bitcoin) == obj->addresses.end()) {
      return "";
   }

   std::vector<std::pair<fc::ecc::public_key, uint16_t>> pubkey_weights;
   for (auto &son : obj->sons) {
      std::string pub_key_str = son.sidechain_public_keys.at(sidechain_type::bitcoin);
      auto pub_key = fc::ecc::public_key(create_public_key_data(parse_hex(pub_key_str)));
      pubkey_weights.push_back(std::make_pair(pub_key, son.weight));
   }
   auto user_pub_key = fc::ecc::public_key(create_public_key_data(parse_hex(addr_itr->deposit_public_key)));
   btc_one_or_weighted_multisig_address deposit_addr(user_pub_key, pubkey_weights);
   return fc::to_hex(deposit_addr.get_redeem_script());
}

std::vector<info_for_vin> sidechain_net_handler_bitcoin::extract_info_from_block(const std::string &_block) {
   std::stringstream ss(_block);
   boost::property_tree::ptree block;
   boost::property_tree::read_json(ss, block);

   std::vector<info_for_vin> result;

   for (const auto &tx_child : block.get_child("tx")) {
      const auto &tx = tx_child.second;

      for (const auto &o : tx.get_child("vout")) {
         const auto script = o.second.get_child("scriptPubKey");

         if (!script.count("addresses"))
            continue;

         for (const auto &addr : script.get_child("addresses")) { // in which cases there can be more addresses?
            const auto address_base58 = addr.second.get_value<std::string>();
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
               bitcoin_client->importaddress(pw_address);
            }

            if (pw_pt.count("redeemScript")) {
               std::string pw_redeem_script = pw_pt.get<std::string>("redeemScript");
               bitcoin_client->importaddress(pw_redeem_script, "", true, true);
            }
         }
      }
   }
}

// =============================================================================
}} // namespace graphene::peerplays_sidechain
