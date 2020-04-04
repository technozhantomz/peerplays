#pragma once
#include <graphene/chain/protocol/asset.hpp>
#include <graphene/chain/protocol/types.hpp>
#include <graphene/chain/sidechain_defs.hpp>

namespace graphene { namespace chain {
   using namespace graphene::db;

   /**
    * @class son_wallet_withdraw_object
    * @brief tracks information about a SON wallet withdrawal.
    * @ingroup object
    */
   class son_wallet_withdraw_object : public abstract_object<son_wallet_withdraw_object>
   {
      public:
         static const uint8_t space_id = protocol_ids;
         static const uint8_t type_id  = son_wallet_withdraw_object_type;

         time_point_sec timestamp;
         uint32_t block_num;
         sidechain_type sidechain = sidechain_type::unknown;
         std::string peerplays_uid;
         std::string peerplays_transaction_id;
         chain::account_id_type peerplays_from;
         chain::asset peerplays_asset;
         sidechain_type withdraw_sidechain;
         std::string withdraw_address;
         std::string withdraw_currency;
         safe<int64_t> withdraw_amount;

         std::map<son_id_type, weight_type> expected_reports;
         std::set<son_id_type> received_reports;

         bool confirmed = false;

         bool processed = false;
   };

   struct by_peerplays_uid;
   struct by_withdraw_sidechain_and_confirmed_and_processed;
   using son_wallet_withdraw_multi_index_type = multi_index_container<
      son_wallet_withdraw_object,
      indexed_by<
         ordered_unique< tag<by_id>,
            member<object, object_id_type, &object::id>
         >,
         ordered_unique< tag<by_peerplays_uid>,
            member<son_wallet_withdraw_object, std::string, &son_wallet_withdraw_object::peerplays_uid>
         >,
         ordered_non_unique< tag<by_withdraw_sidechain_and_confirmed_and_processed>,
            composite_key<son_wallet_withdraw_object,
               member<son_wallet_withdraw_object, sidechain_type, &son_wallet_withdraw_object::withdraw_sidechain>,
               member<son_wallet_withdraw_object, bool, &son_wallet_withdraw_object::confirmed>,
               member<son_wallet_withdraw_object, bool, &son_wallet_withdraw_object::processed>
            >
         >
      >
   >;
   using son_wallet_withdraw_index = generic_index<son_wallet_withdraw_object, son_wallet_withdraw_multi_index_type>;
} } // graphene::chain

FC_REFLECT_DERIVED( graphene::chain::son_wallet_withdraw_object, (graphene::db::object),
                    (timestamp) (block_num) (sidechain)
                    (peerplays_uid) (peerplays_transaction_id) (peerplays_from) (peerplays_asset)
                    (withdraw_sidechain) (withdraw_address) (withdraw_currency) (withdraw_amount)
                    (expected_reports) (received_reports)
                    (confirmed) (processed) )
