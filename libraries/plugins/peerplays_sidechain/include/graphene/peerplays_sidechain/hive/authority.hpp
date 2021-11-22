#pragma once

#include <cstdint>

#include <fc/container/flat_fwd.hpp>

#include <graphene/peerplays_sidechain/hive/types.hpp>

namespace graphene { namespace peerplays_sidechain { namespace hive {

struct authority {
   authority() {
   }

   enum classification {
      owner = 0,
      active = 1,
      key = 2,
      posting = 3
   };

   uint32_t weight_threshold = 0;
   fc::flat_map<hive::account_name_type, uint16_t> account_auths;
   fc::flat_map<hive::public_key_type, uint16_t> key_auths;
};

}}} // namespace graphene::peerplays_sidechain::hive

FC_REFLECT_ENUM(graphene::peerplays_sidechain::hive::authority::classification,
                (owner)(active)(key)(posting))

FC_REFLECT(graphene::peerplays_sidechain::hive::authority,
           (weight_threshold)(account_auths)(key_auths))
