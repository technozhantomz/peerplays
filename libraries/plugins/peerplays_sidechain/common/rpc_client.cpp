#include <graphene/peerplays_sidechain/common/rpc_client.hpp>

#include <sstream>
#include <string>

//#include <curl/curl.h>

#include <boost/algorithm/string/case_conv.hpp>
#include <boost/algorithm/string/trim.hpp>
#include <boost/asio/buffer.hpp>
#include <boost/property_tree/json_parser.hpp>
#include <boost/property_tree/ptree.hpp>

#include <fc/crypto/base64.hpp>
#include <fc/log/logger.hpp>
#include <fc/network/ip.hpp>

namespace graphene { namespace peerplays_sidechain {

constexpr auto http_port = 80;
constexpr auto https_port = 443;

template <class string>
void make_trimmed(string *str) {
   boost::algorithm::trim(*str);
}

template <class string>
void make_lower(string *str) {
   boost::algorithm::to_lower(*str);
}

bool convert_hex_to_num_helper1(const std::string &str, uint32_t *value) {
   try {
      size_t idx;
      auto v = stol(str, &idx, 16);
      if (idx != str.size())
         return false;
      if (value)
         *value = v;
      return true;
   } catch (...) {
      return false;
   }
}

bool convert_dec_to_num_helper1(const std::string &str, uint32_t *value) {
   try {
      size_t idx;
      auto v = stol(str, &idx, 10);
      if (idx != str.size())
         return false;
      if (value)
         *value = v;
      return true;
   } catch (...) {
      return false;
   }
}

bool convert_dec_to_num_helper1(const std::string &str, uint16_t *value) {
   try {
      size_t idx;
      auto v = stol(str, &idx, 10);
      if (idx != str.size())
         return false;
      if (v > std::numeric_limits<uint16_t>::max())
         return false;
      if (value)
         *value = v;
      return true;
   } catch (...) {
      return false;
   }
}

template <typename V, typename D>
constexpr V ceilDiv(V value, D divider) {
   return (value + divider - 1) / divider;
}

template <typename V, typename A>
constexpr V aligned(V value, A align) {
   return ceilDiv(value, align) * align;
}

template <typename Container>
void reserve(
      Container *container,
      typename Container::size_type freeSpaceRequired,
      typename Container::size_type firstAlloc,
      typename Container::size_type nextAlloc) {
   //TSL_ASSERT(container);
   auto &c = *container;
   auto required = c.size() + freeSpaceRequired;
   if (c.capacity() >= required)
      return;
   c.reserve((firstAlloc >= required) ? firstAlloc
                                      : firstAlloc + aligned(required - firstAlloc, nextAlloc));
}

template <typename Container>
void reserve(
      Container *container,
      typename Container::size_type freeSpaceRequired,
      typename Container::size_type alloc) {
   //TSL_ASSERT(container);
   auto &c = *container;
   auto required = c.size() + freeSpaceRequired;
   if (c.capacity() >= required)
      return;
   c.reserve(aligned(required, alloc));
}

bool is_valid(const boost::asio::ip::tcp::endpoint &ep) {

   if (ep.port() == 0)
      return false;

   if (ep.address().is_unspecified())
      return false;

   return true;
}

// utl

url_schema_type identify_url_schema_type(const std::string &schema_name) {
   // rework
   auto temp = schema_name;
   make_lower(&temp);
   if (temp == "http")
      return url_schema_type::http;
   if (temp == "https")
      return url_schema_type::https;
   return url_schema_type::unknown;
}

// url_data

url_data::url_data(const std::string &url) {
   if (!parse(url))
      FC_THROW("URL parse failed");
}

void url_data::clear() {
   schema_type = url_schema_type::unknown;
   schema = decltype(schema)();
   host = decltype(host)();
   port = 0;
   path = decltype(path)();
}

bool url_data::parse(const std::string &url) {

   typedef std::string::size_type size_t;
   constexpr auto npos = std::string::npos;

   size_t schema_end = url.find("://");
   size_t host_begin;
   std::string temp_schema;

   if (schema_end == npos)
      host_begin = 0; // no schema
   else {
      if (schema_end < 3) { // schema too short: less than 3 chars
         return false;
      }
      if (schema_end > 5) { // schema too long: more than 5 chars
         return false;
      }
      host_begin = schema_end + 3;
      temp_schema = url.substr(0, schema_end);
   }

   //	ASSERT(url.size() >= host_begin);

   if (url.size() == host_begin) // host is empty
      return false;

   size_t port_sep = url.find(':', host_begin);

   if (port_sep == host_begin)
      return false;

   size_t path_sep = url.find('/', host_begin);

   if (path_sep == host_begin)
      return false;

   if ((port_sep != npos) && (path_sep != npos) && (port_sep > path_sep))
      port_sep = npos;

   std::string temp_port;

   if (port_sep != npos) {
      auto port_index = port_sep + 1;
      if (path_sep == npos)
         temp_port = url.substr(port_index);
      else
         temp_port = url.substr(port_index, path_sep - port_index);
   }

   if (temp_port.empty())
      port = 0;
   else {
      if (!convert_dec_to_num_helper1(temp_port, &port))
         return false;
   }

   std::string temp_path;

   if (path_sep != npos)
      temp_path = url.substr(path_sep);

   std::string temp_host;

   if (port_sep != npos) {
      temp_host = url.substr(host_begin, port_sep - host_begin);
   } else {
      if (path_sep != npos)
         temp_host = url.substr(host_begin, path_sep - host_begin);
      else
         temp_host = url.substr(host_begin);
   }

   schema = temp_schema;
   host = temp_host;
   path = temp_path;
   schema_type = identify_url_schema_type(schema);

   return true;
}

}} // namespace graphene::peerplays_sidechain

