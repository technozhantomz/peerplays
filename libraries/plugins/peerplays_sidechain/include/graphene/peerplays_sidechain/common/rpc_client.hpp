#pragma once

#include <cstdint>
#include <string>

#include <fc/thread/future.hpp>
#include <fc/thread/thread.hpp>

#include <boost/asio/ip/tcp.hpp>
#include <boost/beast/core.hpp>

#include <graphene/peerplays_sidechain/defs.hpp>

namespace graphene { namespace peerplays_sidechain {

class rpc_connection;

struct rpc_credentials {
   std::string url;
   std::string user;
   std::string password;
};

class rpc_client {
public:
   const sidechain_type sidechain;

   rpc_client(sidechain_type _sidechain, const std::vector<rpc_credentials> &_credentials, bool _debug_rpc_calls, bool _simulate_connection_reselection);
   ~rpc_client();

protected:
   bool debug_rpc_calls;
   bool simulate_connection_reselection;
   std::string send_post_request(std::string method, std::string params, bool show_log);

   static std::string send_post_request(rpc_connection &conn, std::string method, std::string params, bool show_log);

   static std::string retrieve_array_value_from_reply(std::string reply_str, std::string array_path, uint32_t idx);
   static std::string retrieve_value_from_reply(std::string reply_str, std::string value_path);

private:
   std::vector<rpc_connection *> connections;
   int n_active_conn;
   fc::future<void> connection_selection_task;
   std::mutex conn_mutex;

   rpc_connection &get_active_connection() const;

   void select_connection();
   void schedule_connection_selection();
   virtual uint64_t ping(rpc_connection &conn) const = 0;
};

}} // namespace graphene::peerplays_sidechain
