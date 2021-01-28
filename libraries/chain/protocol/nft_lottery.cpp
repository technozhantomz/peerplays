#include <graphene/chain/protocol/nft_ops.hpp>
#include <graphene/chain/protocol/nft_lottery.hpp>
#include <graphene/chain/protocol/operations.hpp>

namespace graphene
{
    namespace chain
    {

        void nft_lottery_options::validate() const
        {
            FC_ASSERT(winning_tickets.size() <= 64);
            FC_ASSERT(ticket_price.amount >= 1);
            uint16_t total = 0;
            for (auto benefactor : benefactors)
            {
                total += benefactor.share;
            }
            for (auto share : winning_tickets)
            {
                total += share;
            }
            FC_ASSERT(total == GRAPHENE_100_PERCENT, "distribution amount not equals GRAPHENE_100_PERCENT");
            FC_ASSERT(ending_on_soldout == true || end_date != time_point_sec(), "lottery may not end");
        }

        share_type nft_lottery_token_purchase_operation::calculate_fee(const fee_parameters_type &k) const
        {
            return k.fee;
        }

        void nft_lottery_token_purchase_operation::validate() const
        {
            FC_ASSERT(fee.amount >= 0, "Fee must not be negative");
            FC_ASSERT(tickets_to_buy > 0);
        }
    } // namespace chain
} // namespace graphene