#pragma once
#include <graphene/chain/protocol/types.hpp>
#include <graphene/db/object.hpp>
#include <graphene/db/generic_index.hpp>
#include <graphene/chain/sidechain_defs.hpp>

namespace graphene { namespace chain {
   using namespace graphene::db;

   enum class son_status
   {
      inactive,
      active,
      request_maintenance,
      in_maintenance,
      deregistered
   };
   /**
    * @class son_statistics_object
    * @ingroup object
    * @ingroup implementation
    *
    * This object contains regularly updated statistical data about an SON. It is provided for the purpose of
    * separating the SON transaction data that changes frequently from the SON object data that is mostly static.
    */
   class son_statistics_object : public graphene::db::abstract_object<son_statistics_object>
   {
      public:
         static const uint8_t space_id = implementation_ids;
         static const uint8_t type_id  = impl_son_statistics_object_type;

         son_id_type  owner;
         // Lifetime total transactions signed
         flat_map<sidechain_type, uint64_t> total_txs_signed;
         // Transactions signed since the last son payouts
         flat_map<sidechain_type, uint64_t> txs_signed;
         // Total Voted Active time i.e. duration selected as part of voted active SONs
         uint64_t total_voted_time = 0;
         // Total Downtime barring the current down time in seconds, used for stats to present to user
         uint64_t total_downtime = 0;
         // Current Interval Downtime since last maintenance
         uint64_t current_interval_downtime = 0;
         // Down timestamp, if son status is in_maintenance use this
         fc::time_point_sec last_down_timestamp;
         // Last Active heartbeat timestamp
         fc::time_point_sec last_active_timestamp;
         // Deregistered Timestamp
         fc::time_point_sec deregistered_timestamp;
         // Total sidechain transactions reported by SON network while SON was active
         flat_map<sidechain_type, uint64_t> total_sidechain_txs_reported;
         // Sidechain transactions reported by this SON
         flat_map<sidechain_type, uint64_t> sidechain_txs_reported;
   };

   /**
    * @class son_object
    * @brief tracks information about a SON account.
    * @ingroup object
    */
   class son_object : public abstract_object<son_object>
   {
      public:
         static const uint8_t space_id = protocol_ids;
         static const uint8_t type_id  = son_object_type;

         account_id_type son_account;
         vote_id_type vote_id;
         uint64_t total_votes = 0;
         string url;
         vesting_balance_id_type deposit;
         public_key_type signing_key;
         vesting_balance_id_type pay_vb;
         son_statistics_id_type statistics;
         son_status status = son_status::inactive;
         flat_map<sidechain_type, string> sidechain_public_keys;

         void pay_son_fee(share_type pay, database& db);
         bool has_valid_config()const;
         bool has_valid_config(time_point_sec head_block_time)const;
   };

   struct by_account;
   struct by_vote_id;
   using son_multi_index_type = multi_index_container<
      son_object,
      indexed_by<
         ordered_unique< tag<by_id>,
            member<object, object_id_type, &object::id>
         >,
         ordered_unique< tag<by_account>,
            member<son_object, account_id_type, &son_object::son_account>
         >,
         ordered_unique< tag<by_vote_id>,
            member<son_object, vote_id_type, &son_object::vote_id>
         >
      >
   >;
   using son_index = generic_index<son_object, son_multi_index_type>;

   struct by_owner;
   using son_stats_multi_index_type = multi_index_container<
      son_statistics_object,
      indexed_by<
         ordered_unique< tag<by_id>,
            member<object, object_id_type, &object::id>
         >,
         ordered_unique< tag<by_owner>,
            member<son_statistics_object, son_id_type, &son_statistics_object::owner>
         >
      >
   >;
   using son_stats_index = generic_index<son_statistics_object, son_stats_multi_index_type>;

} } // graphene::chain

FC_REFLECT_ENUM(graphene::chain::son_status, (inactive)(active)(request_maintenance)(in_maintenance)(deregistered) )

FC_REFLECT_DERIVED( graphene::chain::son_object, (graphene::db::object),
                    (son_account)
                    (vote_id)
                    (total_votes)
                    (url)
                    (deposit)
                    (signing_key)
                    (pay_vb)
                    (statistics)
                    (status)
                    (sidechain_public_keys)
                  )

FC_REFLECT_DERIVED( graphene::chain::son_statistics_object,
                    (graphene::db::object),
                    (owner)
                    (total_txs_signed)
                    (txs_signed)
                    (total_voted_time)
                    (total_downtime)
                    (current_interval_downtime)
                    (last_down_timestamp)
                    (last_active_timestamp)
                    (deregistered_timestamp)
                    (total_sidechain_txs_reported)
                    (sidechain_txs_reported)
                  )

GRAPHENE_EXTERNAL_SERIALIZATION( extern, graphene::chain::son_object )
GRAPHENE_EXTERNAL_SERIALIZATION( extern, graphene::chain::son_statistics_object )
