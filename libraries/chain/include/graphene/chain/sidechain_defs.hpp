#pragma once

#include <set>
#include <fc/reflect/reflect.hpp>

namespace graphene { namespace chain {

enum class sidechain_type {
   unknown,
   bitcoin,
   ethereum,
   eos,
   peerplays,
   hive
};

static const std::set<sidechain_type> active_sidechain_types = {sidechain_type::bitcoin, sidechain_type::ethereum, sidechain_type::hive};

} }

FC_REFLECT_ENUM(graphene::chain::sidechain_type,
   (unknown)
   (bitcoin)
   (ethereum)
   (eos)
   (hive)
   (peerplays) )
