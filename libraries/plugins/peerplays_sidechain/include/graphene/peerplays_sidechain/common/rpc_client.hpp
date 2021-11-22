#pragma once

#include <cstdint>
#include <string>

#include <fc/network/http/connection.hpp>

namespace graphene { namespace peerplays_sidechain {

struct rpc_reply {
   uint16_t status;
   std::string body;
};

class rpc_client {
public:
   rpc_client(std::string _ip, uint32_t _port, std::string _user, std::string _password, bool _debug_rpc_calls);

protected:
   std::string retrieve_array_value_from_reply(std::string reply_str, std::string array_path, uint32_t idx);
   std::string retrieve_value_from_reply(std::string reply_str, std::string value_path);
   std::string send_post_request(std::string method, std::string params, bool show_log);

   std::string ip;
   uint32_t port;
   std::string user;
   std::string password;
   bool debug_rpc_calls;

   uint32_t request_id;

   fc::http::header authorization;

private:
   //fc::http::reply send_post_request(std::string body, bool show_log);
   rpc_reply send_post_request(std::string body, bool show_log);
};

}} // namespace graphene::peerplays_sidechain
