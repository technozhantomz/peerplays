#include <graphene/chain/database.hpp>
#include <graphene/chain/son_object.hpp>

namespace graphene { namespace chain {
   void son_object::pay_son_fee(share_type pay, database& db) {
      db.adjust_balance(son_account, pay);
   }

   bool son_object::has_valid_config(sidechain_type sidechain) const {
      return (sidechain_public_keys.find( sidechain ) != sidechain_public_keys.end()) &&
         (sidechain_public_keys.at(sidechain).length() > 0);
   }

   bool son_object::has_valid_config(time_point_sec head_block_time, sidechain_type sidechain) const {
      bool retval = (std::string(signing_key).length() > 0) && (sidechain_public_keys.size() > 0);

      if (head_block_time < HARDFORK_SON_FOR_HIVE_TIME) {
         retval = retval && has_valid_config(sidechain_type::bitcoin);
      }
      if (head_block_time >= HARDFORK_SON_FOR_HIVE_TIME && head_block_time < HARDFORK_SON_FOR_ETHEREUM_TIME) {
         retval = retval && has_valid_config(sidechain_type::bitcoin) && has_valid_config(sidechain_type::hive);
      }
      else if (head_block_time >= HARDFORK_SON_FOR_ETHEREUM_TIME) {
         retval = retval && has_valid_config(sidechain);
      }

      return retval;
   }
}}
