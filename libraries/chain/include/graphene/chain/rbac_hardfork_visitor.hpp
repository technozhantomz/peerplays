#pragma once
#include <graphene/chain/protocol/operations.hpp>
#include <graphene/chain/hardfork.hpp>

namespace graphene
{
    namespace chain
    {
        struct rbac_operation_hardfork_visitor
        {
            typedef void result_type;
            const fc::time_point_sec block_time;

            rbac_operation_hardfork_visitor(const fc::time_point_sec bt) : block_time(bt) {}
            void operator()(int op_type) const
            {
                int first_allowed_op = operation::tag<custom_permission_create_operation>::value;
                switch (op_type)
                {
                case operation::tag<son_create_operation>::value:
                case operation::tag<son_update_operation>::value:
                case operation::tag<son_deregister_operation>::value:
                case operation::tag<son_heartbeat_operation>::value:
                case operation::tag<son_report_down_operation>::value:
                case operation::tag<son_maintenance_operation>::value:
                case operation::tag<son_wallet_recreate_operation>::value:
                case operation::tag<son_wallet_update_operation>::value:
                case operation::tag<son_wallet_deposit_create_operation>::value:
                case operation::tag<son_wallet_deposit_process_operation>::value:
                case operation::tag<son_wallet_withdraw_create_operation>::value:
                case operation::tag<son_wallet_withdraw_process_operation>::value:
                case operation::tag<sidechain_address_add_operation>::value:
                case operation::tag<sidechain_address_update_operation>::value:
                case operation::tag<sidechain_address_delete_operation>::value:
                case operation::tag<sidechain_transaction_create_operation>::value:
                case operation::tag<sidechain_transaction_sign_operation>::value:
                case operation::tag<sidechain_transaction_send_operation>::value:
                case operation::tag<sidechain_transaction_settle_operation>::value:
                    FC_ASSERT(block_time >= HARDFORK_SON_TIME, "Custom permissions and roles not allowed on this operation yet!");
                    break;
                case operation::tag<custom_permission_create_operation>::value:
                case operation::tag<custom_permission_update_operation>::value:
                case operation::tag<custom_permission_delete_operation>::value:
                case operation::tag<custom_account_authority_create_operation>::value:
                case operation::tag<custom_account_authority_update_operation>::value:
                case operation::tag<custom_account_authority_delete_operation>::value:
                case operation::tag<offer_operation>::value:
                case operation::tag<bid_operation>::value:
                case operation::tag<cancel_offer_operation>::value:
                case operation::tag<finalize_offer_operation>::value:
                case operation::tag<nft_metadata_create_operation>::value:
                case operation::tag<nft_metadata_update_operation>::value:
                case operation::tag<nft_mint_operation>::value:
                case operation::tag<nft_safe_transfer_from_operation>::value:
                case operation::tag<nft_approve_operation>::value:
                case operation::tag<nft_set_approval_for_all_operation>::value:
                case operation::tag<account_role_create_operation>::value:
                case operation::tag<account_role_update_operation>::value:
                case operation::tag<account_role_delete_operation>::value:
                case operation::tag<nft_lottery_token_purchase_operation>::value:
                case operation::tag<nft_lottery_reward_operation>::value:
                case operation::tag<nft_lottery_end_operation>::value:
                    FC_ASSERT(block_time >= HARDFORK_NFT_TIME, "Custom permissions and roles not allowed on this operation yet!");
                    break;
                default:
                    FC_ASSERT(op_type >= operation::tag<transfer_operation>::value && op_type < first_allowed_op, "Custom permissions and roles not allowed on this operation!");
                }
            }
        };

    } // namespace chain
} // namespace graphene