namespace graphene { namespace peerplays_sidechain {

using namespace boost::asio;
using error_code = boost::system::error_code;
using endpoint = ip::tcp::endpoint;

namespace detail {

// http_call_impl

struct tcp_socket {

   typedef ip::tcp::socket underlying_type;

   underlying_type underlying;

   tcp_socket(http_call &call) :
         underlying(call.m_service) {
   }

   underlying_type &operator()() {
      return underlying;
   }

   void connect(const http_call &call, const endpoint &ep, error_code *ec) {
      // TCP connect
      underlying.connect(ep, *ec);
   }

   void shutdown() {
      error_code ec;
      underlying.close(ec);
   }
};

struct ssl_socket {

   typedef ssl::stream<ip::tcp::socket> underlying_type;

   underlying_type underlying;

   ssl_socket(http_call &call) :
         underlying(call.m_service, *call.m_context) {
   }

   underlying_type &operator()() {
      return underlying;
   }

   void connect(const http_call &call, const endpoint &ep, error_code *ec) {

      auto &u = underlying;

      // TCP connect
      u.lowest_layer().connect(ep, *ec);

      // SSL connect
      if (!SSL_set_tlsext_host_name(u.native_handle(), call.m_host.c_str()))
         FC_THROW("SSL_set_tlsext_host_name failed");

      u.set_verify_mode(ssl::verify_peer, *ec);
      u.handshake(ssl::stream_base::client, *ec);
   }

   void shutdown() {
      auto &u = underlying;
      error_code ec;
      u.shutdown(ec);
      u.lowest_layer().close(ec);
   }
};

template <class socket_type>
class http_call_impl {
public:
   http_call_impl(http_call &call, const void *body_data, size_t body_size, const std::string &content_type_, http_response &response);
   void exec();

private:
   http_call &call;
   const void *body_data;
   size_t body_size;
   std::string content_type;
   http_response &response;

   socket_type socket;
   streambuf response_buf;

