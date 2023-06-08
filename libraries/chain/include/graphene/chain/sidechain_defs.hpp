#pragma once

#include <set>

#include <graphene/chain/hardfork.hpp>

#include <fc/reflect/reflect.hpp>
#include <fc/time.hpp>

namespace graphene { namespace chain {

enum class sidechain_type {
   unknown,
   bitcoin,
   ethereum,
   eos,
   peerplays,
   hive
};

static const std::set<sidechain_type> all_sidechain_types = {sidechain_type::bitcoin, sidechain_type::ethereum, sidechain_type::hive};

inline std::set<sidechain_type> active_sidechain_types(const fc::time_point_sec block_time) {
   std::set<sidechain_type> active_sidechain_types{};

   if (block_time >= HARDFORK_SON_TIME)
      active_sidechain_types.insert(sidechain_type::bitcoin);
   if (block_time >= HARDFORK_SON_FOR_HIVE_TIME)
      active_sidechain_types.insert(sidechain_type::hive);
   if (block_time >= HARDFORK_SON_FOR_ETHEREUM_TIME)
      active_sidechain_types.insert(sidechain_type::ethereum);

   return active_sidechain_types;
}

} // namespace chain
} // namespace graphene

FC_REFLECT_ENUM(graphene::chain::sidechain_type,
   (unknown)
   (bitcoin)
   (ethereum)
   (eos)
   (hive)
   (peerplays) )
