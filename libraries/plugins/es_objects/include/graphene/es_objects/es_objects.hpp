/*
 * Copyright (c) 2018 oxarbitrage, and contributors.
 *
 * The MIT License
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */
#pragma once

#include <graphene/app/plugin.hpp>
#include <graphene/chain/database.hpp>

namespace graphene { namespace es_objects {

using namespace chain;


namespace detail
{
    class es_objects_plugin_impl;
}

class es_objects_plugin : public graphene::app::plugin
{
   public:
      es_objects_plugin();
      virtual ~es_objects_plugin();

      std::string plugin_name()const override;
      std::string plugin_description()const override;
      virtual void plugin_set_program_options(
         boost::program_options::options_description& cli,
         boost::program_options::options_description& cfg) override;
      virtual void plugin_initialize(const boost::program_options::variables_map& options) override;
      virtual void plugin_startup() override;

      friend class detail::es_objects_plugin_impl;
      std::unique_ptr<detail::es_objects_plugin_impl> my;
};

struct proposal_struct {
   object_id_type object_id;
   fc::time_point_sec block_time;
   uint32_t block_number;
   time_point_sec expiration_time;
   optional<time_point_sec> review_period_time;
   string proposed_transaction;
   string required_active_approvals;
   string available_active_approvals;
   string required_owner_approvals;
   string available_owner_approvals;
   string available_key_approvals;
   account_id_type proposer;

   friend bool operator==(const proposal_struct& l, const proposal_struct& r)
   {
      return std::tie(l.object_id, l.block_time, l.block_number, l.expiration_time, l.review_period_time,
                      l.proposed_transaction, l.required_active_approvals, l.available_active_approvals,
                      l.required_owner_approvals, l.available_owner_approvals, l.available_key_approvals,
                      l.proposer) == std::tie(r.object_id, r.block_time, r.block_number, r.expiration_time, r.review_period_time,
                      r.proposed_transaction, r.required_active_approvals, r.available_active_approvals,
                      r.required_owner_approvals, r.available_owner_approvals, r.available_key_approvals,
                      r.proposer);
   }
   friend bool operator!=(const proposal_struct& l, const proposal_struct& r)
   {
      return !operator==(l, r);
   }
};
struct account_struct {
   object_id_type object_id;
   fc::time_point_sec block_time;
   uint32_t block_number;
   time_point_sec membership_expiration_date;
   account_id_type registrar;
   account_id_type referrer;
   account_id_type lifetime_referrer;
   uint16_t network_fee_percentage;
   uint16_t lifetime_referrer_fee_percentage;
   uint16_t referrer_rewards_percentage;
   string name;
   string owner_account_auths;
   string owner_key_auths;
   string owner_address_auths;
   string active_account_auths;
   string active_key_auths;
   string active_address_auths;
   account_id_type voting_account;
   string votes;

   friend bool operator==(const account_struct& l, const account_struct& r)
   {
      return std::tie(l.object_id, l.block_time, l.block_number, l.membership_expiration_date, l.registrar, l.referrer,
                      l.lifetime_referrer, l.network_fee_percentage, l.lifetime_referrer_fee_percentage,
                      l.referrer_rewards_percentage, l.name, l.owner_account_auths, l.owner_key_auths,
                      l.owner_address_auths, l.active_account_auths, l.active_key_auths, l.active_address_auths,
                      l.voting_account, l.votes) == std::tie(r.object_id, r.block_time, r.block_number, r.membership_expiration_date, r.registrar, r.referrer,
                      r.lifetime_referrer, r.network_fee_percentage, r.lifetime_referrer_fee_percentage,
                      r.referrer_rewards_percentage, r.name, r.owner_account_auths, r.owner_key_auths,
                      r.owner_address_auths, r.active_account_auths, r.active_key_auths, r.active_address_auths,
                      r.voting_account, r.votes);
   }
   friend bool operator!=(const account_struct& l, const account_struct& r)
   {
      return !operator==(l, r);
   }
};
struct asset_struct {
   object_id_type object_id;
   fc::time_point_sec block_time;
   uint32_t block_number;
   string symbol;
   account_id_type issuer;
   bool is_market_issued;
   asset_dynamic_data_id_type dynamic_asset_data_id;
   optional<asset_bitasset_data_id_type> bitasset_data_id;

