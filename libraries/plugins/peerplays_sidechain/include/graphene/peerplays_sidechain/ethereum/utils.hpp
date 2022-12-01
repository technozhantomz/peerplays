#pragma once

#include <graphene/peerplays_sidechain/ethereum/types.hpp>

namespace graphene { namespace peerplays_sidechain { namespace ethereum {

bytes parse_hex(const std::string &str);

std::string bytes2hex(const std::string &s);

std::string uchar2Hex(unsigned char n);

std::string add_0x(const std::string &s);

std::string remove_0x(const std::string &s);

template <typename T>
std::string to_hex(const T &val, bool add_front_zero = true) {
   std::stringstream stream;
   stream << std::hex << val;
   std::string result(stream.str());
   if (add_front_zero) {
      if (result.size() % 2)
         result = "0" + result;
   }
   return result;
}

template <typename T>
T from_hex(const std::string &s) {
   T val;
   std::stringstream stream;
   stream << std::hex << s;
   stream >> val;

   return val;
}

}}} // namespace graphene::peerplays_sidechain::ethereum
