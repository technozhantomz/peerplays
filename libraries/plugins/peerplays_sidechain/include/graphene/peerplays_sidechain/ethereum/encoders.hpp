#pragma once

#include <boost/multiprecision/cpp_int.hpp>
#include <string>
#include <vector>

#include <graphene/peerplays_sidechain/ethereum/types.hpp>

namespace graphene { namespace peerplays_sidechain { namespace ethereum {

struct encoded_sign_transaction {
   std::string data;
   signature sign;
};

class base_encoder {
public:
   static std::string encode_uint256(boost::multiprecision::uint256_t value);
   static std::string encode_address(const std::string &value);
   static std::string encode_string(const std::string &value);
};

class update_owners_encoder {
public:
   static const std::string function_signature;

   static std::string encode(const std::vector<std::pair<std::string, uint16_t>> &owners_weights, const std::string &object_id);
};

class withdrawal_encoder {
public:
   static const std::string function_signature;

   static std::string encode(const std::string &to, boost::multiprecision::uint256_t amount, const std::string &object_id);
};

class withdrawal_erc20_encoder {
public:
   static const std::string function_signature;

   static std::string encode(const std::string &token, const std::string &to, boost::multiprecision::uint256_t amount, const std::string &object_id);
};

class signature_encoder {
public:
   const std::string function_signature;

   signature_encoder(const std::string &function_hash);

   static std::string get_function_signature_from_transaction(const std::string &transaction);

   std::string encode(const std::vector<encoded_sign_transaction> &transactions) const;
};

class rlp_encoder {
public:
   static std::string encode(const std::string &s);
   static std::string encode_length(int len, int offset);
   static std::string hex2bytes(const std::string &s);

private:
   static std::string encode_rlp(const std::string &s);
   static int char2int(char input);
   static void hex2bin(const char *src, char *target);
};

}}} // namespace graphene::peerplays_sidechain::ethereum
