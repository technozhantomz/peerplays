#pragma once
#include <graphene/chain/protocol/base.hpp>
#include <graphene/chain/protocol/memo.hpp>

namespace graphene
{
   namespace chain
   {

      /*
 *  @class offer_operation
 *  @brief To place an offer to buy or sell an item, a user broadcasts a
 * proposed transaction
 *  @ingroup operations
 * operation
 */
      struct offer_operation : public base_operation
      {
         struct fee_parameters_type
         {
            uint64_t fee = GRAPHENE_BLOCKCHAIN_PRECISION;
            uint32_t price_per_kbyte = GRAPHENE_BLOCKCHAIN_PRECISION; /// only required for large memos.
         };
         asset fee;
         set<nft_id_type> item_ids;
         // /**
         //  * minimum_price.asset_id == maximum_price.asset_id.
         //  * to set fixed price without auction minimum_price == maximum_price
         //  * If buying_item is true, and minimum_price != maximum_price, the user is
         //  proposing a “reverse auction”
         //  * where bidders can offer to sell the item for progressively lower prices.
         //  * In this case, minimum_price functions as the sell-it-now price for the
         //  reverse auction
         //  */
         account_id_type issuer;
         /// minimum_price is minimum bid price. 0 if no minimum_price required
         asset minimum_price;
         /// buy_it_now price. 0 if no maximum price
         asset maximum_price;
         /// true means user wants to buy item, false mean user is selling item
         bool buying_item;
         /// not transaction expiration date
         fc::time_point_sec offer_expiration_date;

         /// User provided data encrypted to the memo key of the "to" account
         optional<memo_data> memo;
         extensions_type extensions;

         account_id_type fee_payer() const { return issuer; }
         void validate() const;
         share_type calculate_fee(const fee_parameters_type &k) const;
      };

      struct bid_operation : public base_operation
      {
         struct fee_parameters_type
         {
            uint64_t fee = GRAPHENE_BLOCKCHAIN_PRECISION;
         };

         asset fee;
         account_id_type bidder;

         asset bid_price;
         offer_id_type offer_id;

         extensions_type extensions;

         account_id_type fee_payer() const { return bidder; }
         void validate() const;
         share_type calculate_fee(const fee_parameters_type &k) const;
      };

      struct cancel_offer_operation : public base_operation
      {
         struct fee_parameters_type
         {
            uint64_t fee = GRAPHENE_BLOCKCHAIN_PRECISION;
         };

         asset fee;

         account_id_type issuer;
         offer_id_type offer_id;

         extensions_type extensions;

         account_id_type fee_payer() const { return issuer; }
         void validate() const;
         share_type calculate_fee(const fee_parameters_type &k) const;
      };

      enum class result_type
      {
         Expired = 0,
         ExpiredNoBid = 1,
         Cancelled = 2
      };

      struct finalize_offer_operation : public base_operation
      {
         struct fee_parameters_type
         {
            uint64_t fee = 0;
         };

         asset fee;
         account_id_type fee_paying_account;

         offer_id_type offer_id;

         result_type result;

         extensions_type extensions;

         account_id_type fee_payer() const { return fee_paying_account; }
         void validate() const;
         share_type calculate_fee(const fee_parameters_type &k) const;
      };

   } // namespace chain
} // namespace graphene

FC_REFLECT(graphene::chain::offer_operation::fee_parameters_type,
           (fee)(price_per_kbyte));
FC_REFLECT(graphene::chain::offer_operation,
           (fee)(item_ids)(issuer)(minimum_price)(maximum_price)(buying_item)(offer_expiration_date)(memo)(extensions));

FC_REFLECT(graphene::chain::bid_operation::fee_parameters_type,
           (fee));
FC_REFLECT(graphene::chain::bid_operation,
           (fee)(bidder)(bid_price)(offer_id)(extensions));

FC_REFLECT(graphene::chain::cancel_offer_operation::fee_parameters_type,
           (fee));
FC_REFLECT(graphene::chain::cancel_offer_operation,
           (fee)(issuer)(offer_id)(extensions));

FC_REFLECT_ENUM(graphene::chain::result_type, (Expired)(ExpiredNoBid)(Cancelled));
FC_REFLECT(graphene::chain::finalize_offer_operation::fee_parameters_type,
           (fee));
FC_REFLECT(graphene::chain::finalize_offer_operation,
           (fee)(fee_paying_account)(offer_id)(result)(extensions));
