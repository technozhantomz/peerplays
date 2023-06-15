#pragma once
#include <graphene/chain/protocol/types.hpp>
#include <graphene/db/object.hpp>
#include <graphene/db/generic_index.hpp>

namespace graphene { namespace chain {
   using namespace graphene::db;

   class nft_lottery_balance_object : public abstract_object<nft_lottery_balance_object>
   {
      public:
         static const uint8_t space_id = implementation_ids;
         static const uint8_t type_id  = impl_nft_lottery_balance_object_type;

         // Total Progressive jackpot carried over from previous lotteries
         asset total_progressive_jackpot;
         // Current total jackpot in this lottery inclusive of the progressive jackpot
         asset jackpot;
         // Total tickets sold
         share_type sweeps_tickets_sold;
   };

   struct nft_lottery_data
   {
      nft_lottery_data() {}
      nft_lottery_data(const nft_lottery_options &options, nft_lottery_balance_id_type lottery_id)
          : lottery_options(options), lottery_balance_id(lottery_id) {}
      nft_lottery_options lottery_options;
      nft_lottery_balance_id_type lottery_balance_id;
   };

   class nft_metadata_object : public abstract_object<nft_metadata_object>
   {
      public:
         static const uint8_t space_id = protocol_ids;
         static const uint8_t type_id  = nft_metadata_type;

         account_id_type owner;
         std::string     name;
         std::string     symbol;
         std::string     base_uri;
         optional<account_id_type> revenue_partner;
         optional<uint16_t> revenue_split;
         bool            is_transferable = false;
         bool            is_sellable = true;
         optional<account_role_id_type> account_role;
         share_type max_supply = GRAPHENE_MAX_SHARE_SUPPLY;
         optional<nft_lottery_data> lottery_data;

         nft_metadata_id_type get_id() const { return id; }
         bool is_lottery() const { return lottery_data.valid(); }
         uint32_t get_owner_num() const { return owner.instance.value; }
         time_point_sec get_lottery_expiration() const;
         asset get_lottery_jackpot(const database &db) const;
         share_type get_token_current_supply(const database &db) const;
         vector<account_id_type> get_holders(const database &db) const;
         vector<uint64_t> get_ticket_ids(const database &db) const;
         void distribute_benefactors_part(database &db);
         map<account_id_type, vector<uint16_t>> distribute_winners_part(database &db);
         void distribute_sweeps_holders_part(database &db);
         void end_lottery(database &db);
   };

   class nft_object : public abstract_object<nft_object>
   {
      public:
         static const uint8_t space_id = protocol_ids;
         static const uint8_t type_id  = nft_object_type;

         nft_metadata_id_type    nft_metadata_id;
         account_id_type         owner;
         account_id_type         approved;
         vector<account_id_type> approved_operators;
         std::string             token_uri;
   };

   struct nft_lottery_comparer
   {
      bool operator()(const nft_metadata_object& lhs, const nft_metadata_object& rhs) const
      {
         if ( !lhs.is_lottery() ) return false;
         if ( !lhs.lottery_data->lottery_options.is_active && !rhs.is_lottery()) return true; // not active lotteries first, just assets then
         if ( !lhs.lottery_data->lottery_options.is_active ) return false;
         if ( lhs.lottery_data->lottery_options.is_active && ( !rhs.is_lottery() || !rhs.lottery_data->lottery_options.is_active ) ) return true;
         return lhs.get_lottery_expiration() > rhs.get_lottery_expiration();
      }
   };

   struct by_name;
   struct by_symbol;
   struct active_nft_lotteries;
   struct by_nft_lottery;
   struct by_nft_lottery_owner;
   using nft_metadata_multi_index_type = multi_index_container<
      nft_metadata_object,
      indexed_by<
         ordered_unique< tag<by_id>,
            member<object, object_id_type, &object::id>
         >,
         ordered_unique< tag<by_name>,
            member<nft_metadata_object, std::string, &nft_metadata_object::name>
         >,
         ordered_unique< tag<by_symbol>,
            member<nft_metadata_object, std::string, &nft_metadata_object::symbol>
         >,
         ordered_non_unique< tag<active_nft_lotteries>,
            identity< nft_metadata_object >,
            nft_lottery_comparer
         >,
         ordered_unique< tag<by_nft_lottery>,
            composite_key<
               nft_metadata_object,
               const_mem_fun<nft_metadata_object, bool, &nft_metadata_object::is_lottery>,
               member<object, object_id_type, &object::id>
            >,
            composite_key_compare<
               std::greater< bool >,
               std::greater< object_id_type >
            >
         >,
         ordered_unique< tag<by_nft_lottery_owner>,
            composite_key<
               nft_metadata_object,
               const_mem_fun<nft_metadata_object, bool, &nft_metadata_object::is_lottery>,
               const_mem_fun<nft_metadata_object, uint32_t, &nft_metadata_object::get_owner_num>,
               member<object, object_id_type, &object::id>
            >,
            composite_key_compare<
               std::greater< bool >,
               std::greater< uint32_t >,
               std::greater< object_id_type >
            >
         >
      >
   >;
   using nft_metadata_index = generic_index<nft_metadata_object, nft_metadata_multi_index_type>;

   struct by_metadata;
   struct by_metadata_and_owner;
   struct by_owner;
   struct by_owner_and_id;
   using nft_multi_index_type = multi_index_container<
      nft_object,
      indexed_by<
         ordered_unique< tag<by_id>,
            member<object, object_id_type, &object::id>
         >,
         ordered_non_unique< tag<by_metadata>,
            member<nft_object, nft_metadata_id_type, &nft_object::nft_metadata_id>
         >,
         ordered_non_unique< tag<by_metadata_and_owner>,
            composite_key<nft_object,
               member<nft_object, nft_metadata_id_type, &nft_object::nft_metadata_id>,
               member<nft_object, account_id_type, &nft_object::owner>
            >
         >,
         ordered_non_unique< tag<by_owner>,
            member<nft_object, account_id_type, &nft_object::owner>
         >,
         ordered_unique< tag<by_owner_and_id>,
            composite_key<nft_object,
               member<nft_object, account_id_type, &nft_object::owner>,
               member<object, object_id_type, &object::id>
            >
         >
      >
   >;
   using nft_index = generic_index<nft_object, nft_multi_index_type>;

   using nft_lottery_balance_index_type = multi_index_container<
      nft_lottery_balance_object,
      indexed_by<
         ordered_unique< tag<by_id>, member< object, object_id_type, &object::id > >
      >
   >;
   using nft_lottery_balance_index = generic_index<nft_lottery_balance_object, nft_lottery_balance_index_type>;

} } // graphene::chain

FC_REFLECT_DERIVED( graphene::chain::nft_lottery_balance_object, (graphene::db::object),
                    (total_progressive_jackpot)
                    (jackpot)
                    (sweeps_tickets_sold) )

FC_REFLECT( graphene::chain::nft_lottery_data, (lottery_options)(lottery_balance_id) )

FC_REFLECT_DERIVED( graphene::chain::nft_metadata_object, (graphene::db::object),
                    (owner)
                    (name)
                    (symbol)
                    (base_uri)
                    (revenue_partner)
                    (revenue_split)
                    (is_transferable)
                    (is_sellable)
                    (account_role)
                    (max_supply)
                    (lottery_data) )

FC_REFLECT_DERIVED( graphene::chain::nft_object, (graphene::db::object),
                    (nft_metadata_id)
                    (owner)
                    (approved)
                    (approved_operators)
                    (token_uri) )

