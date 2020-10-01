#pragma once
#include <graphene/chain/protocol/base.hpp>
#include <graphene/chain/sidechain_defs.hpp>

#include <fc/safe.hpp>

namespace graphene { namespace chain {

    struct son_wallet_deposit_create_operation : public base_operation
    {
        struct fee_parameters_type { uint64_t fee = 0; };

        asset fee;
        account_id_type payer;

        son_id_type son_id;
        fc::time_point_sec timestamp;
        uint32_t block_num;
        sidechain_type sidechain;
        std::string sidechain_uid;
        std::string sidechain_transaction_id;
        std::string sidechain_from;
        std::string sidechain_to;
        std::string sidechain_currency;
        fc::safe<int64_t> sidechain_amount;
        chain::account_id_type peerplays_from;
        chain::account_id_type peerplays_to;
        chain::asset peerplays_asset;

        account_id_type fee_payer()const { return payer; }
        share_type      calculate_fee(const fee_parameters_type& k)const { return 0; }
    };

    struct son_wallet_deposit_process_operation : public base_operation
    {
        struct fee_parameters_type { uint64_t fee = 0; };

        asset fee;
        account_id_type payer;

        son_wallet_deposit_id_type son_wallet_deposit_id;

        account_id_type fee_payer()const { return payer; }
        share_type      calculate_fee(const fee_parameters_type& k)const { return 0; }
    };

} } // namespace graphene::chain

FC_REFLECT(graphene::chain::son_wallet_deposit_create_operation::fee_parameters_type, (fee) )
FC_REFLECT(graphene::chain::son_wallet_deposit_create_operation, (fee)(payer)
      (son_id) (timestamp) (block_num) (sidechain)
      (sidechain_uid) (sidechain_transaction_id) (sidechain_from) (sidechain_to) (sidechain_currency) (sidechain_amount)
      (peerplays_from) (peerplays_to) (peerplays_asset) )

FC_REFLECT(graphene::chain::son_wallet_deposit_process_operation::fee_parameters_type, (fee) )
FC_REFLECT(graphene::chain::son_wallet_deposit_process_operation, (fee)(payer)
      (son_wallet_deposit_id) )
