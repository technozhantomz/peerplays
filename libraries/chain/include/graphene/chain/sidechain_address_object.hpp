#pragma once
#include <graphene/chain/protocol/types.hpp>
#include <graphene/db/object.hpp>
#include <graphene/db/generic_index.hpp>

#include <graphene/chain/sidechain_defs.hpp>
#include <boost/multi_index/composite_key.hpp>

namespace graphene { namespace chain {
   using namespace graphene::db;

   /**
    * @class sidechain_address_object
    * @brief tracks information about a sidechain addresses for user accounts.
    * @ingroup object
    */
   class sidechain_address_object : public abstract_object<sidechain_address_object>
   {
      public:
         static const uint8_t space_id = protocol_ids;
         static const uint8_t type_id  = sidechain_address_object_type;

         account_id_type sidechain_address_account;
         sidechain_type sidechain;
         string deposit_public_key;
         string deposit_address;
         string withdraw_public_key;
         string withdraw_address;

         sidechain_address_object() :
            sidechain(sidechain_type::bitcoin),
            deposit_public_key(""),
            deposit_address(""),
            withdraw_public_key(""),
            withdraw_address("") {}
   };

   struct by_account;
   struct by_sidechain;
   struct by_deposit_public_key;
   struct by_withdraw_public_key;
   struct by_account_and_sidechain;
   struct by_sidechain_and_deposit_address;
   using sidechain_address_multi_index_type = multi_index_container<
      sidechain_address_object,
      indexed_by<
         ordered_unique< tag<by_id>,
            member<object, object_id_type, &object::id>
         >,
         ordered_non_unique< tag<by_account>,
            member<sidechain_address_object, account_id_type, &sidechain_address_object::sidechain_address_account>
         >,
         ordered_non_unique< tag<by_sidechain>,
            member<sidechain_address_object, sidechain_type, &sidechain_address_object::sidechain>
         >,
         ordered_non_unique< tag<by_deposit_public_key>,
            member<sidechain_address_object, std::string, &sidechain_address_object::deposit_public_key>
         >,
         ordered_non_unique< tag<by_withdraw_public_key>,
            member<sidechain_address_object, std::string, &sidechain_address_object::withdraw_public_key>
         >,
         ordered_unique< tag<by_account_and_sidechain>,
            composite_key<sidechain_address_object,
               member<sidechain_address_object, account_id_type, &sidechain_address_object::sidechain_address_account>,
               member<sidechain_address_object, sidechain_type, &sidechain_address_object::sidechain>
            >
         >,
         ordered_unique< tag<by_sidechain_and_deposit_address>,
            composite_key<sidechain_address_object,
               member<sidechain_address_object, sidechain_type, &sidechain_address_object::sidechain>,
               member<sidechain_address_object, std::string, &sidechain_address_object::deposit_address>
            >
         >
      >
   >;
   using sidechain_address_index = generic_index<sidechain_address_object, sidechain_address_multi_index_type>;

} } // graphene::chain

FC_REFLECT_DERIVED( graphene::chain::sidechain_address_object, (graphene::db::object),
                    (sidechain_address_account) (sidechain) (deposit_public_key) (deposit_address) (withdraw_public_key) (withdraw_address) )
