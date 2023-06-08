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

struct rpc_reply {
   uint16_t status;
   std::string body;
};

class rpc_connection {
public:
   rpc_connection(const rpc_credentials &_credentials, bool _debug_rpc_calls);

   std::string send_post_request(std::string method, std::string params, bool show_log);
   std::string get_url() const;

protected:
   rpc_credentials credentials;
   bool debug_rpc_calls;

   std::string protocol;
   std::string host;
   std::string port;
   std::string target;
   std::string authorization;

   uint32_t request_id;

private:
   rpc_reply send_post_request(std::string body, bool show_log);

   boost::beast::net::io_context ioc;
   boost::beast::net::ip::tcp::resolver resolver;
   boost::asio::ip::basic_resolver_results<boost::asio::ip::tcp> results;
};

rpc_connection::rpc_connection(const rpc_credentials &_credentials, bool _debug_rpc_calls) :
      credentials(_credentials),
      debug_rpc_calls(_debug_rpc_calls),
      request_id(0),
      resolver(ioc) {

   std::string reg_expr = "^((?P<Protocol>https|http):\\/\\/)?(?P<Host>[a-zA-Z0-9\\-\\.]+)(:(?P<Port>\\d{1,5}))?(?P<Target>\\/.+)?";
   boost::xpressive::sregex sr = boost::xpressive::sregex::compile(reg_expr);

   boost::xpressive::smatch sm;

   if (boost::xpressive::regex_search(credentials.url, sm, sr)) {
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

      authorization = "Basic " + base64_encode(credentials.user + ":" + credentials.password);

      results = resolver.resolve(host, port);

   } else {
      elog("Invalid URL: ${url}", ("url", credentials.url));
   }
}

std::string rpc_connection::get_url() const {
   return credentials.url;
}

std::string rpc_client::retrieve_array_value_from_reply(std::string reply_str, std::string array_path, uint32_t idx) {
   if (reply_str.empty()) {
      wlog("RPC call ${function}, empty reply string", ("function", __FUNCTION__));
      return "";
   }

   try {
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
   } catch (const boost::property_tree::json_parser::json_parser_error &e) {
      wlog("RPC call ${function} failed: ${e}", ("function", __FUNCTION__)("e", e.what()));
   }

   return "";
}

std::string rpc_client::retrieve_value_from_reply(std::string reply_str, std::string value_path) {
   if (reply_str.empty()) {
      wlog("RPC call ${function}, empty reply string", ("function", __FUNCTION__));
      return "";
   }

   try {
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
   } catch (const boost::property_tree::json_parser::json_parser_error &e) {
      wlog("RPC call ${function} failed: ${e}", ("function", __FUNCTION__)("e", e.what()));
   }

   return "";
}

std::string rpc_connection::send_post_request(std::string method, std::string params, bool show_log) {
   std::stringstream body;

   request_id = request_id + 1;

   body << "{ \"jsonrpc\": \"2.0\", \"id\": " << request_id << ", \"method\": \"" << method << "\"";

   if (!params.empty()) {
      body << ", \"params\": " << params;
   }

   body << " }";

   try {
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
   } catch (const boost::system::system_error &e) {
      elog("RPC call ${function} failed: ${e}", ("function", __FUNCTION__)("e", e.what()));
   }

   return "";
}

rpc_reply rpc_connection::send_post_request(std::string body, bool show_log) {

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
      ilog("### Request URL:    ${url}", ("url", credentials.url));
      ilog("### Request:        ${body}", ("body", body));
      ilog("### Response:       ${rbody}", ("rbody", rbody));
   }

   return reply;
}

rpc_client::rpc_client(sidechain_type _sidechain, const std::vector<rpc_credentials> &_credentials, bool _debug_rpc_calls, bool _simulate_connection_reselection) :
      sidechain(_sidechain),
      debug_rpc_calls(_debug_rpc_calls),
      simulate_connection_reselection(_simulate_connection_reselection) {
   FC_ASSERT(_credentials.size());
   for (size_t i = 0; i < _credentials.size(); i++)
      connections.push_back(new rpc_connection(_credentials[i], _debug_rpc_calls));
   n_active_conn = 0;
   if (connections.size() > 1)
      schedule_connection_selection();
}