   friend bool operator==(const asset_struct& l, const asset_struct& r)
   {
      return std::tie(l.object_id, l.block_time, l.block_number, l.symbol, l.issuer, l.is_market_issued,
                      l.dynamic_asset_data_id, l.bitasset_data_id) == std::tie(r.object_id, r.block_time,
                      r.block_number, r.symbol, r.issuer, r.is_market_issued, r.dynamic_asset_data_id,
                      r.bitasset_data_id);
   }
   friend bool operator!=(const asset_struct& l, const asset_struct& r)
   {
      return !operator==(l, r);
   }
};
struct balance_struct {
   object_id_type object_id;
   fc::time_point_sec block_time;
   uint32_t block_number;
   account_id_type owner;
   asset_id_type asset_type;
   share_type balance;

   friend bool operator==(const balance_struct& l, const balance_struct& r)
   {
      return std::tie(l.object_id, l.block_time, l.block_number, l.block_time, l.owner, l.asset_type, l.balance)
            == std::tie(r.object_id, r.block_time, r.block_number, r.block_time, r.owner, r.asset_type, r.balance);
   }
   friend bool operator!=(const balance_struct& l, const balance_struct& r)
   {
      return !operator==(l, r);
   }
};
struct limit_order_struct {
   object_id_type object_id;
   fc::time_point_sec block_time;
   uint32_t block_number;
   time_point_sec expiration;
   account_id_type seller;
   share_type for_sale;
   price sell_price;
   share_type deferred_fee;

   friend bool operator==(const limit_order_struct& l, const limit_order_struct& r)
   {
      return std::tie(l.object_id, l.block_time, l.block_number, l.expiration, l.seller, l.for_sale, l.sell_price, l.deferred_fee)
            == std::tie(r.object_id, r.block_time, r.block_number, r.expiration, r.seller, r.for_sale, r.sell_price, r.deferred_fee);
   }
   friend bool operator!=(const limit_order_struct& l, const limit_order_struct& r)
   {
      return !operator==(l, r);
   }
};
struct bitasset_struct {
   object_id_type object_id;
   fc::time_point_sec block_time;
   uint32_t block_number;
   string current_feed;
   time_point_sec current_feed_publication_time;
   time_point_sec feed_expiration_time;

   friend bool operator==(const bitasset_struct& l, const bitasset_struct& r)
   {
      return std::tie(l.object_id, l.block_time, l.block_number, l.current_feed, l.current_feed_publication_time)
            == std::tie(r.object_id, r.block_time, r.block_number, r.current_feed, r.current_feed_publication_time);
   }
   friend bool operator!=(const bitasset_struct& l, const bitasset_struct& r)
   {
      return !operator==(l, r);
   }
};

} } //graphene::es_objects

FC_REFLECT(
        graphene::es_objects::proposal_struct,
        (object_id)(block_time)(block_number)(expiration_time)(review_period_time)(proposed_transaction)(required_active_approvals)
        (available_active_approvals)(required_owner_approvals)(available_owner_approvals)(available_key_approvals)(proposer)
)
FC_REFLECT(
        graphene::es_objects::account_struct,
        (object_id)(block_time)(block_number)(membership_expiration_date)(registrar)(referrer)(lifetime_referrer)
        (network_fee_percentage)(lifetime_referrer_fee_percentage)(referrer_rewards_percentage)(name)(owner_account_auths)
        (owner_key_auths)(owner_address_auths)(active_account_auths)(active_key_auths)(active_address_auths)(voting_account)(votes)
)
FC_REFLECT(
        graphene::es_objects::asset_struct,
        (object_id)(block_time)(block_number)(symbol)(issuer)(is_market_issued)(dynamic_asset_data_id)(bitasset_data_id)
)
FC_REFLECT(
        graphene::es_objects::balance_struct,
        (object_id)(block_time)(block_number)(owner)(asset_type)(balance)
)
FC_REFLECT(
        graphene::es_objects::limit_order_struct,
        (object_id)(block_time)(block_number)(expiration)(seller)(for_sale)(sell_price)(deferred_fee)
)
FC_REFLECT(
        graphene::es_objects::bitasset_struct,
        (object_id)(block_time)(block_number)(current_feed)(current_feed_publication_time)
)
