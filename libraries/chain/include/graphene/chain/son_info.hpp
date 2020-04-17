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
            bool sidechain_public_keys_equal = true;
            for (size_t i = 0; i < sidechain_public_keys.size(); i++) {
                const auto lhs_scpk = sidechain_public_keys.nth(i);
                const auto rhs_scpk = rhs.sidechain_public_keys.nth(i);
                sidechain_public_keys_equal = sidechain_public_keys_equal &&
                        (lhs_scpk->first == rhs_scpk->first) &&
                        (lhs_scpk->second == rhs_scpk->second);
            }
            son_sets_equal = son_sets_equal && sidechain_public_keys_equal;
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
