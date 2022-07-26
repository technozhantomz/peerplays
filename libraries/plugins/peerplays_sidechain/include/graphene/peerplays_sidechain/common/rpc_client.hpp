#pragma once

#include <cstdint>
#include <string>

#include <boost/asio/ip/tcp.hpp>
#include <boost/beast/core.hpp>

namespace graphene { namespace peerplays_sidechain {

struct rpc_reply {
   uint16_t status;
   std::string body;
};

class rpc_client {
public:
   rpc_client(std::string _url, std::string _user, std::string _password, bool _debug_rpc_calls);

protected:
   std::string retrieve_array_value_from_reply(std::string reply_str, std::string array_path, uint32_t idx);
   std::string retrieve_value_from_reply(std::string reply_str, std::string value_path);
   std::string send_post_request(std::string method, std::string params, bool show_log);

   std::string url;
   std::string protocol;
   std::string host;
   std::string port;
   std::string target;
   std::string authorization;

   std::string user;
   std::string password;
   bool debug_rpc_calls;
   uint32_t request_id;

private:
   rpc_reply send_post_request(std::string body, bool show_log);

   boost::beast::net::io_context ioc;
   boost::beast::net::ip::tcp::resolver resolver;
   boost::asio::ip::basic_resolver_results<boost::asio::ip::tcp> results;
};

}} // namespace graphene::peerplays_sidechain
