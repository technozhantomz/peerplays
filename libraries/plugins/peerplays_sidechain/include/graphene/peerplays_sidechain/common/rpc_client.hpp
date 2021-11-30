#pragma once

#include <cstdint>
#include <string>

#include <boost/asio.hpp>
#include <boost/asio/ssl.hpp>

//#include <fc/network/http/connection.hpp>

namespace graphene { namespace peerplays_sidechain {

enum class url_schema_type { unknown,
                             http,
                             https,
};

// utl

url_schema_type identify_url_schema_type(const std::string &schema_name);

struct url_data {

   url_schema_type schema_type;
   std::string schema;
   std::string host;
   uint16_t port;
   std::string path;

   url_data() :
         schema_type(url_schema_type::unknown),
         port(0) {
   }

   url_data(const std::string &url);

   void clear();

   bool parse(const std::string &url);
};

struct http_request {

   std::string body;
   std::string content_type;

   http_request(const std::string &body_, const std::string &content_type_) :
         body(body_),
         content_type(content_type_) {
   }
};

struct http_response {

   uint16_t status_code;
   std::string body;

   void clear() {
      status_code = 0;
      body = decltype(body)();
   }
};

namespace detail {
template <class>
class http_call_impl;
class tcp_socket;
class ssl_socket;
} // namespace detail

class http_call {
public:
   http_call(const url_data &url, const std::string &method = std::string(), const std::string &headers = std::string());
   ~http_call();

   bool is_ssl() const;

   const std::string &path() const;
   void set_path(const std::string &path);
   void set_method(const std::string &method);
   void set_headers(const std::string &headers);
   const std::string &host() const;
   void set_host(const std::string &host);

   uint16_t port() const;
   void set_port(uint16_t port);

   bool exec(const http_request &request, http_response *response);

   const std::string &error_what() const;

private:
   template <class>
   friend class detail::http_call_impl;
   friend detail::tcp_socket;
   friend detail::ssl_socket;
   static constexpr auto response_size_limit_bytes = 16 * 1024 * 1024;
   static constexpr auto response_first_alloc_bytes = 32 * 1024;
   static constexpr auto response_next_alloc_bytes = 256 * 1024;
   std::string m_host;
   uint16_t m_port_default;
   uint16_t m_port;
   std::string m_path;
   std::string m_method;
   std::string m_headers;
   std::string m_error_what;

   boost::asio::io_service m_service;
   boost::asio::ssl::context *m_context;
   boost::asio::ip::tcp::endpoint m_endpoint;

   void ctor_priv();
};

}} // namespace graphene::peerplays_sidechain

namespace graphene { namespace peerplays_sidechain {

class rpc_client {
public:
   rpc_client(const std::string &url, const std::string &user_name, const std::string &password, bool debug);

protected:
   std::string retrieve_array_value_from_reply(std::string reply_str, std::string array_path, uint32_t idx);
   std::string retrieve_value_from_reply(std::string reply_str, std::string value_path);
   std::string send_post_request(std::string method, std::string params, bool show_log);

   bool debug_rpc_calls;
   uint32_t request_id;

private:
   http_call client;
   http_response send_post_request(const std::string &body, bool show_log);
};

}} // namespace graphene::peerplays_sidechain