void rpc_client::schedule_connection_selection() {
   fc::time_point now = fc::time_point::now();
   static const int64_t time_to_next_conn_selection = 10 * 1000 * 1000; // 10 sec
   fc::time_point next_wakeup = now + fc::microseconds(time_to_next_conn_selection);
   connection_selection_task = fc::schedule([this] {
      select_connection();
   },
                                            next_wakeup, "SON RPC connection selection");
}

void rpc_client::select_connection() {
   FC_ASSERT(connections.size() > 1);

   const std::lock_guard<std::mutex> lock(conn_mutex);

   static const int t_limit = 5 * 1000 * 1000, // 5 sec
         quality_diff_threshold = 10 * 1000;   // 10 ms

   int best_n = -1;
   int best_quality = -1;

   std::vector<uint64_t> head_block_numbers;
   head_block_numbers.resize(connections.size());

   std::vector<int> qualities;
   qualities.resize(connections.size());

   for (size_t n = 0; n < connections.size(); n++) {
      rpc_connection &conn = *connections[n];
      int quality = 0;
      head_block_numbers[n] = std::numeric_limits<uint64_t>::max();

      // ping n'th node
      if (debug_rpc_calls)
         ilog("### Ping ${sidechain} node #${n}, ${url}", ("sidechain", fc::reflector<sidechain_type>::to_string(sidechain))("n", n)("url", conn.get_url()));
      fc::time_point t_sent = fc::time_point::now();
      uint64_t head_block_number = ping(conn);
      fc::time_point t_received = fc::time_point::now();
      int t = (t_received - t_sent).count();

      // evaluate n'th node reply quality and switch to it if it's better
      if (head_block_number != std::numeric_limits<uint64_t>::max()) {
         if (simulate_connection_reselection)
            t += rand() % 10;
         FC_ASSERT(t != -1);
         head_block_numbers[n] = head_block_number;
         if (t < t_limit)
            quality = t_limit - t; // the less time, the higher quality

         // look for the best quality
         if (quality > best_quality) {
            best_n = n;
            best_quality = quality;
         }
      }
      qualities[n] = quality;
   }

   FC_ASSERT(best_n != -1 && best_quality != -1);
   if (best_n != n_active_conn) { // if the best client is not the current one, ...
      uint64_t active_head_block_number = head_block_numbers[n_active_conn];
      if ((active_head_block_number == std::numeric_limits<uint64_t>::max()      // ...and the current one has no known head block...
           || head_block_numbers[best_n] >= active_head_block_number)            // ...or the best client's head is more recent than the current, ...
          && best_quality > qualities[n_active_conn] + quality_diff_threshold) { // ...and the new client's quality exceeds current more than by threshold
         n_active_conn = best_n;                                                 // ...then select new one
         if (debug_rpc_calls)
            ilog("### Reselected ${sidechain} node to #${n}, ${url}", ("sidechain", fc::reflector<sidechain_type>::to_string(sidechain))("n", n_active_conn)("url", connections[n_active_conn]->get_url()));
      }
   }

   schedule_connection_selection();
}

rpc_connection &rpc_client::get_active_connection() const {
   return *connections[n_active_conn];
}

std::string rpc_client::send_post_request(std::string method, std::string params, bool show_log) {
   const std::lock_guard<std::mutex> lock(conn_mutex);
   return send_post_request(get_active_connection(), method, params, show_log);
}

std::string rpc_client::send_post_request(rpc_connection &conn, std::string method, std::string params, bool show_log) {
   return conn.send_post_request(method, params, show_log);
}

rpc_client::~rpc_client() {
   try {
      if (connection_selection_task.valid())
         connection_selection_task.cancel_and_wait(__FUNCTION__);
   } catch (fc::canceled_exception &) {
      //Expected exception. Move along.
   } catch (fc::exception &e) {
      edump((e.to_detail_string()));
   }
}

}} // namespace graphene::peerplays_sidechain