   int32_t content_length;
   bool transfer_encoding_chunked;

private:
   void connect();
   void shutdown();
   void send_request();
   void on_header(std::string &name, std::string &value);
   void on_header();
   void process_headers();
   void append_entity_body(std::istream *stream, size_t size);
   void append_entity_body_2(std::istream *strm);
   bool read_next_chunk(std::istream *strm);
   void skip_footer();
   void read_body_chunked();
   void read_body_until_eof();
   void read_body_exact();
   void process_response();
};

static const char cr = 0x0D;
static const char lf = 0x0A;
static const char *crlf = "\x0D\x0A";
static const char *crlfcrlf = "\x0D\x0A\x0D\x0A";
static const auto crlf_uint = (((uint16_t)lf) << 8) + cr;

template <class s>
http_call_impl<s>::http_call_impl(http_call &call_, const void *body_data_, size_t body_size_, const std::string &content_type_, http_response &response_) :
      call(call_),
      body_data(body_data_),
      body_size(body_size_),
      content_type(content_type_),
      response(response_),
      socket(call),
      response_buf(http_call::response_size_limit_bytes) {
}

template <class s>
void http_call_impl<s>::exec() {
   try {
      connect();
      send_request();
      process_response();
      shutdown();
   } catch (...) {
      shutdown();
      throw;
   }
}

template <class s>
void http_call_impl<s>::connect() {

   {
      error_code ec;
      auto &ep = call.m_endpoint;
      if (is_valid(ep)) {
         socket.connect(call, ep, &ec);
         if (!ec)
            return;
      }
   }

   ip::tcp::resolver resolver(call.m_service);

   auto rng = resolver.resolve(call.m_host, std::string());

   //ASSERT(rng.begin() != rng.end());

   error_code ec;

   for (endpoint ep : rng) {
      ep.port(call.m_port);
      socket.connect(call, ep, &ec);
      if (!ec) {
         call.m_endpoint = ep;
         return; // comment to test1
      }
   }
   //	if (!ec) return;	// uncomment to test1

   //ASSERT(ec);
   throw boost::system::system_error(ec);
}

template <class s>
void http_call_impl<s>::shutdown() {
   socket.shutdown();
}

template <class s>
void http_call_impl<s>::send_request() {

   streambuf request;
   std::ostream stream(&request);

   // start string: <method> <path> HTTP/1.0

   //ASSERT(!call.m_path.empty());

   stream << call.m_method << " " << call.m_path << " HTTP/1.1" << crlf;

   // host

   stream << "Host: " << call.m_host << ":" << call.m_endpoint.port() << crlf;

   // content

   if (body_size) {
      stream << "Content-Type: " << content_type << crlf;
      stream << "Content-Length: " << body_size << crlf;
   }

   // additional headers

   const auto &h = call.m_headers;

   if (!h.empty()) {
      if (h.size() < 2)
         FC_THROW("invalid headers data");
      stream << h;
      // ensure headers finished correctly
      if ((h.substr(h.size() - 2) != crlf))
         stream << crlf;
   }

   // other

   //      stream << "Accept: *\x2F*" << crlf;
   stream << "Accept: text/html, application/json" << crlf;
   stream << "Connection: close" << crlf;

   // end

   stream << crlf;

   // send headers

   write(socket(), request);

   // send body

   if (body_size)
      write(socket(), buffer(body_data, body_size));
}

template <class s>
void http_call_impl<s>::on_header(std::string &name, std::string &value) {

   if (name == "content-length") {
      uint32_t u;
      if (!convert_dec_to_num_helper1(value, &u))
         FC_THROW("invalid content-length header data");
      content_length = u;
      return;
   }

   if (name == "transfer-encoding") {
      boost::algorithm::to_lower(value);
      if (value == "chunked")
         transfer_encoding_chunked = true;
      return;
   }
}

template <class s>
void http_call_impl<s>::process_headers() {

   std::istream stream(&response_buf);

   std::string http_version;
   stream >> http_version;
   stream >> response.status_code;

   make_trimmed(&http_version);
   make_lower(&http_version);

   if (!stream || http_version.substr(0, 6) != "http/1")
      FC_THROW("invalid response data");

   // read/skip headers

   content_length = -1;
   transfer_encoding_chunked = false;

   for (;;) {
      std::string header;
      if (!std::getline(stream, header, lf) || (header.size() == 1 && header[0] == cr))
         break;
      auto pos = header.find(':');
      if (pos == std::string::npos)
         continue;
      auto name = header.substr(0, pos);
      make_trimmed(&name);
      boost::algorithm::to_lower(name);
      auto value = header.substr(pos + 1);
      make_trimmed(&value);
      on_header(name, value);
   }
}

template <class s>
void http_call_impl<s>::append_entity_body(std::istream *strm, size_t size) {
   if (size == 0)
      return;
   auto &body = response.body;
   reserve(&body, size, http_call::response_first_alloc_bytes, http_call::response_next_alloc_bytes);
   auto cur = body.size();
   body.resize(cur + size);
   auto p = &body[cur];
   if (!strm->read(p, size))
      FC_THROW("stream read failed");
}

template <class s>
void http_call_impl<s>::append_entity_body_2(std::istream *strm) {
   auto avail = response_buf.size();
   if (response.body.size() + avail > http_call::response_size_limit_bytes)
      FC_THROW("response body size limit exceeded");
   append_entity_body(strm, avail);
}

template <class s>
bool http_call_impl<s>::read_next_chunk(std::istream *strm) {

   // content length info is used as pre-alloc hint only
   // it is not used inside the reading logic

   auto &buf = response_buf;
   auto &stream = *strm;
   auto &body = response.body;

   read_until(socket(), buf, crlf);

   std::string chunk_header;

   if (!std::getline(stream, chunk_header, lf))
      FC_THROW("failed to read chunk size");

   auto ext_index = chunk_header.find(':');

   if (ext_index != std::string::npos)
      chunk_header.resize(ext_index);

   make_trimmed(&chunk_header);

   uint32_t chink_size;

   if (!convert_hex_to_num_helper1(chunk_header, &chink_size))
      FC_THROW("invalid chunk size string");

   if (body.size() + chink_size > http_call::response_size_limit_bytes)
      FC_THROW("response body size limit exceeded");

   auto avail = buf.size();
   if (avail < chink_size + 2) {
      auto rest = chink_size + 2 - avail;
      read(socket(), buf, transfer_at_least(rest));
   }

   append_entity_body(&stream, chink_size);

   uint16_t temp;
   if (!stream.read((char *)(&temp), 2))
      FC_THROW("stream read failed");
   if (temp != crlf_uint)
      FC_THROW("invalid chink end");

   return chink_size != 0;
}

template <class s>
void http_call_impl<s>::skip_footer() {
   // to be implemeted
}

template <class s>
void http_call_impl<s>::read_body_chunked() {

   std::istream stream(&response_buf);

   for (;;) {
      if (!read_next_chunk(&stream))
         break;
   }

   skip_footer();
}

template <class s>
void http_call_impl<s>::read_body_until_eof() {

   auto &buf = response_buf;
   std::istream stream(&buf);

   append_entity_body_2(&stream);

   error_code ec;

   for (;;) {
      auto readed = read(socket(), buf, transfer_at_least(1), ec);
      append_entity_body_2(&stream);
      if (ec)
         break;
      if (!readed) {
         //ASSERT(buf.size() == 0);
         FC_THROW("logic error: read failed but no error conditon");
      }
   }
   if ((ec != error::eof) &&
       (ec != ssl::error::stream_truncated))
      throw boost::system::system_error(ec);
}

template <class s>
void http_call_impl<s>::read_body_exact() {

   auto &buf = response_buf;
   auto &body = response.body;

   auto avail = buf.size();

   if (avail > ((size_t)content_length))
      FC_THROW("invalid response body (content length mismatch)");

   body.resize(content_length);

   if (avail) {
      if (avail != ((size_t)buf.sgetn(&body[0], avail)))
         FC_THROW("stream read failed");
   }

   auto rest = content_length - avail;

   if (rest > 0) {
      auto readed = read(socket(), buffer(&body[avail], rest), transfer_exactly(rest));
      //ASSERT(readed <= rest);
      if (readed < rest)
         FC_THROW("logic error: read failed but no error conditon");
   }
}

template <class s>
void http_call_impl<s>::process_response() {

   auto &buf = response_buf;
   auto &body = response.body;

   read_until(socket(), buf, crlfcrlf);

   process_headers();

   // check content length

   if (content_length >= 0) {
      if (content_length < 2) { // minimum content is "{}"
         FC_THROW("invalid response body (too short)");
      }
      if (content_length > http_call::response_size_limit_bytes)
         FC_THROW("response body size limit exceeded");
      body.reserve(content_length);
   }

   if (transfer_encoding_chunked) {
      read_body_chunked();
   } else {
      if (content_length < 0)
         read_body_until_eof();
      else {
         if (content_length > 0)
            read_body_exact();
      }
   }
}

} // namespace detail

// https_call

http_call::http_call(const url_data &url, const std::string &method, const std::string &headers) :
      m_host(url.host),
      m_method(method),
      m_headers(headers) {

   if (url.schema_type == url_schema_type::https) {
      m_context = new boost::asio::ssl::context(ssl::context::tlsv12_client);
   } else {
      m_context = 0;
   }

   if (url.port)
      m_port_default = url.port;
   else {
      if (url.schema_type == url_schema_type::https)
         m_port_default = https_port;
      else
         m_port_default = http_port;
   }

   m_port = m_port_default;

   set_path(url.path);

   try {
      ctor_priv();
   } catch (...) {
      if (m_context)
         delete m_context;
      throw;
   }
}

http_call::~http_call() {
   if (m_context)
      delete m_context;
}

bool http_call::is_ssl() const {
   return m_context != 0;
}

const std::string &http_call::path() const {
   return m_path;
}

void http_call::set_path(const std::string &path) {
   if (path.empty())
      m_path = "/";
   else
      m_path = path;
}

void http_call::set_method(const std::string &method) {
   m_method = method;
}

void http_call::set_headers(const std::string &headers) {
   m_headers = headers;
}

const std::string &http_call::host() const {
   return m_host;
}

void http_call::set_host(const std::string &host) {
   m_host = host;
}

uint16_t http_call::port() const {
   return m_port;
}

void http_call::set_port(uint16_t port) {
   if (port)
      m_port = port;
   else
      m_port = m_port_default;
}

bool http_call::exec(const http_request &request, http_response *response) {

   //ASSERT(response);
   auto &resp = *response;
   m_error_what = decltype(m_error_what)();
   resp.clear();

   try {
      try {
         using namespace detail;
         if (!m_context)
            http_call_impl<tcp_socket>(*this, request.body.data(), request.body.size(), request.content_type, resp).exec();
         else
            http_call_impl<ssl_socket>(*this, request.body.data(), request.body.size(), request.content_type, resp).exec();
         return true;
      } catch (const std::exception &e) {
         m_error_what = e.what();
      }
   } catch (...) {
      m_error_what = "unknown exception";
   }

   resp.clear();
   return false;
}

const std::string &http_call::error_what() const {
   return m_error_what;
}

void http_call::ctor_priv() {
   if (m_context) {
      m_context->set_default_verify_paths();
      m_context->set_options(ssl::context::default_workarounds);
   }
}

}} // namespace graphene::peerplays_sidechain

