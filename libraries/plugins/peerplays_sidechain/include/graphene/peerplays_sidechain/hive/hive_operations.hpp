#pragma once

#include <cstdint>
#include <vector>

#include <fc/optional.hpp>

#include <graphene/chain/protocol/types.hpp>
#include <graphene/peerplays_sidechain/hive/asset.hpp>
#include <graphene/peerplays_sidechain/hive/authority.hpp>
#include <graphene/peerplays_sidechain/hive/types.hpp>

namespace graphene { namespace peerplays_sidechain { namespace hive {

struct vote_operation {};
struct comment_operation {};

struct transfer_operation {
   hive::account_name_type from;
   hive::account_name_type to;
   hive::asset amount;
   std::string memo;
};

struct transfer_to_vesting_operation {};
struct withdraw_vesting_operation {};
struct limit_order_create_operation {};
struct limit_order_cancel_operation {};
struct feed_publish_operation {};
struct convert_operation {};
struct account_create_operation {};

struct account_update_operation {
   hive::account_name_type account;
   fc::optional<authority> owner;
   fc::optional<authority> active;
   fc::optional<authority> posting;
   hive::public_key_type memo_key;
   std::string json_metadata;
};

struct witness_update_operation {};
struct account_witness_vote_operation {};
struct account_witness_proxy_operation {};
struct pow_operation {};
struct custom_operation {};
struct report_over_production_operation {};
struct delete_comment_operation {};
struct custom_json_operation {};
struct comment_options_operation {};
struct set_withdraw_vesting_route_operation {};
struct limit_order_create2_operation {};
struct claim_account_operation {};
struct create_claimed_account_operation {};
struct request_account_recovery_operation {};
struct recover_account_operation {};
struct change_recovery_account_operation {};
struct escrow_transfer_operation {};
struct escrow_dispute_operation {};
struct escrow_release_operation {};
struct pow2_operation {};
struct escrow_approve_operation {};
struct transfer_to_savings_operation {};
struct transfer_from_savings_operation {};
struct cancel_transfer_from_savings_operation {};
struct custom_binary_operation {};
struct decline_voting_rights_operation {};
struct reset_account_operation {};
struct set_reset_account_operation {};
struct claim_reward_balance_operation {};

struct delegate_vesting_shares_operation {
   hive::account_name_type delegator;
   hive::account_name_type delegatee;
   hive::asset vesting_shares;
};

}}} // namespace graphene::peerplays_sidechain::hive

FC_REFLECT(graphene::peerplays_sidechain::hive::vote_operation, )
FC_REFLECT(graphene::peerplays_sidechain::hive::comment_operation, )
FC_REFLECT(graphene::peerplays_sidechain::hive::transfer_operation,
           (from)(to)(amount)(memo))
FC_REFLECT(graphene::peerplays_sidechain::hive::transfer_to_vesting_operation, )
FC_REFLECT(graphene::peerplays_sidechain::hive::withdraw_vesting_operation, )
FC_REFLECT(graphene::peerplays_sidechain::hive::limit_order_create_operation, )
FC_REFLECT(graphene::peerplays_sidechain::hive::limit_order_cancel_operation, )
FC_REFLECT(graphene::peerplays_sidechain::hive::feed_publish_operation, )
FC_REFLECT(graphene::peerplays_sidechain::hive::convert_operation, )
FC_REFLECT(graphene::peerplays_sidechain::hive::account_create_operation, )
FC_REFLECT(graphene::peerplays_sidechain::hive::account_update_operation,
           (account)(owner)(active)(posting)(memo_key)(json_metadata))
FC_REFLECT(graphene::peerplays_sidechain::hive::witness_update_operation, )
FC_REFLECT(graphene::peerplays_sidechain::hive::account_witness_vote_operation, )
FC_REFLECT(graphene::peerplays_sidechain::hive::account_witness_proxy_operation, )
FC_REFLECT(graphene::peerplays_sidechain::hive::pow_operation, )
FC_REFLECT(graphene::peerplays_sidechain::hive::custom_operation, )
FC_REFLECT(graphene::peerplays_sidechain::hive::report_over_production_operation, )
FC_REFLECT(graphene::peerplays_sidechain::hive::delete_comment_operation, )
FC_REFLECT(graphene::peerplays_sidechain::hive::custom_json_operation, )
FC_REFLECT(graphene::peerplays_sidechain::hive::comment_options_operation, )
FC_REFLECT(graphene::peerplays_sidechain::hive::set_withdraw_vesting_route_operation, )
FC_REFLECT(graphene::peerplays_sidechain::hive::limit_order_create2_operation, )
FC_REFLECT(graphene::peerplays_sidechain::hive::claim_account_operation, )
FC_REFLECT(graphene::peerplays_sidechain::hive::create_claimed_account_operation, )
FC_REFLECT(graphene::peerplays_sidechain::hive::request_account_recovery_operation, )
FC_REFLECT(graphene::peerplays_sidechain::hive::recover_account_operation, )
FC_REFLECT(graphene::peerplays_sidechain::hive::change_recovery_account_operation, )
FC_REFLECT(graphene::peerplays_sidechain::hive::escrow_transfer_operation, )
FC_REFLECT(graphene::peerplays_sidechain::hive::escrow_dispute_operation, )
FC_REFLECT(graphene::peerplays_sidechain::hive::escrow_release_operation, )
FC_REFLECT(graphene::peerplays_sidechain::hive::pow2_operation, )
FC_REFLECT(graphene::peerplays_sidechain::hive::escrow_approve_operation, )
FC_REFLECT(graphene::peerplays_sidechain::hive::transfer_to_savings_operation, )
FC_REFLECT(graphene::peerplays_sidechain::hive::transfer_from_savings_operation, )
FC_REFLECT(graphene::peerplays_sidechain::hive::cancel_transfer_from_savings_operation, )
FC_REFLECT(graphene::peerplays_sidechain::hive::custom_binary_operation, )
FC_REFLECT(graphene::peerplays_sidechain::hive::decline_voting_rights_operation, )
FC_REFLECT(graphene::peerplays_sidechain::hive::reset_account_operation, )
FC_REFLECT(graphene::peerplays_sidechain::hive::set_reset_account_operation, )
FC_REFLECT(graphene::peerplays_sidechain::hive::claim_reward_balance_operation, )
FC_REFLECT(graphene::peerplays_sidechain::hive::delegate_vesting_shares_operation,
           (delegator)(delegatee)(vesting_shares))
