#include <graphene/peerplays_sidechain/common/rpc_client.hpp>

#include <sstream>
#include <string>

#include <boost/property_tree/json_parser.hpp>
#include <boost/property_tree/ptree.hpp>

#include <curl/curl.h>

#include <fc/crypto/base64.hpp>
#include <fc/log/logger.hpp>

namespace graphene { namespace peerplays_sidechain {

rpc_client::rpc_client(std::string _ip, uint32_t _port, std::string _user, std::string _password, bool _debug_rpc_calls) :
      ip(_ip),
      port(_port),
      user(_user),
      password(_password),
      debug_rpc_calls(_debug_rpc_calls),
      request_id(0) {
   authorization.key = "Authorization";
   authorization.val = "Basic " + fc::base64_encode(user + ":" + password);
}

std::string rpc_client::retrieve_array_value_from_reply(std::string reply_str, std::string array_path, uint32_t idx) {
   std::stringstream ss(reply_str);
   boost::property_tree::ptree json;
   boost::property_tree::read_json(ss, json);
   if (json.find("result") == json.not_found()) {
      return "";
   }
   auto json_result = json.get_child("result");
   if (json_result.find(array_path) == json_result.not_found()) {
      return "";
   }

   boost::property_tree::ptree array_ptree = json_result;
   if (!array_path.empty()) {
      array_ptree = json_result.get_child(array_path);
   }
   uint32_t array_el_idx = -1;
   for (const auto &array_el : array_ptree) {
      array_el_idx = array_el_idx + 1;
      if (array_el_idx == idx) {
         std::stringstream ss_res;
         boost::property_tree::json_parser::write_json(ss_res, array_el.second);
         return ss_res.str();
      }
   }

   return "";
}

std::string rpc_client::retrieve_value_from_reply(std::string reply_str, std::string value_path) {
   std::stringstream ss(reply_str);
   boost::property_tree::ptree json;
   boost::property_tree::read_json(ss, json);
   if (json.find("result") == json.not_found()) {
      return "";
   }
   auto json_result = json.get_child("result");
   if (json_result.find(value_path) == json_result.not_found()) {
      return "";
   }
   return json_result.get<std::string>(value_path);
}

std::string rpc_client::send_post_request(std::string method, std::string params, bool show_log) {
   std::stringstream body;

   request_id = request_id + 1;

   body << "{ \"jsonrpc\": \"2.0\", \"id\": " << request_id << ", \"method\": \"" << method << "\"";

   if (!params.empty()) {
      body << ", \"params\": " << params;
   }

   body << " }";

   const auto reply = send_post_request(body.str(), show_log);

   if (reply.body.empty()) {
      wlog("RPC call ${function} failed", ("function", __FUNCTION__));
      return "";
   }

   std::stringstream ss(std::string(reply.body.begin(), reply.body.end()));
   boost::property_tree::ptree json;
   boost::property_tree::read_json(ss, json);

   if (reply.status == 200) {
      return ss.str();
   }

   if (json.count("error") && !json.get_child("error").empty()) {
      wlog("RPC call ${function} with body ${body} failed with reply '${msg}'", ("function", __FUNCTION__)("body", body.str())("msg", ss.str()));
   }
   return "";
}

//fc::http::reply rpc_client::send_post_request(std::string body, bool show_log) {
//   fc::http::connection conn;
//   conn.connect_to(fc::ip::endpoint(fc::ip::address(ip), port));
//
//   std::string url = "http://" + ip + ":" + std::to_string(port);
//
//   //if (wallet.length() > 0) {
//   //   url = url + "/wallet/" + wallet;
//   //}
//
//   fc::http::reply reply = conn.request("POST", url, body, fc::http::headers{authorization});
//
//   if (show_log) {
//      ilog("### Request URL:    ${url}", ("url", url));
//      ilog("### Request:        ${body}", ("body", body));
//      std::stringstream ss(std::string(reply.body.begin(), reply.body.end()));
//      ilog("### Response:       ${ss}", ("ss", ss.str()));
//   }
//
//   return reply;
//}

static size_t write_callback(char *ptr, size_t size, size_t nmemb, rpc_reply *reply) {
   size_t retval = 0;
   if (reply != nullptr) {
      reply->body.append(ptr, size * nmemb);
      retval = size * nmemb;
   }
   return retval;
}

rpc_reply rpc_client::send_post_request(std::string body, bool show_log) {

   struct curl_slist *headers = nullptr;
   headers = curl_slist_append(headers, "Accept: application/json");
   headers = curl_slist_append(headers, "Content-Type: application/json");
   headers = curl_slist_append(headers, "charset: utf-8");

   CURL *curl = curl_easy_init();
   if (ip.find("https://", 0) != 0) {
      curl_easy_setopt(curl, CURLOPT_URL, ip.c_str());
      curl_easy_setopt(curl, CURLOPT_PORT, port);
   } else {
      std::string full_address = ip + ":" + std::to_string(port);
      curl_easy_setopt(curl, CURLOPT_URL, full_address.c_str());
      curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, false);
      curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, false);
   }
   if (!user.empty()) {
      curl_easy_setopt(curl, CURLOPT_USERNAME, user.c_str());
      curl_easy_setopt(curl, CURLOPT_PASSWORD, password.c_str());
   }

   curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
   curl_easy_setopt(curl, CURLOPT_POSTFIELDS, body.c_str());

   //curl_easy_setopt(curl, CURLOPT_VERBOSE, true);

   rpc_reply reply;

   curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_callback);
   curl_easy_setopt(curl, CURLOPT_WRITEDATA, &reply);

   curl_easy_perform(curl);

   curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &reply.status);

   curl_easy_cleanup(curl);
   curl_slist_free_all(headers);

   if (show_log) {
      std::string url = ip + ":" + std::to_string(port);
      ilog("### Request URL:    ${url}", ("url", url));
      ilog("### Request:        ${body}", ("body", body));
      std::stringstream ss(std::string(reply.body.begin(), reply.body.end()));
      ilog("### Response:       ${ss}", ("ss", ss.str()));
   }

   return reply;
}

}} // namespace graphene::peerplays_sidechain
