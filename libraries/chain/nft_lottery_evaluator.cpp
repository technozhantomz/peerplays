#include <graphene/chain/nft_lottery_evaluator.hpp>
#include <graphene/chain/nft_object.hpp>
#include <graphene/chain/protocol/operations.hpp>
#include <graphene/chain/account_role_object.hpp>
#include <graphene/chain/hardfork.hpp>

namespace graphene
{
    namespace chain
    {
        void_result nft_lottery_token_purchase_evaluator::do_evaluate(const nft_lottery_token_purchase_operation &op)
        {
            try
            {
                const database &d = db();
                auto now = d.head_block_time();
                FC_ASSERT(now >= HARDFORK_NFT_TIME, "Not allowed until NFT HF");
                op.buyer(d);
                const auto &lottery_md_obj = op.lottery_id(d);
                FC_ASSERT(lottery_md_obj.is_lottery(), "Not a lottery type");
                if (lottery_md_obj.account_role)
                {
                    const auto &ar_idx = d.get_index_type<account_role_index>().indices().get<by_id>();
                    auto ar_itr = ar_idx.find(*lottery_md_obj.account_role);
                    if (ar_itr != ar_idx.end())
                    {
                        FC_ASSERT(d.account_role_valid(*ar_itr, op.buyer, get_type()), "Account role not valid");
                    }
                }

                auto lottery_options = lottery_md_obj.lottery_data->lottery_options;
                FC_ASSERT(lottery_options.ticket_price.asset_id == op.amount.asset_id);
                FC_ASSERT((double)op.amount.amount.value / lottery_options.ticket_price.amount.value == (double)op.tickets_to_buy);
                return void_result();
            }
            FC_CAPTURE_AND_RETHROW((op))
        }

        object_id_type nft_lottery_token_purchase_evaluator::do_apply(const nft_lottery_token_purchase_operation &op)
        {
            try
            {
                transaction_evaluation_state nft_mint_context(&db());
                nft_mint_context.skip_fee_schedule_check = true;
                const auto &lottery_md_obj = op.lottery_id(db());
                nft_id_type nft_id;
                for (size_t i = 0; i < op.tickets_to_buy; i++)
                {
                    nft_mint_operation mint_op;
                    mint_op.payer = lottery_md_obj.owner;
                    mint_op.nft_metadata_id = lottery_md_obj.id;
                    mint_op.owner = op.buyer;
                    nft_id = db().apply_operation(nft_mint_context, mint_op).get<object_id_type>();
                }
                db().adjust_balance(op.buyer, -op.amount);
                db().modify(lottery_md_obj.lottery_data->lottery_balance_id(db()), [&](nft_lottery_balance_object &obj) {
                    obj.jackpot += op.amount;
                });
                return nft_id;
            }
            FC_CAPTURE_AND_RETHROW((op))
        }

        void_result nft_lottery_reward_evaluator::do_evaluate(const nft_lottery_reward_operation &op)
        {
            try
            {
                const database &d = db();
                auto now = d.head_block_time();
                FC_ASSERT(now >= HARDFORK_NFT_TIME, "Not allowed until NFT HF");
                op.winner(d);

                const auto &lottery_md_obj = op.lottery_id(d);
                FC_ASSERT(lottery_md_obj.is_lottery());

                const auto &lottery_options = lottery_md_obj.lottery_data->lottery_options;
                FC_ASSERT(lottery_options.is_active);
                FC_ASSERT(lottery_md_obj.get_lottery_jackpot(d) >= op.amount);
                return void_result();
            }
            FC_CAPTURE_AND_RETHROW((op))
        }

        void_result nft_lottery_reward_evaluator::do_apply(const nft_lottery_reward_operation &op)
        {
            try
            {
                const auto &lottery_md_obj = op.lottery_id(db());
                db().adjust_balance(op.winner, op.amount);
                db().modify(lottery_md_obj.lottery_data->lottery_balance_id(db()), [&](nft_lottery_balance_object &obj) {
                    obj.jackpot -= op.amount;
                });
                return void_result();
            }
            FC_CAPTURE_AND_RETHROW((op))
        }

        void_result nft_lottery_end_evaluator::do_evaluate(const nft_lottery_end_operation &op)
        {
            try
            {
                const database &d = db();
                auto now = d.head_block_time();
                FC_ASSERT(now >= HARDFORK_NFT_TIME, "Not allowed until NFT HF");
                const auto &lottery_md_obj = op.lottery_id(d);
                FC_ASSERT(lottery_md_obj.is_lottery());

                const auto &lottery_options = lottery_md_obj.lottery_data->lottery_options;
                FC_ASSERT(lottery_options.is_active);
                FC_ASSERT(lottery_md_obj.get_lottery_jackpot(d).amount == 0);
                return void_result();
            }
            FC_CAPTURE_AND_RETHROW((op))
        }

        void_result nft_lottery_end_evaluator::do_apply(const nft_lottery_end_operation &op)
        {
            try
            {
                const auto &lottery_md_obj = op.lottery_id(db());
                db().modify(lottery_md_obj, [&](nft_metadata_object &obj) {
                    obj.lottery_data->lottery_options.is_active = false;
                });
                db().modify(lottery_md_obj.lottery_data->lottery_balance_id(db()), [&](nft_lottery_balance_object &obj) {
                    obj.sweeps_tickets_sold = lottery_md_obj.get_token_current_supply(db());
                });

                if (lottery_md_obj.lottery_data->lottery_options.delete_tickets_after_draw)
                {
                    const auto &nft_index_by_md = db().get_index_type<nft_index>().indices().get<by_metadata>();
                    auto delete_nft_itr = nft_index_by_md.lower_bound(op.lottery_id);
                    while (delete_nft_itr != nft_index_by_md.end() && delete_nft_itr->nft_metadata_id == op.lottery_id)
                    {
                        const nft_object &nft_obj = *delete_nft_itr;
                        ++delete_nft_itr;
                        db().remove(nft_obj);
                    }
                }

                return void_result();
            }
            FC_CAPTURE_AND_RETHROW((op))
        }
    } // namespace chain
} // namespace graphene