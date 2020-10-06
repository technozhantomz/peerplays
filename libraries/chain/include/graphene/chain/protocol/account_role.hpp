#pragma once
#include <graphene/chain/protocol/base.hpp>
#include <graphene/chain/protocol/types.hpp>

namespace graphene
{
    namespace chain
    {

        struct account_role_create_operation : public base_operation
        {
            struct fee_parameters_type
            {
                uint64_t fee = GRAPHENE_BLOCKCHAIN_PRECISION;
                uint32_t price_per_kbyte = GRAPHENE_BLOCKCHAIN_PRECISION;
            };
            asset fee;

            account_id_type owner;
            std::string name;
            std::string metadata;
            flat_set<int> allowed_operations;
            flat_set<account_id_type> whitelisted_accounts;
            time_point_sec valid_to;
            extensions_type extensions;

            account_id_type fee_payer() const { return owner; }
            void validate() const;
            share_type calculate_fee(const fee_parameters_type &k) const;
        };

        struct account_role_update_operation : public base_operation
        {
            struct fee_parameters_type
            {
                uint64_t fee = GRAPHENE_BLOCKCHAIN_PRECISION;
                uint32_t price_per_kbyte = GRAPHENE_BLOCKCHAIN_PRECISION;
            };
            asset fee;

            account_id_type owner;
            account_role_id_type account_role_id;
            optional<std::string> name;
            optional<std::string> metadata;
            flat_set<int> allowed_operations_to_add;
            flat_set<int> allowed_operations_to_remove;
            flat_set<account_id_type> accounts_to_add;
            flat_set<account_id_type> accounts_to_remove;
            optional<time_point_sec> valid_to;
            extensions_type extensions;

            account_id_type fee_payer() const { return owner; }
            void validate() const;
            share_type calculate_fee(const fee_parameters_type &k) const;
        };

        struct account_role_delete_operation : public base_operation
        {
            struct fee_parameters_type
            {
                uint64_t fee = GRAPHENE_BLOCKCHAIN_PRECISION;
            };

            asset fee;
            account_id_type owner;
            account_role_id_type account_role_id;
            extensions_type extensions;

            account_id_type fee_payer() const { return owner; }
            void validate() const;
            share_type calculate_fee(const fee_parameters_type &k) const { return k.fee; }
        };
    } // namespace chain
} // namespace graphene

FC_REFLECT(graphene::chain::account_role_create_operation::fee_parameters_type, (fee)(price_per_kbyte))
FC_REFLECT(graphene::chain::account_role_update_operation::fee_parameters_type, (fee)(price_per_kbyte))
FC_REFLECT(graphene::chain::account_role_delete_operation::fee_parameters_type, (fee))

FC_REFLECT(graphene::chain::account_role_create_operation, (fee)(owner)(name)(metadata)(allowed_operations)(whitelisted_accounts)(valid_to)(extensions))
FC_REFLECT(graphene::chain::account_role_update_operation, (fee)(owner)(account_role_id)(name)(metadata)(allowed_operations_to_add)(allowed_operations_to_remove)(accounts_to_add)(accounts_to_remove)(valid_to)(extensions))
FC_REFLECT(graphene::chain::account_role_delete_operation, (fee)(owner)(account_role_id)(owner)(extensions))
