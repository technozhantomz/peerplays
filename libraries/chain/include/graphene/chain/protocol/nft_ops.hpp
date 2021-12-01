#pragma once
#include <graphene/chain/protocol/base.hpp>
#include <graphene/chain/protocol/types.hpp>

namespace graphene { namespace chain {

   struct nft_lottery_benefactor   {
      account_id_type id;
      uint16_t share; // percent * GRAPHENE_1_PERCENT
      nft_lottery_benefactor() = default;
      nft_lottery_benefactor( const nft_lottery_benefactor & ) = default;
      nft_lottery_benefactor( account_id_type _id, uint16_t _share ) : id( _id ), share( _share ) {}
   };

   struct nft_lottery_options
   {
      std::vector<nft_lottery_benefactor> benefactors;
      // specifying winning tickets as shares that will be issued
      std::vector<uint16_t>   winning_tickets;
      asset                   ticket_price;
      time_point_sec          end_date;
      bool                    ending_on_soldout;
      bool                    is_active;
      bool                    delete_tickets_after_draw = false;
      std::vector<nft_metadata_id_type> progressive_jackpots;

      void validate() const;
   };

   struct nft_metadata_create_operation : public base_operation
   {
      struct fee_parameters_type
      {
         uint64_t fee = GRAPHENE_BLOCKCHAIN_PRECISION;
         uint32_t price_per_kbyte = GRAPHENE_BLOCKCHAIN_PRECISION;
      };
      asset fee;

      account_id_type owner;
      std::string name;
      std::string symbol;
      std::string base_uri;
      optional<account_id_type> revenue_partner;
      optional<uint16_t> revenue_split;
      bool is_transferable = false;
      bool is_sellable = true;
      // Accounts Role
      optional<account_role_id_type> account_role;
      // Max number of NFTs that can be minted from the metadata
      optional<share_type> max_supply;
      // Lottery configuration
      optional<nft_lottery_options> lottery_options;
      extensions_type    extensions;

      account_id_type fee_payer()const { return owner; }
      void validate() const;
      share_type calculate_fee(const fee_parameters_type &k) const;
   };

   struct nft_metadata_update_operation : public base_operation
   {
      struct fee_parameters_type
      {
         uint64_t fee = GRAPHENE_BLOCKCHAIN_PRECISION;
         uint32_t price_per_kbyte = GRAPHENE_BLOCKCHAIN_PRECISION;
      };
      asset fee;

      account_id_type owner;
      nft_metadata_id_type nft_metadata_id;
      optional<std::string> name;
      optional<std::string> symbol;
      optional<std::string> base_uri;
      optional<account_id_type> revenue_partner;
      optional<uint16_t> revenue_split;
      optional<bool> is_transferable;
      optional<bool> is_sellable;
      // Accounts Role
      optional<account_role_id_type> account_role;
      extensions_type    extensions;

      account_id_type fee_payer()const { return owner; }
      void validate() const;
      share_type calculate_fee(const fee_parameters_type &k) const;
   };

   struct nft_mint_operation : public base_operation
   {
      struct fee_parameters_type
      {
         uint64_t fee = GRAPHENE_BLOCKCHAIN_PRECISION;
         uint32_t price_per_kbyte = GRAPHENE_BLOCKCHAIN_PRECISION;
      };
      asset fee;

      account_id_type payer;

      nft_metadata_id_type nft_metadata_id;
      account_id_type owner;
      account_id_type approved;
      vector<account_id_type> approved_operators;
      std::string token_uri;
      extensions_type    extensions;

      account_id_type fee_payer()const { return payer; }
      void validate() const;
      share_type calculate_fee(const fee_parameters_type &k) const;
   };

   struct nft_safe_transfer_from_operation : public base_operation
   {
      struct fee_parameters_type
      {
         uint64_t fee = GRAPHENE_BLOCKCHAIN_PRECISION;
         uint32_t price_per_kbyte = GRAPHENE_BLOCKCHAIN_PRECISION;
      };
      asset fee;

      account_id_type operator_;

      account_id_type from;
      account_id_type to;
      nft_id_type token_id;
      string data;
      extensions_type    extensions;

      account_id_type fee_payer()const { return operator_; }
      share_type calculate_fee(const fee_parameters_type &k) const;
   };

   struct nft_approve_operation : public base_operation
   {
      struct fee_parameters_type { uint64_t fee = GRAPHENE_BLOCKCHAIN_PRECISION; };
      asset fee;

      account_id_type operator_;

      account_id_type approved;
      nft_id_type token_id;
      extensions_type    extensions;

      account_id_type fee_payer()const { return operator_; }
      share_type calculate_fee(const fee_parameters_type &k) const;
   };

   struct nft_set_approval_for_all_operation : public base_operation
   {
      struct fee_parameters_type { uint64_t fee = GRAPHENE_BLOCKCHAIN_PRECISION; };
      asset fee;

      account_id_type owner;

      account_id_type operator_;
      bool approved;
      extensions_type    extensions;

      account_id_type fee_payer()const { return owner; }
      share_type calculate_fee(const fee_parameters_type &k) const;
   };

} } // graphene::chain

FC_REFLECT( graphene::chain::nft_lottery_benefactor, (id)(share) )
FC_REFLECT( graphene::chain::nft_lottery_options, (benefactors)(winning_tickets)(ticket_price)(end_date)(ending_on_soldout)(is_active)(delete_tickets_after_draw)(progressive_jackpots) )

FC_REFLECT( graphene::chain::nft_metadata_create_operation::fee_parameters_type, (fee) (price_per_kbyte) )
FC_REFLECT( graphene::chain::nft_metadata_update_operation::fee_parameters_type, (fee) )
FC_REFLECT( graphene::chain::nft_mint_operation::fee_parameters_type, (fee) (price_per_kbyte) )
FC_REFLECT( graphene::chain::nft_safe_transfer_from_operation::fee_parameters_type, (fee) (price_per_kbyte) )
FC_REFLECT( graphene::chain::nft_approve_operation::fee_parameters_type, (fee) )
FC_REFLECT( graphene::chain::nft_set_approval_for_all_operation::fee_parameters_type, (fee) )

FC_REFLECT( graphene::chain::nft_metadata_create_operation, (fee) (owner) (name) (symbol) (base_uri) (revenue_partner) (revenue_split) (is_transferable) (is_sellable) (account_role) (max_supply) (lottery_options) (extensions) )
FC_REFLECT( graphene::chain::nft_metadata_update_operation, (fee) (owner) (nft_metadata_id) (name) (symbol) (base_uri) (revenue_partner) (revenue_split) (is_transferable) (is_sellable) (account_role) (extensions) )
FC_REFLECT( graphene::chain::nft_mint_operation, (fee) (payer) (nft_metadata_id) (owner) (approved) (approved_operators) (token_uri) (extensions) )
FC_REFLECT( graphene::chain::nft_safe_transfer_from_operation, (fee) (operator_) (from) (to) (token_id) (data) (extensions) )
FC_REFLECT( graphene::chain::nft_approve_operation, (fee) (operator_) (approved) (token_id) (extensions) )
FC_REFLECT( graphene::chain::nft_set_approval_for_all_operation, (fee) (owner) (operator_) (approved) (extensions) )
