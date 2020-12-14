#pragma once
#include <graphene/chain/protocol/types.hpp>
#include <graphene/db/object.hpp>
#include <graphene/db/generic_index.hpp>

namespace graphene { namespace chain {
   using namespace graphene::db;

   /**
    * @class custom_permission_object
    * @brief Tracks all the custom permission of an account.
    * @ingroup object
    */
   class custom_permission_object : public abstract_object<custom_permission_object>
   {
      public:
         static const uint8_t space_id = protocol_ids;
         static const uint8_t type_id  = custom_permission_object_type;

         // Account for which this permission is being created
         account_id_type account;
         // Permission name
         string permission_name;
         // Authority required for this permission
         authority auth;
   };

   struct by_id;
   struct by_account_and_permission;
   using custom_permission_multi_index_type = multi_index_container<
      custom_permission_object,
      indexed_by<
         ordered_unique< tag<by_id>,
            member<object, object_id_type, &object::id>
         >,
         ordered_unique< tag<by_account_and_permission>,
            composite_key<custom_permission_object,
               member<custom_permission_object, account_id_type, &custom_permission_object::account>,
               member<custom_permission_object, string, &custom_permission_object::permission_name>
            >
         >
      >
   >;
   using custom_permission_index = generic_index<custom_permission_object, custom_permission_multi_index_type>;

} } // graphene::chain

FC_REFLECT_DERIVED( graphene::chain::custom_permission_object, (graphene::db::object),
                    (account)(permission_name)(auth) )