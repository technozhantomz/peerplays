#pragma once
#include <graphene/chain/protocol/asset.hpp>
#include <graphene/chain/protocol/types.hpp>
#include <graphene/chain/sidechain_defs.hpp>

namespace graphene { namespace chain {
   using namespace graphene::db;

   /**
    * @class son_wallet_deposit_object
    * @brief tracks information about a SON wallet deposit.
    * @ingroup object
    */
   class son_wallet_deposit_object : public abstract_object<son_wallet_deposit_object>
   {
      public:
         static const uint8_t space_id = protocol_ids;
         static const uint8_t type_id  = son_wallet_deposit_object_type;

         time_point_sec timestamp;
         sidechain_type sidechain;
         std::string sidechain_uid;
         std::string sidechain_transaction_id;
         std::string sidechain_from;
         std::string sidechain_to;
         std::string sidechain_currency;
         safe<int64_t> sidechain_amount;
         chain::account_id_type peerplays_from;
         chain::account_id_type peerplays_to;
         chain::asset peerplays_asset;

         std::set<son_id_type> expected_reports;
         std::set<son_id_type> received_reports;

         bool processed;
   };

   struct by_sidechain;
   struct by_sidechain_uid;
   struct by_processed;
   struct by_sidechain_and_processed;
   using son_wallet_deposit_multi_index_type = multi_index_container<
      son_wallet_deposit_object,
      indexed_by<
         ordered_unique< tag<by_id>,
            member<object, object_id_type, &object::id>
         >,
         ordered_non_unique< tag<by_sidechain>,
            member<son_wallet_deposit_object, sidechain_type, &son_wallet_deposit_object::sidechain>
         >,
         ordered_unique< tag<by_sidechain_uid>,
            member<son_wallet_deposit_object, std::string, &son_wallet_deposit_object::sidechain_uid>
         >,
         ordered_non_unique< tag<by_processed>,
            member<son_wallet_deposit_object, bool, &son_wallet_deposit_object::processed>
         >,
         ordered_non_unique< tag<by_sidechain_and_processed>,
            composite_key<son_wallet_deposit_object,
               member<son_wallet_deposit_object, sidechain_type, &son_wallet_deposit_object::sidechain>,
               member<son_wallet_deposit_object, bool, &son_wallet_deposit_object::processed>
            >
         >
      >
   >;
   using son_wallet_deposit_index = generic_index<son_wallet_deposit_object, son_wallet_deposit_multi_index_type>;
} } // graphene::chain

FC_REFLECT_DERIVED( graphene::chain::son_wallet_deposit_object, (graphene::db::object),
                    (timestamp) (sidechain)
                    (sidechain_uid) (sidechain_transaction_id) (sidechain_from) (sidechain_to) (sidechain_currency) (sidechain_amount)
                    (peerplays_from) (peerplays_to) (peerplays_asset)
                    (expected_reports) (received_reports)
                    (processed) )
