#include <boost/property_tree/json_parser.hpp>
#include <boost/property_tree/ptree.hpp>
#include <fc/crypto/base58.hpp>
#include <graphene/peerplays_sidechain/bitcoin/utils.hpp>

namespace graphene { namespace peerplays_sidechain { namespace bitcoin {

fc::ecc::public_key_data create_public_key_data(const std::vector<char> &public_key) {
   FC_ASSERT(public_key.size() == 33);
   fc::ecc::public_key_data key;
   for (size_t i = 0; i < 33; i++) {
      key.at(i) = public_key[i];
   }
   return key;
}

bytes get_privkey_bytes(const std::string &privkey_base58) {
   const auto privkey = fc::from_base58(privkey_base58);
   // Remove version and hash
   return bytes(privkey.cbegin() + 1, privkey.cbegin() + 1 + 32);
}

bytes parse_hex(const std::string &str) {
   bytes vec(str.size() / 2);
   fc::from_hex(str, vec.data(), vec.size());
   return vec;
}

std::vector<bytes> get_pubkey_from_redeemScript(bytes script) {
   FC_ASSERT(script.size() >= 37);

   script.erase(script.begin());
   script.erase(script.end() - 2, script.end());

   std::vector<bytes> result;
   uint64_t count = script.size() / 34;
   for (size_t i = 0; i < count; i++) {
      result.push_back(bytes(script.begin() + (34 * i) + 1, script.begin() + (34 * (i + 1))));
   }
   return result;
}

bytes public_key_data_to_bytes(const fc::ecc::public_key_data &key) {
   bytes result;
   result.resize(key.size());
   std::copy(key.begin(), key.end(), result.begin());
   return result;
}

std::vector<bytes> read_byte_arrays_from_string(const std::string &string_buf) {
   std::stringstream ss(string_buf);
   boost::property_tree::ptree json;
   boost::property_tree::read_json(ss, json);

   std::vector<bytes> data;
   for (auto &v : json) {
      std::string hex = v.second.data();
      bytes item;
      item.resize(hex.size() / 2);
      fc::from_hex(hex, (char *)&item[0], item.size());
      data.push_back(item);
   }
   return data;
}

std::string write_transaction_signatures(const std::vector<bytes> &data) {
   std::string res = "[";
   for (unsigned int idx = 0; idx < data.size(); ++idx) {
      res += "\"" + fc::to_hex((char *)&data[idx][0], data[idx].size()) + "\"";
      if (idx != data.size() - 1)
         res += ",";
   }
   res += "]";
   return res;
}

void read_transaction_data(const std::string &string_buf, std::string &tx_hex, std::vector<uint64_t> &in_amounts, std::string &redeem_script) {
   std::stringstream ss(string_buf);
   boost::property_tree::ptree json;
   boost::property_tree::read_json(ss, json);
   tx_hex = json.get<std::string>("tx_hex");
   in_amounts.clear();
   for (auto &v : json.get_child("in_amounts"))
      in_amounts.push_back(fc::to_uint64(v.second.data()));
   redeem_script = json.get<std::string>("redeem_script");
}

std::string write_transaction_data(const std::string &tx, const std::vector<uint64_t> &in_amounts, const std::string &redeem_script) {
   std::string res = "{\"tx_hex\":\"" + tx + "\",\"in_amounts\":[";
   for (unsigned int idx = 0; idx < in_amounts.size(); ++idx) {
      res += fc::to_string(in_amounts[idx]);
      if (idx != in_amounts.size() - 1)
         res += ",";
   }
   res += "],\"redeem_script\":\"" + redeem_script + "\"}";
   return res;
}

}}} // namespace graphene::peerplays_sidechain::bitcoin
