#include <graphene/chain/offer_evaluator.hpp>
#include <graphene/chain/account_object.hpp>
#include <graphene/chain/offer_object.hpp>
#include <graphene/chain/nft_object.hpp>
#include <graphene/chain/exceptions.hpp>
#include <graphene/chain/hardfork.hpp>
#include <graphene/chain/is_authorized_asset.hpp>
#include <iostream>

namespace graphene
{
    namespace chain
    {
        void_result offer_evaluator::do_evaluate(const offer_operation &op)
        {
            try
            {
                const database &d = db();
                auto now = d.head_block_time();
                FC_ASSERT(now >= HARDFORK_NFT_TIME, "Not allowed until NFT HF");
                op.issuer(d);
                for (const auto &item : op.item_ids)
                {
                    const auto &nft_obj = item(d);
                    FC_ASSERT(!d.item_locked(item), "Item(s) is already on sale");
                    bool is_owner = (nft_obj.owner == op.issuer);
                    bool is_approved = (nft_obj.approved == op.issuer);
                    bool is_approved_operator = (std::find(nft_obj.approved_operators.begin(), nft_obj.approved_operators.end(), op.issuer) != nft_obj.approved_operators.end());
                    if (op.buying_item)
                    {
                        FC_ASSERT(!is_owner, "Buyer cannot already be an onwer of the item");
                        FC_ASSERT(!is_approved, "Buyer cannot already be approved account of the item");
                        FC_ASSERT(!is_approved_operator, "Buyer cannot already be an approved operator of the item");
                    }
                    else
                    {
                        FC_ASSERT(is_owner, "Issuer has no authority to sell the item");
                    }
                    const auto &nft_meta_obj = nft_obj.nft_metadata_id(d);
                    FC_ASSERT(nft_meta_obj.is_sellable == true, "NFT is not sellable");
                }
                FC_ASSERT(op.offer_expiration_date > d.head_block_time(), "Expiration should be in future");
                FC_ASSERT(op.fee.amount >= 0, "Invalid fee");
                FC_ASSERT(op.minimum_price.amount >= 0 && op.maximum_price.amount > 0, "Invalid amount");
                FC_ASSERT(op.minimum_price.asset_id == op.maximum_price.asset_id, "Asset ID mismatch");
                FC_ASSERT(op.maximum_price >= op.minimum_price, "Invalid max min prices");
                return void_result();
            }
            FC_CAPTURE_AND_RETHROW((op))
        }

        object_id_type offer_evaluator::do_apply(const offer_operation &op)
        {
            try
            {
                database &d = db();
                if (op.buying_item)
                {
                    d.adjust_balance(op.issuer, -op.maximum_price);
                }

                const auto &offer_obj = db().create<offer_object>([&](offer_object &obj) {
                    obj.issuer = op.issuer;

                    obj.item_ids = op.item_ids;

                    obj.minimum_price = op.minimum_price;
                    obj.maximum_price = op.maximum_price;

                    obj.buying_item = op.buying_item;
                    obj.offer_expiration_date = op.offer_expiration_date;
                });
                return offer_obj.id;
            }
            FC_CAPTURE_AND_RETHROW((op))
        }

        void_result bid_evaluator::do_evaluate(const bid_operation &op)
        {
            try
            {
                const database &d = db();
                auto now = d.head_block_time();
                FC_ASSERT(now >= HARDFORK_NFT_TIME, "Not allowed until NFT HF");
                const auto &offer = op.offer_id(d);
                op.bidder(d);
                for (const auto &item : offer.item_ids)
                {
                    const auto &nft_obj = item(d);
                    bool is_owner = (nft_obj.owner == op.bidder);
                    bool is_approved = (nft_obj.approved == op.bidder);
                    bool is_approved_operator = (std::find(nft_obj.approved_operators.begin(), nft_obj.approved_operators.end(), op.bidder) != nft_obj.approved_operators.end());
                    if (offer.buying_item)
                    {
                        FC_ASSERT(is_owner, "Bidder has no authority to sell the item");
                    }
                    else
                    {
                        FC_ASSERT(!is_owner, "Bidder cannot already be an onwer of the item");
                        FC_ASSERT(!is_approved, "Bidder cannot already be an approved account of the item");
                        FC_ASSERT(!is_approved_operator, "Bidder cannot already be an approved operator of the item");
                    }
                }

                FC_ASSERT(op.bid_price.asset_id == offer.minimum_price.asset_id, "Asset type mismatch");
                FC_ASSERT(offer.minimum_price.amount == 0 || op.bid_price >= offer.minimum_price);
                FC_ASSERT(offer.maximum_price.amount == 0 || op.bid_price <= offer.maximum_price);
                if (offer.bidder)
                {
                    FC_ASSERT((offer.buying_item && op.bid_price < *offer.bid_price) || (!offer.buying_item && op.bid_price > *offer.bid_price), "There is already a better bid than this");
                }
                return void_result();
            }
            FC_CAPTURE_AND_RETHROW((op))
        }

