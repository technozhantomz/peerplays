#pragma once
#include <graphene/chain/protocol/operations.hpp>
#include <graphene/chain/evaluator.hpp>
#include <graphene/chain/database.hpp>

namespace graphene
{
   namespace chain
   {

      class nft_lottery_token_purchase_evaluator : public evaluator<nft_lottery_token_purchase_evaluator>
      {
      public:
         typedef nft_lottery_token_purchase_operation operation_type;

         void_result do_evaluate(const nft_lottery_token_purchase_operation &o);
         object_id_type do_apply(const nft_lottery_token_purchase_operation &o);
      };

      class nft_lottery_reward_evaluator : public evaluator<nft_lottery_reward_evaluator>
      {
      public:
         typedef nft_lottery_reward_operation operation_type;

         void_result do_evaluate(const nft_lottery_reward_operation &o);
         void_result do_apply(const nft_lottery_reward_operation &o);
      };

      class nft_lottery_end_evaluator : public evaluator<nft_lottery_end_evaluator>
      {
      public:
         typedef nft_lottery_end_operation operation_type;

         void_result do_evaluate(const nft_lottery_end_operation &o);
         void_result do_apply(const nft_lottery_end_operation &o);
      };

   } // namespace chain
} // namespace graphene
