
#include <graphene/peerplays_sidechain/bitcoin/estimate_fee_external.hpp>

#include <boost/property_tree/json_parser.hpp>
#include <boost/property_tree/ptree.hpp>

#include <fc/log/logger.hpp>

#include <iostream>

namespace graphene {
namespace peerplays_sidechain {

static size_t writeFunction(void *ptr, size_t size, size_t nmemb, std::string *data) {
   data->append((char *)ptr, size * nmemb);
   return size * nmemb;
}

estimate_fee_external::estimate_fee_external() {
   curl = curl_easy_init();
}

estimate_fee_external::~estimate_fee_external() {
   curl_easy_cleanup(curl);
}

std::vector<std::pair<std::string, uint64_t>> estimate_fee_external::get_fee_external(uint16_t target_block) {

   std::vector<std::pair<std::string, uint64_t>> estimate_fee_external_collection;
   this->target_block = target_block;

   for (auto &url_fee_parser : url_get_fee_parsers) {
      response = get_response(url_fee_parser.first);
      uint64_t fee = url_fee_parser.second();
      std::string url_str = url_fee_parser.first;
      if (fee != 0) {
         estimate_fee_external_collection.emplace_back(std::make_pair(url_fee_parser.first, fee));
      }
   }

   return estimate_fee_external_collection;
}

std::string estimate_fee_external::get_response(std::string url) {

   std::string response;
   if (curl) {
      curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
      curl_easy_setopt(curl, CURLOPT_NOPROGRESS, 1L);
      curl_easy_setopt(curl, CURLOPT_USERPWD, "user:pass");
      curl_easy_setopt(curl, CURLOPT_USERAGENT, "curl/7.42.0");
      curl_easy_setopt(curl, CURLOPT_MAXREDIRS, 50L);
      curl_easy_setopt(curl, CURLOPT_TCP_KEEPALIVE, 1L);
      curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, writeFunction);
      curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);
      curl_easy_perform(curl);
   }
   return response;
}

uint64_t estimate_fee_external::parse_and_get_fee_1() {
   //"https://www.bitgo.com/api/v2/btc/tx/fee"

   uint64_t founded_fee = 0;

   if (response.empty()) {
      return founded_fee;
   }

   std::stringstream response_ss(response);
   boost::property_tree::ptree response_pt;
   boost::property_tree::read_json(response_ss, response_pt);

   for (const auto &tx_child : response_pt.get_child("feeByBlockTarget")) {
      const auto &block_num = tx_child.first.data();
      const auto &fee = tx_child.second.data();

      founded_fee = std::stoi(fee);

      if (std::stoi(block_num) >= target_block) {
         return founded_fee;
      }
   }

   return founded_fee;
}

uint64_t estimate_fee_external::parse_and_get_fee_2() {
   // https://bitcoiner.live/api/fees/estimates/latest
   uint64_t founded_fee = 0;

   if (response.empty()) {
      return founded_fee;
   }

   std::stringstream response_ss(response);
   boost::property_tree::ptree response_pt;
   boost::property_tree::read_json(response_ss, response_pt);

   for (const auto &tx_child : response_pt.get_child("estimates")) {
      const auto &time_str = tx_child.first.data();

      auto time = std::stoi(time_str);
      auto block_num = time / 30;

      if (tx_child.second.count("sat_per_vbyte")) {
         auto founded_fee_str = tx_child.second.get_child("sat_per_vbyte").data();
         founded_fee = std::stoi(founded_fee_str) * 1000;
      }

      if (block_num >= target_block) {
         return founded_fee;
      }
   }

   return founded_fee;
}

uint64_t estimate_fee_external::parse_and_get_fee_3() {
   // https://api.blockchain.info/mempool/fees

   if (response.empty()) {
      return 0;
   }

   std::stringstream response_ss(response);
   boost::property_tree::ptree response_pt;
   boost::property_tree::read_json(response_ss, response_pt);

   if (response_pt.get_child("limits").count("min") && response_pt.get_child("limits").count("max")) {
      auto limits_min_str = response_pt.get_child("limits.min").data();
      auto limits_max_str = response_pt.get_child("limits.max").data();

      auto limits_min = std::stoi(limits_min_str);
      auto limits_max = std::stoi(limits_max_str);

      auto priority_max = (limits_max - (limits_min - 1)) / 2;

      if (response_pt.count("regular") && response_pt.count("priority")) {
         auto regular_str = response_pt.get_child("regular").data();
         auto priority_str = response_pt.get_child("priority").data();

         auto regular = std::stoi(regular_str);
         auto priority = std::stoi(priority_str);

         if (target_block >= priority_max) {
            return regular * 1000;
         } else {
            return priority * 1000;
         }
      }
   }

   return 0;
}

}} // namespace graphene::peerplays_sidechain