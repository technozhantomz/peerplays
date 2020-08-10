#pragma once
#include <graphene/chain/protocol/types.hpp>
#include <graphene/db/object.hpp>
#include <graphene/db/generic_index.hpp>

namespace graphene { namespace chain {
   using namespace graphene::db;

   /**
    * @class custom_account_authority_object
    * @brief Tracks the mappings between permission and operation types.
    * @ingroup object
    */
   class custom_account_authority_object : public abstract_object<custom_account_authority_object>
   {
      public:
         static const uint8_t space_id = protocol_ids;
         static const uint8_t type_id  = custom_account_authority_object_type;

         custom_permission_id_type permission_id;
         int operation_type;
         time_point_sec valid_from;
         time_point_sec valid_to;
   };

   struct by_id;
   struct by_permission_and_op;
   struct by_expiration;
   using custom_account_authority_multi_index_type = multi_index_container<
      custom_account_authority_object,
      indexed_by<
         ordered_unique< tag<by_id>,
            member<object, object_id_type, &object::id>
         >,
         ordered_unique< tag<by_permission_and_op>,
            composite_key<custom_account_authority_object,
               member<custom_account_authority_object, custom_permission_id_type, &custom_account_authority_object::permission_id>,
               member<custom_account_authority_object, int, &custom_account_authority_object::operation_type>,
               member<object, object_id_type, &object::id>
            >
         >,
         ordered_unique<tag<by_expiration>,
            composite_key<custom_account_authority_object,
               member<custom_account_authority_object, time_point_sec, &custom_account_authority_object::valid_to>,
               member<object, object_id_type, &object::id>
            >
         >
      >
   >;
   using custom_account_authority_index = generic_index<custom_account_authority_object, custom_account_authority_multi_index_type>;

} } // graphene::chain

FC_REFLECT_DERIVED( graphene::chain::custom_account_authority_object, (graphene::db::object),
                    (permission_id)(operation_type)(valid_from)(valid_to) )