#pragma once
#include <graphene/chain/protocol/types.hpp>
#include <graphene/peerplays_sidechain/defs.hpp>

namespace graphene { namespace chain {
   using namespace graphene::db;

   /**
    * @class bitcoin_transaction_object
    * @brief tracks state of bitcoin transaction.
    * @ingroup object
    */
   class bitcoin_transaction_object : public abstract_object<bitcoin_transaction_object>
   {
      public:
         static const uint8_t space_id = protocol_ids;
         static const uint8_t type_id  = bitcoin_transaction_object_type;
         // Bitcoin structs go here.
         bool processed = false;
         fc::flat_map<son_id_type, std::vector<peerplays_sidechain::bytes>> signatures;
   };

   struct by_processed;
   using bitcoin_transaction_multi_index_type = multi_index_container<
      bitcoin_transaction_object,
      indexed_by<
         ordered_unique< tag<by_id>,
            member<object, object_id_type, &object::id>
         >,
         ordered_non_unique< tag<by_processed>,
            member<bitcoin_transaction_object, bool, &bitcoin_transaction_object::processed>
         >
      >
   >;
   using bitcoin_transaction_index = generic_index<bitcoin_transaction_object, bitcoin_transaction_multi_index_type>;
} } // graphene::chain

FC_REFLECT_DERIVED( graphene::chain::bitcoin_transaction_object, (graphene::db::object),
                    (processed)(signatures) )
