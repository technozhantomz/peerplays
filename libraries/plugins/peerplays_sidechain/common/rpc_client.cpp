#include <graphene/peerplays_sidechain/common/rpc_client.hpp>

#include <regex>
#include <sstream>

#include <boost/asio/buffers_iterator.hpp>
#include <boost/asio/connect.hpp>
#include <boost/asio/ssl/error.hpp>
#include <boost/asio/ssl/stream.hpp>
#include <boost/beast/http.hpp>
#include <boost/property_tree/json_parser.hpp>
#include <boost/property_tree/ptree.hpp>
#include <boost/xpressive/xpressive.hpp>

#include <fc/log/logger.hpp>

#include <graphene/peerplays_sidechain/common/utils.hpp>

namespace graphene { namespace peerplays_sidechain {

rpc_client::rpc_client(std::string _url, std::string _user, std::string _password, bool _debug_rpc_calls) :
      url(_url),
      user(_user),
      password(_password),
      debug_rpc_calls(_debug_rpc_calls),
      request_id(0),
      resolver(ioc) {

   std::string reg_expr = "^((?P<Protocol>https|http):\\/\\/)?(?P<Host>[a-zA-Z0-9\\-\\.]+)(:(?P<Port>\\d{1,5}))?(?P<Target>\\/.+)?";
   boost::xpressive::sregex sr = boost::xpressive::sregex::compile(reg_expr);

   boost::xpressive::smatch sm;

   if (boost::xpressive::regex_search(url, sm, sr)) {
      protocol = sm["Protocol"];
      if (protocol.empty()) {
         protocol = "http";
      }

      host = sm["Host"];
      if (host.empty()) {
         host + "localhost";
      }

      port = sm["Port"];
      if (port.empty()) {
         port = "80";
      }

      target = sm["Target"];
      if (target.empty()) {
         target = "/";
      }

      authorization = "Basic " + base64_encode(user + ":" + password);

      results = resolver.resolve(host, port);

   } else {
      elog("Invalid URL: ${url}", ("url", url));
   }
}

std::string rpc_client::retrieve_array_value_from_reply(std::string reply_str, std::string array_path, uint32_t idx) {
   std::stringstream ss(reply_str);
   boost::property_tree::ptree json;
   boost::property_tree::read_json(ss, json);
   if (json.find("result") == json.not_found()) {
      return "";
   }

   auto json_result = json.get_child_optional("result");
   if (json_result) {
      boost::property_tree::ptree array_ptree = json_result.get();
      if (!array_path.empty()) {
         array_ptree = json_result.get().get_child(array_path);
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

   auto json_result = json.get_child_optional("result");
   if (json_result) {
      return json_result.get().get<std::string>(value_path);
   }

   return json.get<std::string>("result");
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

   if (json.count("error") && !json.get_child("error").empty()) {
      wlog("RPC call ${function} with body ${body} failed with reply '${msg}'", ("function", __FUNCTION__)("body", body.str())("msg", ss.str()));
   }

   if (reply.status == 200) {
      return ss.str();
   }

   return "";
}

rpc_reply rpc_client::send_post_request(std::string body, bool show_log) {

   // These object is used as a context for ssl connection
   boost::asio::ssl::context ctx(boost::asio::ssl::context::tlsv12_client);

   boost::beast::net::ssl::stream<boost::beast::tcp_stream> ssl_tcp_stream(ioc, ctx);
   boost::beast::tcp_stream tcp_stream(ioc);

   // Set SNI Hostname (many hosts need this to handshake successfully)
   if (protocol == "https") {
      if (!SSL_set_tlsext_host_name(ssl_tcp_stream.native_handle(), host.c_str())) {
         boost::beast::error_code ec{static_cast<int>(::ERR_get_error()), boost::asio::error::get_ssl_category()};
         throw boost::beast::system_error{ec};
      }
      ctx.set_default_verify_paths();
      ctx.set_verify_mode(boost::asio::ssl::verify_peer);
   }

   // Make the connection on the IP address we get from a lookup
   if (protocol == "https") {
      boost::beast::get_lowest_layer(ssl_tcp_stream).connect(results);
      ssl_tcp_stream.handshake(boost::beast::net::ssl::stream_base::client);
   } else {
      tcp_stream.connect(results);
   }

   // Set up an HTTP GET request message
   boost::beast::http::request<boost::beast::http::string_body> req{boost::beast::http::verb::post, target, 11};
   req.set(boost::beast::http::field::host, host + ":" + port);
   req.set(boost::beast::http::field::accept, "application/json");
   req.set(boost::beast::http::field::authorization, authorization);
   req.set(boost::beast::http::field::content_type, "application/json");
   req.set(boost::beast::http::field::content_encoding, "utf-8");
   req.set(boost::beast::http::field::content_length, body.length());
   req.body() = body;

   // Send the HTTP request to the remote host
   if (protocol == "https")
      boost::beast::http::write(ssl_tcp_stream, req);
   else
      boost::beast::http::write(tcp_stream, req);

   // This buffer is used for reading and must be persisted
   boost::beast::flat_buffer buffer;

   // Declare a container to hold the response
   boost::beast::http::response<boost::beast::http::dynamic_body> res;

   // Receive the HTTP response
   if (protocol == "https")
      boost::beast::http::read(ssl_tcp_stream, buffer, res);
   else
      boost::beast::http::read(tcp_stream, buffer, res);

   // Gracefully close the socket
   boost::beast::error_code ec;
   if (protocol == "https") {
      boost::beast::get_lowest_layer(ssl_tcp_stream).close();
   } else {
      tcp_stream.socket().shutdown(boost::asio::ip::tcp::socket::shutdown_both, ec);
   }

   // not_connected happens sometimes. Also on ssl level some servers are managing
   // connecntion close, so closing here will sometimes end up with error stream truncated
   // so don't bother reporting it.
   if (ec && ec != boost::beast::errc::not_connected && ec != boost::asio::ssl::error::stream_truncated)
      throw boost::beast::system_error{ec};

   std::string rbody{boost::asio::buffers_begin(res.body().data()),
                     boost::asio::buffers_end(res.body().data())};
   rpc_reply reply;
   reply.status = 200;
   reply.body = rbody;

   if (show_log) {
      ilog("### Request URL:    ${url}", ("url", url));
      ilog("### Request:        ${body}", ("body", body));
      ilog("### Response:       ${rbody}", ("rbody", rbody));
   }

   return reply;
}

}} // namespace graphene::peerplays_sidechain
