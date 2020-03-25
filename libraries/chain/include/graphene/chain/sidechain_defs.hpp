#pragma once

#include <fc/reflect/reflect.hpp>

namespace graphene { namespace chain {

enum class sidechain_type {
   bitcoin,
   ethereum,
   eos,
   peerplays
};

} }

namespace graphene { namespace peerplays_sidechain {

using sidechain_type = graphene::chain::sidechain_type;

} }

FC_REFLECT_ENUM(graphene::chain::sidechain_type,
        (bitcoin)
        (ethereum)
        (eos)
        (peerplays) )
