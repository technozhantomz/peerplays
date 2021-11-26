#include <graphene/chain/database.hpp>
#include <graphene/chain/son_object.hpp>

namespace graphene { namespace chain {
   void son_object::pay_son_fee(share_type pay, database& db) {
      db.adjust_balance(son_account, pay);
   }

   bool son_object::has_valid_config()const {
      return ((std::string(signing_key).length() > 0) &&
             (sidechain_public_keys.size() > 0) &&
             (sidechain_public_keys.find( sidechain_type::bitcoin ) != sidechain_public_keys.end()) &&
             (sidechain_public_keys.at(sidechain_type::bitcoin).length() > 0));
   }

   bool son_object::has_valid_config(time_point_sec head_block_time)const {
      bool retval = has_valid_config();

      if (head_block_time >= HARDFORK_SON_FOR_HIVE_TIME) {
          retval = retval &&
                   (sidechain_public_keys.find( sidechain_type::hive ) != sidechain_public_keys.end()) &&
                   (sidechain_public_keys.at(sidechain_type::hive).length() > 0);
      }

      return retval;
   }
}}
