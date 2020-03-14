#pragma once
#include <graphene/chain/protocol/base.hpp>

#include <fc/safe.hpp>

namespace graphene { namespace chain {

    struct son_wallet_withdraw_create_operation : public base_operation
    {
        struct fee_parameters_type { uint64_t fee = 0; };

        asset fee;
        account_id_type payer;

        son_id_type son_id;
        fc::time_point_sec timestamp;
        peerplays_sidechain::sidechain_type sidechain;
        std::string peerplays_uid;
        std::string peerplays_transaction_id;
        chain::account_id_type peerplays_from;
        chain::asset peerplays_asset;
        peerplays_sidechain::sidechain_type withdraw_sidechain;
        std::string withdraw_address;
        std::string withdraw_currency;
        safe<int64_t> withdraw_amount;

        account_id_type fee_payer()const { return payer; }
        share_type      calculate_fee(const fee_parameters_type& k)const { return 0; }
    };

    struct son_wallet_withdraw_process_operation : public base_operation
    {
        struct fee_parameters_type { uint64_t fee = 0; };

        asset fee;
        account_id_type payer;

        son_wallet_withdraw_id_type son_wallet_withdraw_id;

        account_id_type fee_payer()const { return payer; }
        share_type      calculate_fee(const fee_parameters_type& k)const { return 0; }
    };

} } // namespace graphene::chain

FC_REFLECT(graphene::chain::son_wallet_withdraw_create_operation::fee_parameters_type, (fee) )
FC_REFLECT(graphene::chain::son_wallet_withdraw_create_operation, (fee)(payer)
      (son_id) (timestamp) (sidechain) (peerplays_uid) (peerplays_transaction_id) (peerplays_from) (peerplays_asset) (withdraw_sidechain) (withdraw_address) (withdraw_currency) (withdraw_amount) )
FC_REFLECT(graphene::chain::son_wallet_withdraw_process_operation::fee_parameters_type, (fee) )
FC_REFLECT(graphene::chain::son_wallet_withdraw_process_operation, (fee)(payer)
      (son_wallet_withdraw_id))