        void_result bid_evaluator::do_apply(const bid_operation &op)
        {
            try
            {
                database &d = db();

                const auto &offer = op.offer_id(d);

                if (!offer.buying_item)
                {
                    if (offer.bidder)
                    {
                        d.adjust_balance(*offer.bidder, *offer.bid_price);
                    }
                    d.adjust_balance(op.bidder, -op.bid_price);
                }
                d.modify(op.offer_id(d), [&](offer_object &o) {
                    if (op.bid_price == (offer.buying_item ? offer.minimum_price : offer.maximum_price))
                    {
                        o.offer_expiration_date = d.head_block_time();
                    }
                    o.bidder = op.bidder;
                    o.bid_price = op.bid_price;
                });
                return void_result();
            }
            FC_CAPTURE_AND_RETHROW((op))
        }

        void_result cancel_offer_evaluator::do_evaluate(const cancel_offer_operation &op)
        {
            try
            {
                const database &d = db();
                auto now = d.head_block_time();
                FC_ASSERT(now >= HARDFORK_NFT_TIME, "Not allowed until NFT HF");
                const auto &offer = op.offer_id(d);
                op.issuer(d);
                FC_ASSERT(op.issuer == offer.issuer, "Only offer issuer can cancel the offer");
                FC_ASSERT(offer.offer_expiration_date > d.head_block_time(), "Expiration should be in future when cancelling the offer");
                return void_result();
            }
            FC_CAPTURE_AND_RETHROW((op))
        }

        void_result cancel_offer_evaluator::do_apply(const cancel_offer_operation &op)
        {
            try
            {
                database &d = db();

                const auto &offer = op.offer_id(d);
                if (offer.buying_item)
                {
                    // Refund the max price to issuer
                    d.adjust_balance(offer.issuer, offer.maximum_price);
                }
                else
                {
                    if (offer.bidder)
                    {
                        // Refund the bid price to the best bidder till now
                        d.adjust_balance(*offer.bidder, *offer.bid_price);
                    }
                }

                d.create<offer_history_object>([&](offer_history_object &obj) {
                    obj.issuer = offer.issuer;
                    obj.item_ids = offer.item_ids;
                    obj.bidder = offer.bidder;
                    obj.bid_price = offer.bid_price;
                    obj.minimum_price = offer.minimum_price;
                    obj.maximum_price = offer.maximum_price;
                    obj.buying_item = offer.buying_item;
                    obj.offer_expiration_date = offer.offer_expiration_date;
                    obj.result = result_type::Cancelled;
                });
                // This should unlock the item
                d.remove(op.offer_id(d));

                return void_result();
            }
            FC_CAPTURE_AND_RETHROW((op))
        }

        void_result finalize_offer_evaluator::do_evaluate(const finalize_offer_operation &op)
        {
            try
            {
                const database &d = db();
                auto now = d.head_block_time();
                FC_ASSERT(now >= HARDFORK_NFT_TIME, "Not allowed until NFT HF");
                const auto &offer = op.offer_id(d);

                if (op.result != result_type::ExpiredNoBid)
                {
                    FC_ASSERT(offer.bidder, "No valid bidder");
                    FC_ASSERT((*offer.bid_price).amount >= 0, "Invalid bid price");
                }
                else
                {
                    FC_ASSERT(!offer.bidder, "There should not be a valid bidder");
                }

                switch (op.result)
                {
                case result_type::Expired:
                case result_type::ExpiredNoBid:
                    FC_ASSERT(offer.offer_expiration_date <= d.head_block_time(), "Offer finalized beyong expiration time");
                    break;
                default:
                    FC_THROW_EXCEPTION(fc::assert_exception, "finalize_offer_operation: unknown result type.");
                    break;
                }
                return void_result();
            }
            FC_CAPTURE_AND_RETHROW((op))
        }

