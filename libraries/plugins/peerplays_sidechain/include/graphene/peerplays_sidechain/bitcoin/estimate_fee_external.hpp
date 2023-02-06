#pragma once

#include <curl/curl.h>

#include <functional>
#include <map>
#include <vector>

typedef std::function<uint64_t()> get_fee_func_type;

namespace graphene { namespace peerplays_sidechain {

class estimate_fee_external {
public:
   estimate_fee_external();
   ~estimate_fee_external();
   std::vector<std::pair<std::string, uint64_t>> get_fee_external(uint16_t target_block);

private:
   std::string get_response(std::string url);
   // Here add your custom parser for external url. Take care of incremental name
   // and populate the list of url_parsers bellow paired with the function
   uint64_t parse_and_get_fee_1();
   uint64_t parse_and_get_fee_2();
   uint64_t parse_and_get_fee_3();

   const std::map<std::string, get_fee_func_type> url_get_fee_parsers{
         {"https://www.bitgo.com/api/v2/btc/tx/fee", std::bind(&estimate_fee_external::parse_and_get_fee_1, this)},
         {"https://bitcoiner.live/api/fees/estimates/latest", std::bind(&estimate_fee_external::parse_and_get_fee_2, this)},
         {"https://api.blockchain.info/mempool/fees", std::bind(&estimate_fee_external::parse_and_get_fee_3, this)}};

   std::string response;
   uint16_t target_block;
   CURL *curl{nullptr};
};

}} // namespace graphene::peerplays_sidechain