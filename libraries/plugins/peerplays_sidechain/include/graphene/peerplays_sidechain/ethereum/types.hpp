#pragma once

#include <boost/multiprecision/cpp_int.hpp>

namespace graphene { namespace peerplays_sidechain { namespace ethereum {

typedef uint64_t chain_id_type;
typedef uint64_t network_id_type;

using bytes = std::vector<char>;

class signature {
public:
   std::string v;
   std::string r;
   std::string s;

   signature() = default;
   signature(const std::string &sign);

   std::string serialize() const;
   void deserialize(const std::string &sign);
};

}}} // namespace graphene::peerplays_sidechain::ethereum
