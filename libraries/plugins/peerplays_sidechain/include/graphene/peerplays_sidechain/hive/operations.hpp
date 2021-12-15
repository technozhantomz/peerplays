#pragma once

#include <graphene/peerplays_sidechain/hive/hive_operations.hpp>

namespace graphene { namespace peerplays_sidechain { namespace hive {

typedef fc::static_variant<
      vote_operation,
      comment_operation,

      transfer_operation,
      transfer_to_vesting_operation,
      withdraw_vesting_operation,

      limit_order_create_operation,
      limit_order_cancel_operation,

      feed_publish_operation,
      convert_operation,

      account_create_operation,
      account_update_operation,

      witness_update_operation,
      account_witness_vote_operation,
      account_witness_proxy_operation,

      pow_operation,

      custom_operation,

      report_over_production_operation,

      delete_comment_operation,
      custom_json_operation,
      comment_options_operation,
      set_withdraw_vesting_route_operation,
      limit_order_create2_operation,
      claim_account_operation,
      create_claimed_account_operation,
      request_account_recovery_operation,
      recover_account_operation,
      change_recovery_account_operation,
      escrow_transfer_operation,
      escrow_dispute_operation,
      escrow_release_operation,
      pow2_operation,
      escrow_approve_operation,
      transfer_to_savings_operation,
      transfer_from_savings_operation,
      cancel_transfer_from_savings_operation,
      custom_binary_operation,
      decline_voting_rights_operation,
      reset_account_operation,
      set_reset_account_operation,
      claim_reward_balance_operation,
      delegate_vesting_shares_operation>
      hive_operation;

}}} // namespace graphene::peerplays_sidechain::hive

namespace fc {

void to_variant(const graphene::peerplays_sidechain::hive::hive_operation &var, fc::variant &vo, uint32_t max_depth = 5);
void from_variant(const fc::variant &var, graphene::peerplays_sidechain::hive::hive_operation &vo, uint32_t max_depth = 5);

} // namespace fc

FC_REFLECT_TYPENAME(graphene::peerplays_sidechain::hive::hive_operation)
