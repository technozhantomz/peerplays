#pragma once
#include <graphene/chain/protocol/base.hpp>

#include <graphene/chain/sidechain_defs.hpp>

namespace graphene { namespace chain {

    struct sidechain_address_add_operation : public base_operation
    {
        struct fee_parameters_type { uint64_t fee = 0; };

        asset fee;
        account_id_type sidechain_address_account;
        graphene::peerplays_sidechain::sidechain_type sidechain;
        string deposit_address;
        string withdraw_address;

        account_id_type fee_payer()const { return sidechain_address_account; }
        share_type      calculate_fee(const fee_parameters_type& k)const { return 0; }
    };

    struct sidechain_address_update_operation : public base_operation
    {
        struct fee_parameters_type { uint64_t fee = 0; };

        asset fee;
        sidechain_address_id_type sidechain_address_id;
        account_id_type sidechain_address_account;
        graphene::peerplays_sidechain::sidechain_type sidechain;
        optional<string> deposit_address;
        optional<string> withdraw_address;

        account_id_type fee_payer()const { return sidechain_address_account; }
        share_type      calculate_fee(const fee_parameters_type& k)const { return 0; }
    };

    struct sidechain_address_delete_operation : public base_operation
    {
        struct fee_parameters_type { uint64_t fee = 0; };

        asset fee;
        sidechain_address_id_type sidechain_address_id;
        account_id_type sidechain_address_account;
        graphene::peerplays_sidechain::sidechain_type sidechain;

        account_id_type fee_payer()const { return sidechain_address_account; }
        share_type      calculate_fee(const fee_parameters_type& k)const { return 0; }
    };

} } // namespace graphene::chain

FC_REFLECT(graphene::chain::sidechain_address_add_operation::fee_parameters_type, (fee) )
FC_REFLECT(graphene::chain::sidechain_address_add_operation, (fee)
        (sidechain_address_account)(sidechain)(deposit_address)(withdraw_address) )

FC_REFLECT(graphene::chain::sidechain_address_update_operation::fee_parameters_type, (fee) )
FC_REFLECT(graphene::chain::sidechain_address_update_operation, (fee)
        (sidechain_address_id)
        (sidechain_address_account)(sidechain)(deposit_address)(withdraw_address) )

FC_REFLECT(graphene::chain::sidechain_address_delete_operation::fee_parameters_type, (fee) )
FC_REFLECT(graphene::chain::sidechain_address_delete_operation, (fee)
        (sidechain_address_id)
        (sidechain_address_account)(sidechain) )
