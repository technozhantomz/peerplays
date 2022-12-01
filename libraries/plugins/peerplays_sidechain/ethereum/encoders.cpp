#include <graphene/peerplays_sidechain/ethereum/encoders.hpp>

#include <boost/algorithm/hex.hpp>
#include <boost/format.hpp>

#include <graphene/peerplays_sidechain/ethereum/utils.hpp>

namespace graphene { namespace peerplays_sidechain { namespace ethereum {

//! base_encoder
std::string base_encoder::encode_uint256(boost::multiprecision::uint256_t value) {
   return (boost::format("%x") % boost::io::group(std::setw(64), std::setfill('0'), value)).str();
}

std::string base_encoder::encode_address(const std::string &value) {
   return (boost::format("%x") % boost::io::group(std::setw(64), std::setfill('0'), value)).str();
}

std::string base_encoder::encode_string(const std::string &value) {
   std::string data = (boost::format("%x") % boost::io::group(std::setw(64), std::setfill('0'), value.size())).str();
   data += boost::algorithm::hex(value);
   if (value.size() % 32 != 0) {
      data += std::string((64 - value.size() * 2 % 64), '0');
   }
   return data;
}

//! update_owners_encoder
const std::string update_owners_encoder::function_signature = "f6afbeff"; //! updateOwners_(address,(address,uint256)[],string)
std::string update_owners_encoder::encode(const std::vector<std::pair<std::string, uint16_t>> &owners_weights, const std::string &object_id) {
   std::string data = "0x" + function_signature;
   data += base_encoder::encode_uint256(64);
   data += base_encoder::encode_uint256((owners_weights.size() * 2 + 3) * 32);
   data += base_encoder::encode_uint256(owners_weights.size());
   for (const auto &owner : owners_weights) {
      data += base_encoder::encode_address(owner.first);
      data += base_encoder::encode_uint256(owner.second);
   }
   data += base_encoder::encode_string(object_id);

   return data;
}

//! withdrawal_encoder
const std::string withdrawal_encoder::function_signature = "cf7c8f6d"; //! withdraw_(address,address,uint256,string)
std::string withdrawal_encoder::encode(const std::string &to, boost::multiprecision::uint256_t amount, const std::string &object_id) {
   std::string data = "0x" + function_signature;
   data += base_encoder::encode_address(to);
   data += base_encoder::encode_uint256(amount);
   data += base_encoder::encode_uint256(32 * 3);
   data += base_encoder::encode_string(object_id);

   return data;
}

//! signature_encoder
signature_encoder::signature_encoder(const std::string &function_hash) :
      function_signature{function_hash} {
}

std::string signature_encoder::get_function_signature_from_transaction(const std::string &transaction) {
   const std::string tr = remove_0x(transaction);
   if (tr.substr(0, 8) == update_owners_encoder::function_signature)
      return update_owners_function_signature;

   if (tr.substr(0, 8) == withdrawal_encoder::function_signature)
      return withdrawal_function_signature;

   return "";
}

std::string signature_encoder::encode(const std::vector<encoded_sign_transaction> &transactions) const {
   std::string data = "0x" + function_signature;
   data += base_encoder::encode_uint256(32);
   data += base_encoder::encode_uint256(transactions.size());
   size_t offset = (transactions.size()) * 32;
   for (const auto &transaction : transactions) {
      data += base_encoder::encode_uint256(offset);
      const auto transaction_data = remove_0x(transaction.data);
      offset += 5 * 32 + transaction_data.size() / 2;
      if (transaction_data.size() / 2 % 32 != 0) {
         offset += 32 - transaction_data.size() / 2 % 32;
      }
   }
   for (const auto &transaction : transactions) {
      data += base_encoder::encode_uint256(4 * 32);
      data += base_encoder::encode_address(transaction.sign.v);
      data += base_encoder::encode_address(transaction.sign.r);
      data += base_encoder::encode_address(transaction.sign.s);
      const auto transaction_data = remove_0x(transaction.data);
      data += base_encoder::encode_uint256(transaction_data.size() / 2);
      data += transaction_data;
      if (transaction_data.size() % 64 != 0) {
         data += std::string((64 - transaction_data.size() % 64), '0');
      }
   }

   return data;
}

//! rlp_encoder
std::string rlp_encoder::encode(const std::string &s) {
   return encode_rlp(hex2bytes(s));
}

std::string rlp_encoder::encode_length(int len, int offset) {
   if (len < 56) {
      std::string temp;
      temp = (char)(len + offset);
      return temp;
   } else {
      const std::string hexLength = to_hex(len);
      const int lLength = hexLength.size() / 2;
      const std::string fByte = to_hex(offset + 55 + lLength);
      return hex2bytes(fByte + hexLength);
   }
}

std::string rlp_encoder::hex2bytes(const std::string &s) {
   std::string dest;
   dest.resize(s.size() / 2);
   hex2bin(s.c_str(), &dest[0]);
   return dest;
}

std::string rlp_encoder::encode_rlp(const std::string &s) {
   if (s.size() == 1 && (unsigned char)s[0] < 128)
      return s;
   else
      return encode_length(s.size(), 128) + s;
}

int rlp_encoder::char2int(char input) {
   if (input >= '0' && input <= '9')
      return input - '0';
   if (input >= 'A' && input <= 'F')
      return input - 'A' + 10;
   if (input >= 'a' && input <= 'f')
      return input - 'a' + 10;

   return -1;
}

void rlp_encoder::hex2bin(const char *src, char *target) {
   while (*src && src[1]) {
      *(target++) = char2int(*src) * 16 + char2int(src[1]);
      src += 2;
   }
}

}}} // namespace graphene::peerplays_sidechain::ethereum