        void_result finalize_offer_evaluator::do_apply(const finalize_offer_operation &op)
        {
            try
            {
                database &d = db();
                offer_object offer = op.offer_id(d);
                // Calculate the fees for all revenue partners of the items
                auto calc_fee = [&](int64_t &tot_fees) {
                    map<account_id_type, asset> fee_map;
                    for (const auto &item : offer.item_ids)
                    {
                        const auto &nft_obj = item(d);
                        const auto &nft_meta_obj = nft_obj.nft_metadata_id(d);
                        if (nft_meta_obj.revenue_partner && *nft_meta_obj.revenue_split > 0)
                        {
                            const auto &rev_partner = *nft_meta_obj.revenue_partner;
                            const auto &rev_split = *nft_meta_obj.revenue_split;
                            int64_t item_fee = static_cast<int64_t>((rev_split * (*offer.bid_price).amount.value / GRAPHENE_100_PERCENT) / offer.item_ids.size());
                            const auto &fee_asset = asset(item_fee, (*offer.bid_price).asset_id);
                            auto ret_val = fee_map.insert({rev_partner, fee_asset});
                            if (ret_val.second == false)
                            {
                                fee_map[rev_partner] += fee_asset;
                            }
                            tot_fees += item_fee;
                        }
                    }
                    return fee_map;
                };

                if (op.result != result_type::ExpiredNoBid)
                {
                    int64_t tot_fees = 0;
                    auto &&fee_map = calc_fee(tot_fees);
                    // Transfer all the fee
                    for (const auto &fee_itr : fee_map)
                    {
                        auto &acc = fee_itr.first;
                        auto &acc_fee = fee_itr.second;
                        d.adjust_balance(acc, acc_fee);
                    }
                    // Calculate the remaining seller amount after the fee is deducted
                    auto &&seller_amount = *offer.bid_price - asset(tot_fees, (*offer.bid_price).asset_id);
                    // Buy Offer
                    if (offer.buying_item)
                    {
                        // Send the seller his amount
                        d.adjust_balance(*offer.bidder, seller_amount);
                        if (offer.bid_price < offer.maximum_price)
                        {
                            // Send the buyer the delta
                            d.adjust_balance(offer.issuer, offer.maximum_price - *offer.bid_price);
                        }
                    }
                    else
                    {
                        // Sell Offer, send the seller his amount
                        d.adjust_balance(offer.issuer, seller_amount);
                    }
                    // Tranfer the NFTs
                    for (auto item : offer.item_ids)
                    {
                        auto &nft_obj = item(d);
                        d.modify(nft_obj, [&offer](nft_object &obj) {
                            if (offer.buying_item)
                            {
                                obj.owner = offer.issuer;
                            }
                            else
                            {
                                obj.owner = *offer.bidder;
                            }
                            obj.approved = {};
                            obj.approved_operators.clear();
                        });
                    }
                }
                else
                {
                    if (offer.buying_item)
                    {
                        d.adjust_balance(offer.issuer, offer.maximum_price);
                    }
                }
                d.create<offer_history_object>([&](offer_history_object &obj) {
                    obj.issuer = offer.issuer;
                    obj.item_ids = offer.item_ids;
                    obj.bidder = offer.bidder;
                    obj.bid_price = offer.bid_price;
                    obj.minimum_price = offer.minimum_price;
                    obj.maximum_price = offer.maximum_price;
                    obj.buying_item = offer.buying_item;
                    obj.offer_expiration_date = offer.offer_expiration_date;
                    obj.result = op.result;
                });
                // This should unlock the item
                d.remove(op.offer_id(d));
                return void_result();
            }
            FC_CAPTURE_AND_RETHROW((op))
        }
    } // namespace chain
} // namespace graphene