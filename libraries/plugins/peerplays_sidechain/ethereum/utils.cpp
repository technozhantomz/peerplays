#include <graphene/peerplays_sidechain/ethereum/utils.hpp>

#include <fc/crypto/hex.hpp>

namespace graphene { namespace peerplays_sidechain { namespace ethereum {

bytes parse_hex(const std::string &str) {
   bytes vec(str.size() / 2);
   fc::from_hex(str, vec.data(), vec.size());
   return vec;
}

std::string bytes2hex(const std::string &s) {
   std::string dest;
   for (const auto &i : s)
      dest += uchar2Hex((unsigned char)i);

   return dest;
}

std::string uchar2Hex(unsigned char n) {
   std::string dest;
   dest.resize(2);
   sprintf(&dest[0], "%X", n);

   if (n < (unsigned char)16) {
      dest[1] = dest[0];
      dest[0] = '0';
   }

   return dest;
}

std::string add_0x(const std::string &s) {
   if (s.size() > 1) {
      if (s.substr(0, 2) == "0x")
         return s;
   }

   return "0x" + s;
}

std::string remove_0x(const std::string &s) {
   if (s.size() > 1) {
      if (s.substr(0, 2) == "0x")
         return s.substr(2);
   }

   return s;
}

}}} // namespace graphene::peerplays_sidechain::ethereum