namespace graphene { namespace peerplays_sidechain {

rpc_client::rpc_client(const std::string &url, const std::string &user_name, const std::string &password, bool debug) :
      debug_rpc_calls(debug),
      request_id(0),
      client(url)

{

   client.set_method("POST");
   client.set_headers("Authorization : Basic" + fc::base64_encode(user_name + ":" + password));
}

std::string rpc_client::retrieve_array_value_from_reply(std::string reply_str, std::string array_path, uint32_t idx) {
   if (reply_str.empty())
      return std::string();
   std::stringstream ss(reply_str);
   boost::property_tree::ptree json;
   boost::property_tree::read_json(ss, json);
   if (json.find("result") == json.not_found()) {
      return std::string();
   }
   auto json_result = json.get_child("result");
   if (json_result.find(array_path) == json_result.not_found()) {
      return std::string();
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

   return std::string();
}

std::string rpc_client::retrieve_value_from_reply(std::string reply_str, std::string value_path) {
   if (reply_str.empty())
      return std::string();
   std::stringstream ss(reply_str);
   boost::property_tree::ptree json;
   boost::property_tree::read_json(ss, json);
   if (json.find("result") == json.not_found()) {
      return std::string();
   }
   auto json_result = json.get_child("result");
   if (json_result.find(value_path) == json_result.not_found()) {
      return std::string();
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

   if (reply.status_code == 200) {
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

//static size_t write_callback(char *ptr, size_t size, size_t nmemb, rpc_reply *reply) {
//   size_t retval = 0;
//   if (reply != nullptr) {
//      reply->body.append(ptr, size * nmemb);
//      retval = size * nmemb;
//   }
//   return retval;
//}

//rpc_reply rpc_client::send_post_request(std::string body, bool show_log) {
//
//   struct curl_slist *headers = nullptr;
//   headers = curl_slist_append(headers, "Accept: application/json");
//   headers = curl_slist_append(headers, "Content-Type: application/json");
//   headers = curl_slist_append(headers, "charset: utf-8");
//
//   CURL *curl = curl_easy_init();
//   if (ip.find("https://", 0) != 0) {
//      curl_easy_setopt(curl, CURLOPT_URL, ip.c_str());
//      curl_easy_setopt(curl, CURLOPT_PORT, port);
//   } else {
//      std::string full_address = ip + ":" + std::to_string(port);
//      curl_easy_setopt(curl, CURLOPT_URL, full_address.c_str());
//      curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, false);
//      curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, false);
//   }
//   if (!user.empty()) {
//      curl_easy_setopt(curl, CURLOPT_USERNAME, user.c_str());
//      curl_easy_setopt(curl, CURLOPT_PASSWORD, password.c_str());
//   }
//
//   curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
//   curl_easy_setopt(curl, CURLOPT_POSTFIELDS, body.c_str());
//
//   //curl_easy_setopt(curl, CURLOPT_VERBOSE, true);
//
//   rpc_reply reply;
//
//   curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_callback);
//   curl_easy_setopt(curl, CURLOPT_WRITEDATA, &reply);
//
//   curl_easy_perform(curl);
//
//   curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &reply.status);
//
//   curl_easy_cleanup(curl);
//   curl_slist_free_all(headers);
//
//   if (show_log) {
//      std::string url = ip + ":" + std::to_string(port);
//      ilog("### Request URL:    ${url}", ("url", url));
//      ilog("### Request:        ${body}", ("body", body));
//      std::stringstream ss(std::string(reply.body.begin(), reply.body.end()));
//      ilog("### Response:       ${ss}", ("ss", ss.str()));
//   }
//
//   return reply;
//}

http_response rpc_client::send_post_request(const std::string &body, bool show_log) {

   http_request request(body, "application/json");
   http_response response;

   client.exec(request, &response);

   if (show_log) {
      std::string url = client.is_ssl() ? "https" : "http";
      url = url + "://" + client.host() + ":" + std::to_string(client.port()) + client.path();
      ilog("### Request URL:    ${url}", ("url", url));
      ilog("### Request:        ${body}", ("body", body));
      std::stringstream ss(std::string(response.body.begin(), response.body.end()));
      ilog("### Response:       ${ss}", ("ss", ss.str()));
   }

   return response;
}

}} // namespace graphene::peerplays_sidechain
