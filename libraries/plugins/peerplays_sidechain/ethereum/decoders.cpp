#include <graphene/peerplays_sidechain/ethereum/decoders.hpp>

#include <fc/exception/exception.hpp>
#include <graphene/peerplays_sidechain/ethereum/utils.hpp>

namespace graphene { namespace peerplays_sidechain { namespace ethereum {

//! base_decoder
boost::multiprecision::uint256_t base_decoder::decode_uint256(const std::string &value) {
   boost::multiprecision::uint256_t result = 0;

   boost::multiprecision::uint256_t power(1);
   uint8_t digit;
   int pos = value.size() - 1;
   while (pos >= 0) {
      digit = 0;
      if ('0' <= value[pos] && value[pos] <= '9') {
         digit = value[pos] - '0';
      } else if ('a' <= value[pos] && value[pos] <= 'z') {
         digit = value[pos] - 'a' + 10;
      }
      result += digit * power;
      pos--;
      power *= 16;
   }

   return result;
}

std::string base_decoder::decode_address(const std::string &value) {
   return value.substr(24, 40);
}

//! deposit_erc20_decoder
const std::string deposit_erc20_decoder::function_signature = "97feb926"; //! depositERC20(address,uint256)
fc::optional<deposit_erc20_transaction> deposit_erc20_decoder::decode(const std::string &input) {
   const auto input_without_0x = remove_0x(input);
   if (function_signature != input_without_0x.substr(0, 8)) {
      return fc::optional<deposit_erc20_transaction>{};
   }
   if (input_without_0x.size() != 136) {
      return fc::optional<deposit_erc20_transaction>{};
   }

   deposit_erc20_transaction erc_20;
   erc_20.token = add_0x(base_decoder::decode_address(input_without_0x.substr(8, 64)));
   erc_20.amount = base_decoder::decode_uint256(input_without_0x.substr(72, 64));

   return erc_20;
}

//! rlp_decoder
namespace {
const signed char p_util_hexdigit[256] =
      {-1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
       -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
       -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
       0, 1, 2, 3, 4, 5, 6, 7, 8, 9, -1, -1, -1, -1, -1, -1,
       -1, 0xa, 0xb, 0xc, 0xd, 0xe, 0xf, -1, -1, -1, -1, -1, -1, -1, -1, -1,
       -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
       -1, 0xa, 0xb, 0xc, 0xd, 0xe, 0xf, -1, -1, -1, -1, -1, -1, -1, -1, -1,
       -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
       -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
       -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
       -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
       -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
       -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
       -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
       -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
       -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1};
}

std::vector<std::string> rlp_decoder::decode(const std::string &str) {
   size_t consumed = 0;
   const auto raw_vec = parse_hex(str);
   const std::vector<std::string> rlp_array = decode_rlp(raw_vec.data(), raw_vec.size(), consumed);
   std::vector<std::string> result_array;
   for (const auto &rlp : decode_rlp(raw_vec.data(), raw_vec.size(), consumed)) {
      result_array.emplace_back(bytes2hex(rlp));
   }
   return result_array;
}

std::vector<std::string> rlp_decoder::decode_rlp(const unsigned char *raw, size_t len, size_t &consumed) {
   std::vector<std::string> rlp_result;

   consumed = 0;

   const unsigned char *end = raw + len;
   const size_t prefixlen = 1;
   unsigned char ch = *raw;

   if (len < 1) {
      return rlp_result;
   }

   // Case 1: [prefix is 1-byte data buffer]
   if (ch <= 0x7f) {
      const unsigned char *tok_start = raw;
      const unsigned char *tok_end = tok_start + prefixlen;
      FC_ASSERT(tok_end <= end);

      // parsing done; assign data buffer value.
      const std::vector<unsigned char> buf{tok_start, tok_end};
      rlp_result.emplace_back(buf.cbegin(), buf.cend());

      consumed = buf.size();
   }
   // Case 2: [prefix, including buffer length][data]
   else if ((ch >= 0x80) && (ch <= 0xb7)) {
      const size_t blen = ch - 0x80;
      const size_t expected = prefixlen + blen;

      if (len < expected)
         return std::vector<std::string>{};

      const unsigned char *tok_start = raw + 1;
      const unsigned char *tok_end = tok_start + blen;
      FC_ASSERT(tok_end <= end);

      // require minimal encoding
      if ((blen == 1) && (tok_start[0] <= 0x7f))
         return std::vector<std::string>{};

      // parsing done; assign data buffer value.
      const std::vector<unsigned char> buf{tok_start, tok_end};
      rlp_result.emplace_back(buf.cbegin(), buf.cend());

      consumed = expected;
   }
   // Case 3: [prefix][buffer length][data]
   else if ((ch >= 0xb8) && (ch <= 0xbf)) {
      const size_t uintlen = ch - 0xb7;
      size_t expected = prefixlen + uintlen;

      if (len < expected)
         return std::vector<std::string>{};

      FC_ASSERT(uintlen > 0 && uintlen <= RLP_maxUintLen);

      const unsigned char *tok_start = raw + prefixlen;
      if ((uintlen > 1) && (tok_start[0] == 0)) // no leading zeroes
         return std::vector<std::string>{};

      // read buffer length
      const uint64_t slen = to_int(tok_start, uintlen);

      // validate buffer length, including possible addition overflows.
      expected = prefixlen + uintlen + slen;
      if ((slen < (RLP_listStart - RLP_bufferLenStart - RLP_maxUintLen)) || (expected > len) || (slen > len))
         return std::vector<std::string>{};

      // parsing done; assign data buffer value.
      tok_start = raw + prefixlen + uintlen;
      const unsigned char *tok_end = tok_start + slen;
      const std::vector<unsigned char> buf{tok_start, tok_end};
      rlp_result.emplace_back(buf.cbegin(), buf.cend());

      consumed = expected;
   }
   // Case 4: [prefix][list]
   else if ((ch >= 0xc0) && (ch <= 0xf7)) {
      const size_t payloadlen = ch - 0xc0;
      const size_t expected = prefixlen + payloadlen;

      // read list payload
      const auto array = decode_array(raw, len, 0, payloadlen);
      rlp_result.insert(rlp_result.end(), array.cbegin(), array.cend());

      consumed = expected;
   }
   // Case 5: [prefix][list length][list]
   else {
      FC_ASSERT((ch >= 0xf8) && (ch <= 0xff));

      const size_t uintlen = ch - 0xf7;
      const size_t expected = prefixlen + uintlen;

      if (len < expected)
         return std::vector<std::string>{};

      FC_ASSERT(uintlen > 0 && uintlen <= RLP_maxUintLen);

      const unsigned char *tok_start = raw + prefixlen;
      if ((uintlen > 1) && (tok_start[0] == 0)) // no leading zeroes
         return std::vector<std::string>{};

      // read list length
      const size_t payloadlen = to_int(tok_start, uintlen);

      // special requirement for non-immediate length
      if (payloadlen < (0x100 - RLP_listStart - RLP_maxUintLen))
         return std::vector<std::string>{};

      // read list payload
      const auto array = decode_array(raw, len, uintlen, payloadlen);
      rlp_result.insert(rlp_result.end(), array.cbegin(), array.cend());

      consumed = prefixlen + uintlen + payloadlen;
   }

   return rlp_result;
}

std::vector<std::string> rlp_decoder::decode_array(const unsigned char *raw, size_t len, size_t uintlen, size_t payloadlen) {
   std::vector<std::string> rlp_result;
   const size_t prefixlen = 1;

   // validate list length, including possible addition overflows.
   const size_t expected = prefixlen + uintlen + payloadlen;
   if ((expected > len) || (payloadlen > len))
      return std::vector<std::string>{};

   size_t child_len = payloadlen;
   const unsigned char *list_ent = raw + prefixlen + uintlen;

   // recursively read until payloadlen bytes parsed, or error
   while (child_len > 0) {
      size_t child_consumed = 0;

      const auto val = decode_rlp(list_ent, child_len, child_consumed);
      rlp_result.insert(rlp_result.end(), val.cbegin(), val.cend());

      list_ent += child_consumed;
      child_len -= child_consumed;
   }

   return rlp_result;
}

uint64_t rlp_decoder::to_int(const unsigned char *raw, size_t len) {
   if (len == 0)
      return 0;
   else if (len == 1)
      return *raw;
   else
      return (raw[len - 1]) + (to_int(raw, len - 1) * 256);
}

std::vector<unsigned char> rlp_decoder::parse_hex(const std::string &str) {
   return parse_hex(str.c_str());
}

std::vector<unsigned char> rlp_decoder::parse_hex(const char *psz) {
   // convert hex dump to vector
   std::vector<unsigned char> vch;
   while (true) {
      while (isspace(*psz))
         psz++;
      signed char c = hex_digit(*psz++);
      if (c == (signed char)-1)
         break;
      unsigned char n = (c << 4);
      c = hex_digit(*psz++);
      if (c == (signed char)-1)
         break;
      n |= c;
      vch.push_back(n);
   }
   return vch;
}

signed char rlp_decoder::hex_digit(char c) {
   return p_util_hexdigit[(unsigned char)c];
}

}}} // namespace graphene::peerplays_sidechain::ethereum
