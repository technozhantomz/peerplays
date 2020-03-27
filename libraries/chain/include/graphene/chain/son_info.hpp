#pragma once
#include <graphene/chain/protocol/types.hpp>
#include <graphene/chain/sidechain_defs.hpp>

namespace graphene { namespace chain {
   using namespace graphene::db;

   /**
    * @class son_info
    * @brief tracks information about a SON info required to re/create primary wallet
    * @ingroup object
    */
   struct son_info {
      son_id_type son_id;
      weight_type weight = 0;
      public_key_type signing_key;
      flat_map<sidechain_type, string> sidechain_public_keys;

      bool operator==(const son_info& rhs) {
         bool son_sets_equal =
               (son_id == rhs.son_id) &&
               (weight == rhs.weight) &&
               (signing_key == rhs.signing_key) &&
               (sidechain_public_keys.size() == rhs.sidechain_public_keys.size());

         if (son_sets_equal) {
            // Compare sidechain public keys
         }
         return son_sets_equal;
      }
   };

} }

FC_REFLECT( graphene::chain::son_info,
      (son_id)
      (weight)
      (signing_key)
      (sidechain_public_keys) )
