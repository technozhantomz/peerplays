#pragma once

namespace graphene { namespace chain {
   using namespace graphene::db;

   class random_number_object : public abstract_object<random_number_object>
   {
      public:
         static const uint8_t space_id = protocol_ids;
         static const uint8_t type_id  = random_number_object_type;

         account_id_type account; /* account who requested random number */
         time_point_sec timestamp; /* date and time when the number is read */
         vector<uint64_t> random_number; /* random number(s) */
         std::string data; /* custom data in json format */
   };

   struct by_account;
   struct by_timestamp;
   using random_number_multi_index_type = multi_index_container<
      random_number_object,
      indexed_by<
         ordered_unique< tag<by_id>,
            member<object, object_id_type, &object::id>
         >,
         ordered_non_unique< tag<by_account>,
            member<random_number_object, account_id_type, &random_number_object::account>
         >,
         ordered_non_unique< tag<by_timestamp>,
            member<random_number_object, time_point_sec, &random_number_object::timestamp>
         >
      >
   >;
   using random_number_index = generic_index<random_number_object, random_number_multi_index_type>;

} } // graphene::chain

FC_REFLECT_DERIVED( graphene::chain::random_number_object, (graphene::db::object),
                    (account) (timestamp)
                    (random_number) (data) )

