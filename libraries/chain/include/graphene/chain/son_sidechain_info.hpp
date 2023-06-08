#pragma once
#include <graphene/chain/protocol/types.hpp>
#include <graphene/chain/sidechain_defs.hpp>

namespace graphene { namespace chain {

   /**
    * @class son_sidechain_info
    * @brief tracks information about a SON info required to re/create primary wallet
    * @ingroup object
    */
   struct son_sidechain_info {
      son_id_type son_id;
      weight_type weight = 0;
      public_key_type signing_key;
      string public_key;

      bool operator==(const son_sidechain_info& rhs) const {
         bool son_sets_equal =
               (son_id == rhs.son_id) &&
               (weight == rhs.weight) &&
               (signing_key == rhs.signing_key) &&
               (public_key == rhs.public_key);

         return son_sets_equal;
      }
   };

} }

FC_REFLECT( graphene::chain::son_sidechain_info, (son_id) (weight) (signing_key) (public_key) )
