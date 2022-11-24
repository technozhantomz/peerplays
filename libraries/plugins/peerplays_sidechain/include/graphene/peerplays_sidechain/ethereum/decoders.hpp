#pragma once

#include <string>
#include <vector>

namespace graphene { namespace peerplays_sidechain { namespace ethereum {

class rlp_decoder {
private:
   enum RLP_constants {
      RLP_maxUintLen = 8,
      RLP_bufferLenStart = 0x80,
      RLP_listStart = 0xc0,
   };

public:
   static std::vector<std::string> decode(const std::string &str);

private:
   static std::vector<std::string> decode_rlp(const unsigned char *raw, size_t len, size_t &consumed);
   static std::vector<std::string> decode_array(const unsigned char *raw, size_t len, size_t uintlen, size_t payloadlen);
   static uint64_t to_int(const unsigned char *raw, size_t len);
   static std::vector<unsigned char> parse_hex(const std::string &str);
   static std::vector<unsigned char> parse_hex(const char *psz);
   static signed char hex_digit(char c);
};

}}} // namespace graphene::peerplays_sidechain::ethereum
