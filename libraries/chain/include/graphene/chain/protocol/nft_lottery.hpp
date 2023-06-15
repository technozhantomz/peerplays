#pragma once
#include <graphene/chain/protocol/base.hpp>
#include <graphene/chain/protocol/types.hpp>

namespace graphene
{
    namespace chain
    {
        struct nft_lottery_token_purchase_operation : public base_operation
        {
            struct fee_parameters_type
            {
                uint64_t fee = GRAPHENE_BLOCKCHAIN_PRECISION;
            };
            asset fee;
            // Lottery NFT Metadata
            nft_metadata_id_type lottery_id;
            // Buyer purchasing lottery tickets
            account_id_type buyer;
            // count of tickets to buy
            uint64_t tickets_to_buy;
            // amount that can spent
            asset amount;

            extensions_type extensions;

            account_id_type fee_payer() const { return buyer; }
            void validate() const;
            share_type calculate_fee(const fee_parameters_type &k) const;
        };

        struct nft_lottery_reward_operation : public base_operation
        {
            struct fee_parameters_type
            {
                uint64_t fee = GRAPHENE_BLOCKCHAIN_PRECISION;
            };

            asset fee;
            // Lottery NFT Metadata
            nft_metadata_id_type lottery_id;
            // winner account
            account_id_type winner;
            // amount that won
            asset amount;
            // percentage of jackpot that user won
            uint16_t win_percentage;
            // true if recieved from benefators section of lottery; false otherwise
            bool is_benefactor_reward;

            uint64_t winner_ticket_id;

            extensions_type extensions;

            account_id_type fee_payer() const { return account_id_type(); }
            void validate() const {};
            share_type calculate_fee(const fee_parameters_type &k) const { return k.fee; };
        };

        struct nft_lottery_end_operation : public base_operation
        {
            struct fee_parameters_type
            {
                uint64_t fee = GRAPHENE_BLOCKCHAIN_PRECISION;
            };

            asset fee;
            // Lottery NFT Metadata
            nft_metadata_id_type lottery_id;

            extensions_type extensions;

            account_id_type fee_payer() const { return account_id_type(); }
            void validate() const {}
            share_type calculate_fee(const fee_parameters_type &k) const { return k.fee; }
        };

    } // namespace chain
} // namespace graphene

FC_REFLECT(graphene::chain::nft_lottery_token_purchase_operation::fee_parameters_type, (fee))
FC_REFLECT(graphene::chain::nft_lottery_reward_operation::fee_parameters_type, (fee))
FC_REFLECT(graphene::chain::nft_lottery_end_operation::fee_parameters_type, (fee))
FC_REFLECT(graphene::chain::nft_lottery_token_purchase_operation, (fee)(lottery_id)(buyer)(tickets_to_buy)(amount)(extensions))
FC_REFLECT(graphene::chain::nft_lottery_reward_operation, (fee)(lottery_id)(winner)(amount)(win_percentage)(is_benefactor_reward)(winner_ticket_id)(extensions))
FC_REFLECT(graphene::chain::nft_lottery_end_operation, (fee)(lottery_id)(extensions))