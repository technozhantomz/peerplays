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
#include <graphene/chain/son_info.hpp>
#include <graphene/chain/son_wallet_object.hpp>

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
   body = body + params + std::string("] }");

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
      return;
   } else if (json.count("error") && !json.get_child("error").empty()) {
      wlog("Bitcoin RPC call ${function} with body ${body} failed with reply '${msg}'", ("function", __FUNCTION__)("body", body)("msg", ss.str()));
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
      wlog("Bitcoin RPC call ${function} with body ${body} failed with reply '${msg}'", ("function", __FUNCTION__)("body", body)("msg", ss.str()));
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

bool bitcoin_rpc_client::sendrawtransaction(const std::string &tx_hex) {
   const auto body = std::string("{\"jsonrpc\": \"1.0\", \"id\":\"sendrawtransaction\", "
                                 "\"method\": \"sendrawtransaction\", \"params\": [") +
                     std::string("\"") + tx_hex + std::string("\"") + std::string("] }");

   const auto reply = send_post_request(body);

   if (reply.body.empty()) {
      wlog("Bitcoin RPC call ${function} failed", ("function", __FUNCTION__));
      return false;
   }

   std::stringstream ss(std::string(reply.body.begin(), reply.body.end()));
   boost::property_tree::ptree json;
   boost::property_tree::read_json(ss, json);

   if (reply.status == 200) {
      return true;
   } else if (json.count("error") && !json.get_child("error").empty()) {
      const auto error_code = json.get_child("error").get_child("code").get_value<int>();
      if (error_code == -27) // transaction already in block chain
         return true;
      wlog("Bitcoin RPC call ${function} with body ${body} failed with reply '${msg}'", ("function", __FUNCTION__)("body", body)("msg", ss.str()));
   }
   return false;
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

std::string bitcoin_rpc_client::walletlock() {
   std::string body = std::string("{\"jsonrpc\": \"1.0\", \"id\":\"walletlock\", \"method\": "
                                  "\"walletlock\", \"params\": [] }");

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

bool bitcoin_rpc_client::walletpassphrase(const std::string &passphrase, uint32_t timeout) {
   std::string body = std::string("{\"jsonrpc\": \"1.0\", \"id\":\"walletpassphrase\", \"method\": "
                                  "\"walletpassphrase\", \"params\": [\"" +
                                  passphrase + "\", " + std::to_string(timeout) + "] }");

   const auto reply = send_post_request(body);

   if (reply.body.empty()) {
      wlog("Bitcoin RPC call ${function} failed", ("function", __FUNCTION__));
      return false;
   }

   std::stringstream ss(std::string(reply.body.begin(), reply.body.end()));
   boost::property_tree::ptree json;
   boost::property_tree::read_json(ss, json);

   if (reply.status == 200) {
      return true;
   }

   if (json.count("error") && !json.get_child("error").empty()) {
      wlog("Bitcoin RPC call ${function} with body ${body} failed with reply '${msg}'", ("function", __FUNCTION__)("body", body)("msg", ss.str()));
   }
   return false;
}

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

void sidechain_net_handler_bitcoin::recreate_primary_wallet() {
   const auto &swi = database.get_index_type<son_wallet_index>().indices().get<by_id>();
   const auto &active_sw = swi.rbegin();
   if (active_sw != swi.rend()) {

      if ((active_sw->addresses.find(sidechain_type::bitcoin) == active_sw->addresses.end()) ||
          (active_sw->addresses.at(sidechain_type::bitcoin).empty())) {

         const chain::global_property_object &gpo = database.get_global_properties();

         auto active_sons = gpo.active_sons;
         vector<string> son_pubkeys_bitcoin;
         for (const son_info &si : active_sons) {
            son_pubkeys_bitcoin.push_back(si.sidechain_public_keys.at(sidechain_type::bitcoin));
         }

         if (!wallet_password.empty()) {
            bitcoin_client->walletpassphrase(wallet_password, 5);
         }
         uint32_t nrequired = son_pubkeys_bitcoin.size() * 2 / 3 + 1;
         string reply_str = bitcoin_client->addmultisigaddress(nrequired, son_pubkeys_bitcoin);

         std::stringstream active_pw_ss(reply_str);
         boost::property_tree::ptree active_pw_pt;
         boost::property_tree::read_json(active_pw_ss, active_pw_pt);
         if (active_pw_pt.count("error") && active_pw_pt.get_child("error").empty()) {

            std::stringstream res;
            boost::property_tree::json_parser::write_json(res, active_pw_pt.get_child("result"));

            son_wallet_update_operation op;
            op.payer = gpo.parameters.son_account();
            op.son_wallet_id = active_sw->id;
            op.sidechain = sidechain_type::bitcoin;
            op.address = res.str();

            proposal_create_operation proposal_op;
            proposal_op.fee_paying_account = plugin.get_current_son_object().son_account;
            proposal_op.proposed_ops.emplace_back(op);
            uint32_t lifetime = (gpo.parameters.block_interval * gpo.active_witnesses.size()) * 3;
            proposal_op.expiration_time = time_point_sec(database.head_block_time().sec_since_epoch() + lifetime);

            signed_transaction trx = database.create_signed_transaction(plugin.get_private_key(plugin.get_current_son_id()), proposal_op);
            try {
               database.push_transaction(trx, database::validation_steps::skip_block_size_check);
               if (plugin.app().p2p_node())
                  plugin.app().p2p_node()->broadcast(net::trx_message(trx));
            } catch (fc::exception e) {
               elog("Sending proposal for son wallet update operation failed with exception ${e}", ("e", e.what()));
               return;
            }

            const auto &prev_sw = std::next(active_sw);
            if (prev_sw != swi.rend()) {
               std::stringstream prev_sw_ss(prev_sw->addresses.at(sidechain_type::bitcoin));
               boost::property_tree::ptree prev_sw_pt;
               boost::property_tree::read_json(prev_sw_ss, prev_sw_pt);

               std::string active_pw_address = active_pw_pt.get_child("result").get<std::string>("address");
               std::string prev_pw_address = prev_sw_pt.get<std::string>("address");

               if (prev_pw_address == active_pw_address) {
                  elog("BTC previous and new primary wallet addresses are same. No funds moving needed [from ${prev_sw} to ${active_sw}]", ("prev_sw", prev_sw->id)("active_sw", active_sw->id));
                  return;
               }

               uint64_t fee_rate = bitcoin_client->estimatesmartfee();
               uint64_t min_fee_rate = 1000;
               fee_rate = std::max(fee_rate, min_fee_rate);

               double min_amount = ((double)fee_rate / 100000000.0); // Account only for relay fee for now
               double total_amount = 0.0;
               std::vector<btc_txout> inputs = bitcoin_client->listunspent_by_address_and_amount(prev_pw_address, 0);

               if (inputs.size() == 0) {
                  elog("Failed to find UTXOs to spend for ${pw}", ("pw", prev_pw_address));
                  return;
               } else {
                  for (const auto &utx : inputs) {
                     total_amount += utx.amount_;
                  }

                  if (min_amount >= total_amount) {
                     elog("Failed not enough BTC to transfer from ${fa}", ("fa", prev_pw_address));
                     return;
                  }
               }

               fc::flat_map<std::string, double> outputs;
               outputs[active_pw_address] = total_amount - min_amount;

               std::string tx_str = create_transaction(inputs, outputs);

               if (!tx_str.empty()) {

                  sidechain_transaction_create_operation stc_op;
                  stc_op.payer = gpo.parameters.son_account();
                  stc_op.object_id = prev_sw->id;
                  stc_op.sidechain = sidechain;
                  stc_op.transaction = tx_str;
                  stc_op.signers = prev_sw->sons;

                  proposal_create_operation proposal_op;
                  proposal_op.fee_paying_account = plugin.get_current_son_object().son_account;
                  proposal_op.proposed_ops.emplace_back(stc_op);
                  uint32_t lifetime = (gpo.parameters.block_interval * gpo.active_witnesses.size()) * 3;
                  proposal_op.expiration_time = time_point_sec(database.head_block_time().sec_since_epoch() + lifetime);

                  signed_transaction trx = database.create_signed_transaction(plugin.get_private_key(plugin.get_current_son_id()), proposal_op);
                  trx.validate();
                  try {
                     database.push_transaction(trx, database::validation_steps::skip_block_size_check);
                     if (plugin.app().p2p_node())
                        plugin.app().p2p_node()->broadcast(net::trx_message(trx));
                  } catch (fc::exception e) {
                     elog("Sending proposal for withdrawal sidechain transaction create operation failed with exception ${e}", ("e", e.what()));
                  }
               }
            }
         }
      }
   }
}

bool sidechain_net_handler_bitcoin::process_deposit(const son_wallet_deposit_object &swdo) {
   const auto &idx = database.get_index_type<son_wallet_index>().indices().get<by_id>();
   auto obj = idx.rbegin();
   if (obj == idx.rend() || obj->addresses.find(sidechain_type::bitcoin) == obj->addresses.end()) {
      return false;
   }

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

   inputs.push_back(utxo);

   outputs[pw_address] = transfer_amount;

   std::string tx_str = create_transaction(inputs, outputs);

   if (!tx_str.empty()) {
      const chain::global_property_object &gpo = database.get_global_properties();

      sidechain_transaction_create_operation stc_op;
      stc_op.payer = gpo.parameters.son_account();
      stc_op.object_id = swdo.id;
      stc_op.sidechain = sidechain;
      stc_op.transaction = tx_str;
      stc_op.signers = gpo.active_sons;

      proposal_create_operation proposal_op;
      proposal_op.fee_paying_account = plugin.get_current_son_object().son_account;
      proposal_op.proposed_ops.emplace_back(stc_op);
      uint32_t lifetime = (gpo.parameters.block_interval * gpo.active_witnesses.size()) * 3;
      proposal_op.expiration_time = time_point_sec(database.head_block_time().sec_since_epoch() + lifetime);

      signed_transaction trx = database.create_signed_transaction(plugin.get_private_key(plugin.get_current_son_id()), proposal_op);
      trx.validate();
      try {
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
   const auto &idx = database.get_index_type<son_wallet_index>().indices().get<by_id>();
   auto obj = idx.rbegin();
   if (obj == idx.rend() || obj->addresses.find(sidechain_type::bitcoin) == obj->addresses.end()) {
      return false;
   }

   std::string pw_address_json = obj->addresses.find(sidechain_type::bitcoin)->second;

   std::stringstream ss(pw_address_json);
   boost::property_tree::ptree json;
   boost::property_tree::read_json(ss, json);

   std::string pw_address = json.get<std::string>("address");

   uint64_t fee_rate = bitcoin_client->estimatesmartfee();
   uint64_t min_fee_rate = 1000;
   fee_rate = std::max(fee_rate, min_fee_rate);

   double min_amount = ((double)(swwo.withdraw_amount.value + fee_rate) / 100000000.0); // Account only for relay fee for now
   double total_amount = 0.0;
   std::vector<btc_txout> inputs = bitcoin_client->listunspent_by_address_and_amount(pw_address, 0);

   if (inputs.size() == 0) {
      elog("Failed to find UTXOs to spend for ${pw}", ("pw", pw_address));
      return "";
   } else {
      for (const auto &utx : inputs) {
         total_amount += utx.amount_;
      }

      if (min_amount > total_amount) {
         elog("Failed not enough BTC to spend for ${pw}", ("pw", pw_address));
         return "";
      }
   }

   fc::flat_map<std::string, double> outputs;
   outputs[swwo.withdraw_address] = swwo.withdraw_amount.value / 100000000.0;
   if ((total_amount - min_amount) > 0.0) {
      outputs[pw_address] = total_amount - min_amount;
   }

   std::string tx_str = create_transaction(inputs, outputs);

   if (!tx_str.empty()) {
      const chain::global_property_object &gpo = database.get_global_properties();

      sidechain_transaction_create_operation stc_op;
      stc_op.payer = gpo.parameters.son_account();
      stc_op.object_id = swwo.id;
      stc_op.sidechain = sidechain;
      stc_op.transaction = tx_str;
      stc_op.signers = gpo.active_sons;

      proposal_create_operation proposal_op;
      proposal_op.fee_paying_account = plugin.get_current_son_object().son_account;
      proposal_op.proposed_ops.emplace_back(stc_op);
      uint32_t lifetime = (gpo.parameters.block_interval * gpo.active_witnesses.size()) * 3;
      proposal_op.expiration_time = time_point_sec(database.head_block_time().sec_since_epoch() + lifetime);

      signed_transaction trx = database.create_signed_transaction(plugin.get_private_key(plugin.get_current_son_id()), proposal_op);
      trx.validate();
      try {
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

std::string sidechain_net_handler_bitcoin::process_sidechain_transaction(const sidechain_transaction_object &sto, bool &complete) {
   complete = false;

   //// Uncomment to get signing in order from sto.signers
   //son_id_type invalid_signer = son_id_type(0xFFFFFFFF);
   //son_id_type next_signer = invalid_signer;
   //for (auto &signer : sto.signers) {
   //   if (signer.second == false) {
   //      next_signer = signer.first;
   //      break;
   //   }
   //}
   //
   //if ((next_signer == invalid_signer) || (next_signer != plugin.get_current_son_id())) {
   //   return "";
   //}

   return sign_transaction(sto, complete);
}

bool sidechain_net_handler_bitcoin::send_sidechain_transaction(const sidechain_transaction_object &sto, std::string &sidechain_transaction) {
   sidechain_transaction = "";

   return send_transaction(sto, sidechain_transaction);
}

// Creates transaction in any format
// Function to actually create transaction should return transaction string, or empty string in case of failure
std::string sidechain_net_handler_bitcoin::create_transaction(const std::vector<btc_txout> &inputs, const fc::flat_map<std::string, double> outputs) {
   std::string new_tx = "";
   //new_tx = create_transaction_raw(inputs, outputs);
   new_tx = create_transaction_psbt(inputs, outputs);
   //new_tx = create_transaction_standalone(inputs, outputs);
   return new_tx;
}

// Adds signature to transaction
// Function to actually add signature should return transaction with added signature string, or empty string in case of failure
std::string sidechain_net_handler_bitcoin::sign_transaction(const sidechain_transaction_object &sto, bool &complete) {
   complete = false;
   std::string new_tx = "";
   //new_tx = sign_transaction_raw(sto, complete);
   new_tx = sign_transaction_psbt(sto, complete);
   //new_tx = sign_transaction_standalone(sto, complete);
   return new_tx;
}

bool sidechain_net_handler_bitcoin::send_transaction(const sidechain_transaction_object &sto, std::string &sidechain_transaction) {
   sidechain_transaction = "";
   //return send_transaction_raw(sto, sidechain_transaction);
   return send_transaction_psbt(sto, sidechain_transaction);
}

std::string sidechain_net_handler_bitcoin::create_transaction_raw(const std::vector<btc_txout> &inputs, const fc::flat_map<std::string, double> outputs) {
   return bitcoin_client->createrawtransaction(inputs, outputs);
}

std::string sidechain_net_handler_bitcoin::create_transaction_psbt(const std::vector<btc_txout> &inputs, const fc::flat_map<std::string, double> outputs) {
   return bitcoin_client->createpsbt(inputs, outputs);
}

std::string sidechain_net_handler_bitcoin::create_transaction_standalone(const std::vector<btc_txout> &inputs, const fc::flat_map<std::string, double> outputs) {
   // Examples

   // Transaction with no inputs and outputs
   //bitcoin-core.cli -rpcuser=1 -rpcpassword=1 -rpcwallet="" createrawtransaction '[]' '[]'
   //02000000000000000000
   //bitcoin-core.cli -rpcuser=1 -rpcpassword=1 -rpcwallet="" decoderawtransaction 02000000000000000000
   //{
   //  "txid": "4ebd325a4b394cff8c57e8317ccf5a8d0e2bdf1b8526f8aad6c8e43d8240621a",
   //  "hash": "4ebd325a4b394cff8c57e8317ccf5a8d0e2bdf1b8526f8aad6c8e43d8240621a",
   //  "version": 2,
   //  "size": 10,
   //  "vsize": 10,
   //  "weight": 40,
   //  "locktime": 0,
   //  "vin": [
   //  ],
   //  "vout": [
   //  ]
   //}

   // Transaction with input and output
   //{
   //  "txid": "ff60f48f767bbf70d79efc1347b5554b481f14fda68709839091286e035e669b",
   //  "hash": "ff60f48f767bbf70d79efc1347b5554b481f14fda68709839091286e035e669b",
   //  "version": 2,
   //  "size": 83,
   //  "vsize": 83,
   //  "weight": 332,
   //  "locktime": 0,
   //  "vin": [
   //    {
   //      "txid": "3d322dc2640239a2e68e182b254d19c88e5172a61947f94a105c3f57618092ff",
   //      "vout": 0,
   //      "scriptSig": {
   //        "asm": "",
   //        "hex": ""
   //      },
   //      "sequence": 4294967295
   //    }
   //  ],
   //  "vout": [
   //    {
   //      "value": 1.00000000,
   //      "n": 0,
   //      "scriptPubKey": {
   //        "asm": "OP_HASH160 b87c323018cae236eb03a1f63000c85b672270f6 OP_EQUAL",
   //        "hex": "a914b87c323018cae236eb03a1f63000c85b672270f687",
   //        "reqSigs": 1,
   //        "type": "scripthash",
   //        "addresses": [
   //          "2NA4h6sc9oZ4ogfNKU9Wp6fkqPZLZPqqpgf"
   //        ]
   //      }
   //    }
   //  ]
   //}

   return "";
}

std::string sidechain_net_handler_bitcoin::sign_transaction_raw(const sidechain_transaction_object &sto, bool &complete) {
   complete = false;

   if (sto.transaction.empty()) {
      elog("Signing failed, tx string is empty");
      return "";
   }

   if (!wallet_password.empty()) {
      bitcoin_client->walletpassphrase(wallet_password, 5);
   }

   std::string reply_str = bitcoin_client->signrawtransactionwithwallet(sto.transaction);

   std::stringstream ss(reply_str);
   boost::property_tree::ptree json;
   boost::property_tree::read_json(ss, json);
   boost::property_tree::ptree json_res = json.get_child("result");

   if ((json_res.count("hex") == 0) || (json_res.count("complete") == 0)) {
      elog("Failed to process raw transaction ${tx}", ("tx", sto.transaction));
      return "";
   }

   std::string new_tx_raw = json_res.get<std::string>("hex");
   bool complete_raw = json_res.get<bool>("complete");

   if (complete_raw) {
      complete = true;
      return new_tx_raw;
   }
   return new_tx_raw;
}

std::string sidechain_net_handler_bitcoin::sign_transaction_psbt(const sidechain_transaction_object &sto, bool &complete) {
   complete = false;

   if (sto.transaction.empty()) {
      elog("Signing failed, tx string is empty");
      return "";
   }

   if (!wallet_password.empty()) {
      bitcoin_client->walletpassphrase(wallet_password, 5);
   }

   std::string reply_str = bitcoin_client->walletprocesspsbt(sto.transaction);

   std::stringstream ss(reply_str);
   boost::property_tree::ptree json;
   boost::property_tree::read_json(ss, json);
   boost::property_tree::ptree json_res = json.get_child("result");

   if ((json_res.count("psbt") == 0) || (json_res.count("complete") == 0)) {
      elog("Failed to process psbt transaction ${tx}", ("tx", sto.transaction));
      return "";
   }

   std::string new_tx_psbt = json_res.get<std::string>("psbt");
   bool complete_psbt = json_res.get<bool>("complete");

   if (!complete_psbt) {
      // Try to combine and finalize
      vector<std::string> psbts;
      for (auto signature : sto.signatures) {
         if (!signature.second.empty()) {
            psbts.push_back(signature.second);
         }
      }
      psbts.push_back(new_tx_psbt);

      std::string reply_str = bitcoin_client->combinepsbt(psbts);

      std::stringstream ss(reply_str);
      boost::property_tree::ptree json;
      boost::property_tree::read_json(ss, json);
      if (json.count("error") && json.get_child("error").empty()) {

         std::string new_tx_psbt = json.get<std::string>("result");

         std::string reply_str_fin = bitcoin_client->finalizepsbt(new_tx_psbt);
         std::stringstream ss_fin(reply_str_fin);
         boost::property_tree::ptree json_fin;
         boost::property_tree::read_json(ss_fin, json_fin);
         boost::property_tree::ptree json_res = json_fin.get_child("result");

         if (json_res.count("hex") && json_res.count("complete")) {
            complete_psbt = json_res.get<bool>("complete");
         }
      }
   }

   complete = complete_psbt;
   return new_tx_psbt;
}

std::string sidechain_net_handler_bitcoin::sign_transaction_standalone(const sidechain_transaction_object &sto, bool &complete) {
   complete = false;

   complete = true;
   return "";
}

bool sidechain_net_handler_bitcoin::send_transaction_raw(const sidechain_transaction_object &sto, std::string &sidechain_transaction) {
   sidechain_transaction = "";

   return bitcoin_client->sendrawtransaction(sto.transaction);
}

bool sidechain_net_handler_bitcoin::send_transaction_psbt(const sidechain_transaction_object &sto, std::string &sidechain_transaction) {
   sidechain_transaction = "";

   vector<std::string> psbts;
   for (auto signature : sto.signatures) {
      if (!signature.second.empty()) {
         psbts.push_back(signature.second);
      }
   }

   std::string reply_str = bitcoin_client->combinepsbt(psbts);

   std::stringstream ss(reply_str);
   boost::property_tree::ptree json;
   boost::property_tree::read_json(ss, json);

   if (json.count("error") && !json.get_child("error").empty()) {
      elog("Failed to combine psbt transactions from ${sto}", ("sto", sto));
      return false;
   }

   std::string new_tx_psbt = json.get<std::string>("result");

   std::string reply_str_fin = bitcoin_client->finalizepsbt(new_tx_psbt);
   std::stringstream ss_fin(reply_str_fin);
   boost::property_tree::ptree json_fin;
   boost::property_tree::read_json(ss_fin, json_fin);
   boost::property_tree::ptree json_res = json_fin.get_child("result");

   if ((json_res.count("hex") == 0) || (json_res.count("complete") == 0)) {
      elog("Failed to finalize psbt transaction ${tx}", ("tx", new_tx_psbt));
      return false;
   }

   std::string new_tx_raw = json_res.get<std::string>("hex");
   bool complete_raw = json_res.get<bool>("complete");

   if (complete_raw) {
      return bitcoin_client->sendrawtransaction(new_tx_raw);
   }

   return false;
} // namespace peerplays_sidechain

void sidechain_net_handler_bitcoin::handle_event(const std::string &event_data) {
   std::string block = bitcoin_client->getblock(event_data);
   if (block != "") {
      const auto &vins = extract_info_from_block(block);

      const auto &sidechain_addresses_idx = database.get_index_type<sidechain_address_index>().indices().get<by_sidechain_and_deposit_address>();

      for (const auto &v : vins) {
         const auto &addr_itr = sidechain_addresses_idx.find(std::make_tuple(sidechain, v.address));
         if (addr_itr == sidechain_addresses_idx.end())
            continue;

         std::stringstream ss;
         ss << "bitcoin"
            << "-" << v.out.hash_tx << "-" << v.out.n_vout;
         std::string sidechain_uid = ss.str();

         sidechain_event_data sed;
         sed.timestamp = database.head_block_time();
         sed.sidechain = addr_itr->sidechain;
         sed.sidechain_uid = sidechain_uid;
         sed.sidechain_transaction_id = v.out.hash_tx;
         sed.sidechain_from = "";
         sed.sidechain_to = v.address;
         sed.sidechain_currency = "BTC";
         sed.sidechain_amount = v.out.amount;
         sed.peerplays_from = database.get_global_properties().parameters.son_account();
         sed.peerplays_to = addr_itr->sidechain_address_account;
         asset_id_type btc_asset_id = database.get_global_properties().parameters.btc_asset();
         asset_object btc_asset = btc_asset_id(database);
         sed.peerplays_asset = btc_asset.amount(sed.sidechain_amount);
         sidechain_event_data_received(sed);
      }
   }
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

            if (!wallet_password.empty()) {
               bitcoin_client->walletpassphrase(wallet_password, 5);
            }

            std::string pw_address = "";
            if (pw_pt.count("address")) {
               pw_address = pw_pt.get<std::string>("address");
               bitcoin_client->importaddress(pw_address);
            }

            std::string pw_redeem_script = "";
            if (pw_pt.count("redeemScript")) {
               pw_redeem_script = pw_pt.get<std::string>("redeemScript");
               bitcoin_client->importaddress(pw_redeem_script);
            }

            vector<string> son_pubkeys_bitcoin;
            for (const son_info &si : swo->sons) {
               son_pubkeys_bitcoin.push_back(si.sidechain_public_keys.at(sidechain_type::bitcoin));
            }
            uint32_t nrequired = son_pubkeys_bitcoin.size() * 2 / 3 + 1;
            bitcoin_client->addmultisigaddress(nrequired, son_pubkeys_bitcoin);
         }
      }
   }
}

// =============================================================================

}} // namespace graphene::peerplays_sidechain
