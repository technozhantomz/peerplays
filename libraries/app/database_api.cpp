/*
 * Copyright (c) 2015 Cryptonomex, Inc., and contributors.
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

#include <graphene/app/database_api.hpp>
#include <graphene/chain/account_object.hpp>
#include <graphene/chain/get_config.hpp>
#include <graphene/chain/protocol/address.hpp>
#include <graphene/chain/pts_address.hpp>
#include <graphene/chain/tournament_object.hpp>

#include <graphene/utilities/git_revision.hpp>

#include <fc/bloom_filter.hpp>

#include <fc/crypto/hex.hpp>
#include <fc/rpc/api_connection.hpp>
#include <fc/uint128.hpp>

#include <boost/algorithm/string.hpp>
#include <boost/multiprecision/cpp_int.hpp>
#include <boost/range/iterator_range.hpp>
#include <boost/rational.hpp>

#include <cctype>

#include <cfenv>
#include <iostream>

#define GET_REQUIRED_FEES_MAX_RECURSION 4

typedef std::map<std::pair<graphene::chain::asset_id_type, graphene::chain::asset_id_type>, std::vector<fc::variant>> market_queue_type;

template class fc::api<graphene::app::database_api>;

namespace graphene { namespace app {

template <class T>
optional<T> maybe_id(const string &name_or_id) {
   if (std::isdigit(name_or_id.front())) {
      try {
         return fc::variant(name_or_id, 1).as<T>(1);
      } catch (const fc::exception &) { // not an ID
      }
   }
   return optional<T>();
}

std::string object_id_to_string(object_id_type id) {
   std::string object_id = fc::to_string(id.space()) + "." + fc::to_string(id.type()) + "." + fc::to_string(id.instance());
   return object_id;
}

class database_api_impl : public std::enable_shared_from_this<database_api_impl> {
public:
   database_api_impl(graphene::chain::database &db);
   ~database_api_impl();

   // Objects
   fc::variants get_objects(const vector<object_id_type> &ids) const;

   // Subscriptions
   void set_subscribe_callback(std::function<void(const variant &)> cb, bool notify_remove_create);
   void set_pending_transaction_callback(std::function<void(const variant &)> cb);
   void set_block_applied_callback(std::function<void(const variant &block_id)> cb);
   void cancel_all_subscriptions();

   // Blocks and transactions
   optional<block_header> get_block_header(uint32_t block_num) const;
   map<uint32_t, optional<block_header>> get_block_header_batch(const vector<uint32_t> block_nums) const;
   optional<signed_block> get_block(uint32_t block_num) const;
   vector<optional<signed_block>> get_blocks(uint32_t block_num_from, uint32_t block_num_to) const;
   processed_transaction get_transaction(uint32_t block_num, uint32_t trx_in_block) const;

   // Globals
   version_info get_version_info() const;
   chain_property_object get_chain_properties() const;
   global_property_object get_global_properties() const;
   fc::variant_object get_config() const;
   chain_id_type get_chain_id() const;
   dynamic_global_property_object get_dynamic_global_properties() const;
   global_betting_statistics_object get_global_betting_statistics() const;

   // Keys
   vector<vector<account_id_type>> get_key_references(vector<public_key_type> key) const;
   bool is_public_key_registered(string public_key) const;

   // Accounts
   account_id_type get_account_id_from_string(const std::string &name_or_id) const;
   vector<optional<account_object>> get_accounts(const vector<std::string> &account_names_or_ids) const;
   std::map<string, full_account> get_full_accounts(const vector<string> &names_or_ids, bool subscribe);
   optional<account_object> get_account_by_name(string name) const;
   vector<account_id_type> get_account_references(const std::string account_id_or_name) const;
   vector<optional<account_object>> lookup_account_names(const vector<string> &account_names) const;
   map<string, account_id_type> lookup_accounts(const string &lower_bound_name, uint32_t limit) const;
   uint64_t get_account_count() const;

   // Balances
   vector<asset> get_account_balances(const std::string &account_name_or_id, const flat_set<asset_id_type> &assets) const;
   vector<balance_object> get_balance_objects(const vector<address> &addrs) const;
   vector<asset> get_vested_balances(const vector<balance_id_type> &objs) const;
   vector<vesting_balance_object> get_vesting_balances(const std::string account_id_or_name) const;

   // Assets
   asset_id_type get_asset_id_from_string(const std::string &symbol_or_id) const;
   vector<optional<asset_object>> get_assets(const vector<std::string> &asset_symbols_or_ids) const;
   // helper function
   vector<optional<asset_object>> get_assets(const vector<asset_id_type> &asset_ids) const;
   vector<asset_object> list_assets(const string &lower_bound_symbol, uint32_t limit) const;
   vector<optional<asset_object>> lookup_asset_symbols(const vector<string> &symbols_or_ids) const;
   uint64_t get_asset_count() const;

   // Peerplays
   vector<sport_object> list_sports() const;
   vector<event_group_object> list_event_groups(sport_id_type sport_id) const;
   vector<event_object> list_events_in_group(event_group_id_type event_group_id) const;
   vector<betting_market_group_object> list_betting_market_groups(event_id_type) const;
   vector<betting_market_object> list_betting_markets(betting_market_group_id_type) const;
   vector<bet_object> get_unmatched_bets_for_bettor(betting_market_id_type, account_id_type) const;
   vector<bet_object> get_all_unmatched_bets_for_bettor(account_id_type) const;

   // Lottery Assets
   vector<asset_object> get_lotteries(asset_id_type stop = asset_id_type(),
                                      unsigned limit = 100,
                                      asset_id_type start = asset_id_type()) const;
   vector<asset_object> get_account_lotteries(account_id_type issuer,
                                              asset_id_type stop,
                                              unsigned limit,
                                              asset_id_type start) const;
   asset get_lottery_balance(asset_id_type lottery_id) const;
   sweeps_vesting_balance_object get_sweeps_vesting_balance_object(account_id_type account) const;
   asset get_sweeps_vesting_balance_available_for_claim(account_id_type account) const;

   // Markets / feeds
   vector<limit_order_object> get_limit_orders(const asset_id_type a, const asset_id_type b, const uint32_t limit) const;
   vector<limit_order_object> get_limit_orders(const std::string &a, const std::string &b, const uint32_t limit) const;
   vector<call_order_object> get_call_orders(const std::string &a, uint32_t limit) const;
   vector<force_settlement_object> get_settle_orders(const std::string &a, uint32_t limit) const;
   vector<call_order_object> get_margin_positions(const std::string account_id_or_name) const;
   void subscribe_to_market(std::function<void(const variant &)> callback, const std::string &a, const std::string &b);
   void unsubscribe_from_market(const std::string &a, const std::string &b);
   market_ticker get_ticker(const string &base, const string &quote) const;
   market_volume get_24_volume(const string &base, const string &quote) const;
   order_book get_order_book(const string &base, const string &quote, unsigned limit = 50) const;
   vector<market_trade> get_trade_history(const string &base, const string &quote, fc::time_point_sec start, fc::time_point_sec stop, unsigned limit = 100) const;

   // Witnesses
   vector<optional<witness_object>> get_witnesses(const vector<witness_id_type> &witness_ids) const;
   fc::optional<witness_object> get_witness_by_account_id(account_id_type account) const;
   fc::optional<witness_object> get_witness_by_account(const std::string account_id_or_name) const;
   map<string, witness_id_type> lookup_witness_accounts(const string &lower_bound_name, uint32_t limit) const;
   uint64_t get_witness_count() const;

   // Committee members
   vector<optional<committee_member_object>> get_committee_members(const vector<committee_member_id_type> &committee_member_ids) const;
   fc::optional<committee_member_object> get_committee_member_by_account_id(account_id_type account) const;
   fc::optional<committee_member_object> get_committee_member_by_account(const std::string account_id_or_name) const;
   map<string, committee_member_id_type> lookup_committee_member_accounts(const string &lower_bound_name, uint32_t limit) const;
   uint64_t get_committee_member_count() const;

   // SON members
   vector<optional<son_object>> get_sons(const vector<son_id_type> &son_ids) const;
   fc::optional<son_object> get_son_by_account_id(account_id_type account) const;
   fc::optional<son_object> get_son_by_account(const std::string account_id_or_name) const;
   map<string, son_id_type> lookup_son_accounts(const string &lower_bound_name, uint32_t limit) const;
   uint64_t get_son_count() const;

   // SON wallets
   optional<son_wallet_object> get_active_son_wallet();
   optional<son_wallet_object> get_son_wallet_by_time_point(time_point_sec time_point);
   vector<optional<son_wallet_object>> get_son_wallets(uint32_t limit);

   // Sidechain addresses
   vector<optional<sidechain_address_object>> get_sidechain_addresses(const vector<sidechain_address_id_type> &sidechain_address_ids) const;
   vector<optional<sidechain_address_object>> get_sidechain_addresses_by_account(account_id_type account) const;
   vector<optional<sidechain_address_object>> get_sidechain_addresses_by_sidechain(sidechain_type sidechain) const;
   fc::optional<sidechain_address_object> get_sidechain_address_by_account_and_sidechain(account_id_type account, sidechain_type sidechain) const;
   uint64_t get_sidechain_addresses_count() const;

   // Workers
   vector<optional<worker_object>> get_workers(const vector<worker_id_type> &witness_ids) const;
   vector<worker_object> get_workers_by_account_id(account_id_type account) const;
   vector<worker_object> get_workers_by_account(const std::string account_id_or_name) const;
   map<string, worker_id_type> lookup_worker_accounts(const string &lower_bound_name, uint32_t limit) const;
   uint64_t get_worker_count() const;

   // Votes
   vector<variant> lookup_vote_ids(const vector<vote_id_type> &votes) const;
   vector<vote_id_type> get_votes_ids(const string &account_name_or_id) const;
   template <typename IndexType, typename Tag>
   vector<variant> get_votes_objects(const vector<vote_id_type> &votes, unsigned int variant_max_depth = 1) const {
      static_assert(std::is_base_of<index, IndexType>::value, "Type must be an index type");

      vector<variant> result;
      const auto &idx = _db.get_index_type<IndexType>().indices().template get<Tag>();
      for (auto id : votes) {
         auto itr = idx.find(id);
         if (itr != idx.end())
            result.emplace_back(variant(*itr, variant_max_depth));
      }
      return result;
   }
   votes_info get_votes(const string &account_name_or_id) const;
   vector<account_object> get_voters_by_id(const vote_id_type &vote_id) const;
   voters_info get_voters(const string &account_name_or_id) const;

   // Authority / validation
   std::string get_transaction_hex(const signed_transaction &trx) const;
   set<public_key_type> get_required_signatures(const signed_transaction &trx, const flat_set<public_key_type> &available_keys) const;
   set<public_key_type> get_potential_signatures(const signed_transaction &trx) const;
   set<address> get_potential_address_signatures(const signed_transaction &trx) const;
   bool verify_authority(const signed_transaction &trx) const;
   bool verify_account_authority(const string &name_or_id, const flat_set<public_key_type> &signers) const;
   processed_transaction validate_transaction(const signed_transaction &trx) const;
   vector<fc::variant> get_required_fees(const vector<operation> &ops, const std::string &asset_id_or_symbol) const;

   // Proposed transactions
   vector<proposal_object> get_proposed_transactions(const std::string account_id_or_name) const;

   // Blinded balances
   vector<blinded_balance_object> get_blinded_balances(const flat_set<commitment_type> &commitments) const;

   // Tournaments
   vector<tournament_object> get_tournaments_in_state(tournament_state state, uint32_t limit) const;
   vector<tournament_object> get_tournaments(tournament_id_type stop, unsigned limit, tournament_id_type start);
   vector<tournament_object> get_tournaments_by_state(tournament_id_type stop, unsigned limit, tournament_id_type start, tournament_state state);
   vector<tournament_id_type> get_registered_tournaments(account_id_type account_filter, uint32_t limit) const;

   // gpos
   gpos_info get_gpos_info(const account_id_type account) const;

   // rbac
   vector<custom_permission_object> get_custom_permissions(const account_id_type account) const;
   fc::optional<custom_permission_object> get_custom_permission_by_name(const account_id_type account, const string &permission_name) const;
   vector<custom_account_authority_object> get_custom_account_authorities(const account_id_type account) const;
   vector<custom_account_authority_object> get_custom_account_authorities_by_permission_id(const custom_permission_id_type permission_id) const;
   vector<custom_account_authority_object> get_custom_account_authorities_by_permission_name(const account_id_type account, const string &permission_name) const;
   vector<authority> get_active_custom_account_authorities_by_operation(const account_id_type account, int operation_type) const;

   // NFT
   uint64_t nft_get_balance(const account_id_type owner) const;
   optional<account_id_type> nft_owner_of(const nft_id_type token_id) const;
   optional<account_id_type> nft_get_approved(const nft_id_type token_id) const;
   bool nft_is_approved_for_all(const account_id_type owner, const account_id_type operator_) const;
   string nft_get_name(const nft_metadata_id_type nft_metadata_id) const;
   string nft_get_symbol(const nft_metadata_id_type nft_metadata_id) const;
   string nft_get_token_uri(const nft_id_type token_id) const;
   uint64_t nft_get_total_supply(const nft_metadata_id_type nft_metadata_id) const;
   nft_object nft_token_by_index(const nft_metadata_id_type nft_metadata_id, const uint64_t token_idx) const;
   nft_object nft_token_of_owner_by_index(const nft_metadata_id_type nft_metadata_id, const account_id_type owner, const uint64_t token_idx) const;
   vector<nft_object> nft_get_all_tokens() const;
   vector<nft_object> nft_get_tokens_by_owner(const account_id_type owner) const;

   // Marketplace
   vector<offer_object> list_offers(const offer_id_type lower_id, uint32_t limit) const;
   vector<offer_object> list_sell_offers(const offer_id_type lower_id, uint32_t limit) const;
   vector<offer_object> list_buy_offers(const offer_id_type lower_id, uint32_t limit) const;
   vector<offer_history_object> list_offer_history(const offer_history_id_type lower_id, uint32_t limit) const;
   vector<offer_object> get_offers_by_issuer(const offer_id_type lower_id, const account_id_type issuer_account_id, uint32_t limit) const;
   vector<offer_object> get_offers_by_item(const offer_id_type lower_id, const nft_id_type item, uint32_t limit) const;
   vector<offer_history_object> get_offer_history_by_issuer(const offer_history_id_type lower_id, const account_id_type issuer_account_id, uint32_t limit) const;
   vector<offer_history_object> get_offer_history_by_item(const offer_history_id_type lower_id, const nft_id_type item, uint32_t limit) const;
   vector<offer_history_object> get_offer_history_by_bidder(const offer_history_id_type lower_id, const account_id_type bidder_account_id, uint32_t limit) const;

   // Account Role
   vector<account_role_object> get_account_roles_by_owner(account_id_type owner) const;

   uint32_t api_limit_get_lower_bound_symbol = 100;
   uint32_t api_limit_get_limit_orders = 300;
   uint32_t api_limit_get_limit_orders_by_account = 101;
   uint32_t api_limit_get_order_book = 50;
   uint32_t api_limit_all_offers_count = 100;
   uint32_t api_limit_lookup_accounts = 1000;
   uint32_t api_limit_lookup_witness_accounts = 1000;
   uint32_t api_limit_lookup_committee_member_accounts = 1000;
   uint32_t api_limit_lookup_son_accounts = 1000;
   uint32_t api_limit_lookup_worker_accounts = 1000;
   uint32_t api_limit_get_trade_history = 100;
   uint32_t api_limit_get_trade_history_by_sequence = 100;

   //private:
   const account_object *get_account_from_string(const std::string &name_or_id,
                                                 bool throw_if_not_found = true) const;
   const asset_object *get_asset_from_string(const std::string &symbol_or_id,
                                             bool throw_if_not_found = true) const;
   template <typename T>
   void subscribe_to_item(const T &i) const {
      auto vec = fc::raw::pack(i);
      if (!_subscribe_callback)
         return;

      if (!is_subscribed_to_item(i)) {
         _subscribe_filter.insert(vec.data(), vec.size()); //(vecconst char*)&i, sizeof(i) );
      }
   }

   template <typename T>
   bool is_subscribed_to_item(const T &i) const {
      if (!_subscribe_callback)
         return false;

      return _subscribe_filter.contains(i);
   }

   bool is_impacted_account(const flat_set<account_id_type> &accounts) {
      if (!_subscribed_accounts.size() || !accounts.size())
         return false;

      return std::any_of(accounts.begin(), accounts.end(), [this](const account_id_type &account) {
         return _subscribed_accounts.find(account) != _subscribed_accounts.end();
      });
   }

   template <typename T>
   void enqueue_if_subscribed_to_market(const object *obj, market_queue_type &queue, bool full_object = true) {
      const T *order = dynamic_cast<const T *>(obj);
      FC_ASSERT(order != nullptr);

      auto market = order->get_market();

      auto sub = _market_subscriptions.find(market);
      if (sub != _market_subscriptions.end()) {
         queue[market].emplace_back(full_object ? obj->to_variant() : fc::variant(obj->id, 1));
      }
   }

   void broadcast_updates(const vector<variant> &updates);
   void broadcast_market_updates(const market_queue_type &queue);
   void handle_object_changed(bool force_notify, bool full_object, const vector<object_id_type> &ids, const flat_set<account_id_type> &impacted_accounts, std::function<const object *(object_id_type id)> find_object);

   /** called every time a block is applied to report the objects that were changed */
   void on_objects_new(const vector<object_id_type> &ids, const flat_set<account_id_type> &impacted_accounts);
   void on_objects_changed(const vector<object_id_type> &ids, const flat_set<account_id_type> &impacted_accounts);
   void on_objects_removed(const vector<object_id_type> &ids, const vector<const object *> &objs, const flat_set<account_id_type> &impacted_accounts);
   void on_applied_block();

   bool _notify_remove_create = false;
   mutable fc::bloom_filter _subscribe_filter;
   std::set<account_id_type> _subscribed_accounts;
   std::function<void(const fc::variant &)> _subscribe_callback;
   std::function<void(const fc::variant &)> _pending_trx_callback;
   std::function<void(const fc::variant &)> _block_applied_callback;

   boost::signals2::scoped_connection _new_connection;
   boost::signals2::scoped_connection _change_connection;
   boost::signals2::scoped_connection _removed_connection;
   boost::signals2::scoped_connection _applied_block_connection;
   boost::signals2::scoped_connection _pending_trx_connection;
   map<pair<asset_id_type, asset_id_type>, std::function<void(const variant &)>> _market_subscriptions;
   graphene::chain::database &_db;
};

//////////////////////////////////////////////////////////////////////
//                                                                  //
// Constructors                                                     //
//                                                                  //
//////////////////////////////////////////////////////////////////////

database_api::database_api(graphene::chain::database &db) :
      my(new database_api_impl(db)) {
}

database_api::~database_api() {
}

database_api_impl::database_api_impl(graphene::chain::database &db) :
      _db(db) {
   wlog("creating database api ${x}", ("x", int64_t(this)));
   _new_connection = _db.new_objects.connect([this](const vector<object_id_type> &ids, const flat_set<account_id_type> &impacted_accounts) {
      on_objects_new(ids, impacted_accounts);
   });
   _change_connection = _db.changed_objects.connect([this](const vector<object_id_type> &ids, const flat_set<account_id_type> &impacted_accounts) {
      on_objects_changed(ids, impacted_accounts);
   });
   _removed_connection = _db.removed_objects.connect([this](const vector<object_id_type> &ids, const vector<const object *> &objs, const flat_set<account_id_type> &impacted_accounts) {
      on_objects_removed(ids, objs, impacted_accounts);
   });
   _applied_block_connection = _db.applied_block.connect([this](const signed_block &) {
      on_applied_block();
   });

   _pending_trx_connection = _db.on_pending_transaction.connect([this](const signed_transaction &trx) {
      if (_pending_trx_callback)
         _pending_trx_callback(fc::variant(trx, GRAPHENE_MAX_NESTED_OBJECTS));
   });
}

database_api_impl::~database_api_impl() {
   elog("freeing database api ${x}", ("x", int64_t(this)));
}

//////////////////////////////////////////////////////////////////////
//                                                                  //
// Objects                                                          //
//                                                                  //
//////////////////////////////////////////////////////////////////////

fc::variants database_api::get_objects(const vector<object_id_type> &ids) const {
   return my->get_objects(ids);
}

fc::variants database_api_impl::get_objects(const vector<object_id_type> &ids) const {
   if (_subscribe_callback) {
      for (auto id : ids) {
         if (id.type() == operation_history_object_type && id.space() == protocol_ids)
            continue;
         if (id.type() == impl_account_transaction_history_object_type && id.space() == implementation_ids)
            continue;

         this->subscribe_to_item(id);
      }
   }

   fc::variants result;
   result.reserve(ids.size());

   std::transform(ids.begin(), ids.end(), std::back_inserter(result),
                  [this](object_id_type id) -> fc::variant {
                     if (auto obj = _db.find_object(id))
                        return obj->to_variant();
                     return {};
                  });

   return result;
}

//////////////////////////////////////////////////////////////////////
//                                                                  //
// Subscriptions                                                    //
//                                                                  //
//////////////////////////////////////////////////////////////////////

void database_api::set_subscribe_callback(std::function<void(const variant &)> cb, bool notify_remove_create) {
   my->set_subscribe_callback(cb, notify_remove_create);
}

void database_api_impl::set_subscribe_callback(std::function<void(const variant &)> cb, bool notify_remove_create) {
   //edump((clear_filter));
   _subscribe_callback = cb;
   _notify_remove_create = notify_remove_create;
   _subscribed_accounts.clear();

   static fc::bloom_parameters param;
   param.projected_element_count = 10000;
   param.false_positive_probability = 1.0 / 100;
   param.maximum_size = 1024 * 8 * 8 * 2;
   param.compute_optimal_parameters();
   _subscribe_filter = fc::bloom_filter(param);
}

void database_api::set_pending_transaction_callback(std::function<void(const variant &)> cb) {
   my->set_pending_transaction_callback(cb);
}

void database_api_impl::set_pending_transaction_callback(std::function<void(const variant &)> cb) {
   _pending_trx_callback = cb;
}

void database_api::set_block_applied_callback(std::function<void(const variant &block_id)> cb) {
   my->set_block_applied_callback(cb);
}

void database_api_impl::set_block_applied_callback(std::function<void(const variant &block_id)> cb) {
   _block_applied_callback = cb;
}

void database_api::cancel_all_subscriptions() {
   my->cancel_all_subscriptions();
}

void database_api_impl::cancel_all_subscriptions() {
   set_subscribe_callback(std::function<void(const fc::variant &)>(), true);
   _market_subscriptions.clear();
}

//////////////////////////////////////////////////////////////////////
//                                                                  //
// Blocks and transactions                                          //
//                                                                  //
//////////////////////////////////////////////////////////////////////

optional<block_header> database_api::get_block_header(uint32_t block_num) const {
   return my->get_block_header(block_num);
}

optional<block_header> database_api_impl::get_block_header(uint32_t block_num) const {
   auto result = _db.fetch_block_by_number(block_num);
   if (result)
      return *result;
   return {};
}
map<uint32_t, optional<block_header>> database_api::get_block_header_batch(const vector<uint32_t> block_nums) const {
   return my->get_block_header_batch(block_nums);
}

map<uint32_t, optional<block_header>> database_api_impl::get_block_header_batch(const vector<uint32_t> block_nums) const {
   map<uint32_t, optional<block_header>> results;
   for (const uint32_t block_num : block_nums) {
      results[block_num] = get_block_header(block_num);
   }
   return results;
}

optional<signed_block> database_api::get_block(uint32_t block_num) const {
   return my->get_block(block_num);
}

optional<signed_block> database_api_impl::get_block(uint32_t block_num) const {
   return _db.fetch_block_by_number(block_num);
}

vector<optional<signed_block>> database_api::get_blocks(uint32_t block_num_from, uint32_t block_num_to) const {
   return my->get_blocks(block_num_from, block_num_to);
}

vector<optional<signed_block>> database_api_impl::get_blocks(uint32_t block_num_from, uint32_t block_num_to) const {
   FC_ASSERT(block_num_to >= block_num_from && block_num_to - block_num_from <= 100, "Total blocks to be returned should be less than 100");
   vector<optional<signed_block>> res;
   for (uint32_t block_num = block_num_from; block_num <= block_num_to; block_num++) {
      res.push_back(_db.fetch_block_by_number(block_num));
   }
   return res;
}

processed_transaction database_api::get_transaction(uint32_t block_num, uint32_t trx_in_block) const {
   return my->get_transaction(block_num, trx_in_block);
}

optional<signed_transaction> database_api::get_recent_transaction_by_id(const transaction_id_type &id) const {
   try {
      return my->_db.get_recent_transaction(id);
   } catch (...) {
      return optional<signed_transaction>();
   }
}

processed_transaction database_api_impl::get_transaction(uint32_t block_num, uint32_t trx_num) const {
   auto opt_block = _db.fetch_block_by_number(block_num);
   FC_ASSERT(opt_block);
   FC_ASSERT(opt_block->transactions.size() > trx_num);
   return opt_block->transactions[trx_num];
}

//////////////////////////////////////////////////////////////////////
//                                                                  //
// Globals                                                          //
//                                                                  //
//////////////////////////////////////////////////////////////////////

version_info database_api::get_version_info() const {
   return my->get_version_info();
}

version_info database_api_impl::get_version_info() const {

   std::string witness_version(graphene::utilities::git_revision_description);
   const size_t pos = witness_version.find('/');
   if (pos != std::string::npos && witness_version.size() > pos)
      witness_version = witness_version.substr(pos + 1);

   version_info vi;
   vi.version = witness_version;
   vi.git_revision = graphene::utilities::git_revision_sha;
   vi.built = std::string(__DATE__) + " at " + std::string(__TIME__);
   vi.openssl = OPENSSL_VERSION_TEXT;
   vi.boost = boost::replace_all_copy(std::string(BOOST_LIB_VERSION), "_", ".");

   return vi;
}

chain_property_object database_api::get_chain_properties() const {
   return my->get_chain_properties();
}

chain_property_object database_api_impl::get_chain_properties() const {
   return _db.get(chain_property_id_type());
}

global_property_object database_api::get_global_properties() const {
   return my->get_global_properties();
}

global_property_object database_api_impl::get_global_properties() const {
   return _db.get(global_property_id_type());
}

fc::variant_object database_api::get_config() const {
   return my->get_config();
}

fc::variant_object database_api_impl::get_config() const {
   return graphene::chain::get_config();
}

chain_id_type database_api::get_chain_id() const {
   return my->get_chain_id();
}

chain_id_type database_api_impl::get_chain_id() const {
   return _db.get_chain_id();
}

dynamic_global_property_object database_api::get_dynamic_global_properties() const {
   return my->get_dynamic_global_properties();
}

dynamic_global_property_object database_api_impl::get_dynamic_global_properties() const {
   return _db.get(dynamic_global_property_id_type());
}

global_betting_statistics_object database_api::get_global_betting_statistics() const {
   return my->get_global_betting_statistics();
}

global_betting_statistics_object database_api_impl::get_global_betting_statistics() const {
   return _db.get(global_betting_statistics_id_type());
}

//////////////////////////////////////////////////////////////////////
//                                                                  //
// Keys                                                             //
//                                                                  //
//////////////////////////////////////////////////////////////////////

vector<vector<account_id_type>> database_api::get_key_references(vector<public_key_type> key) const {
   return my->get_key_references(key);
}

/**
 *  @return all accounts that referr to the key or account id in their owner or active authorities.
 */
vector<vector<account_id_type>> database_api_impl::get_key_references(vector<public_key_type> keys) const {
   wdump((keys));

   const auto &idx = _db.get_index_type<account_index>();
   const auto &aidx = dynamic_cast<const base_primary_index &>(idx);
   const auto &refs = aidx.get_secondary_index<graphene::chain::account_member_index>();

   vector<vector<account_id_type>> final_result;
   final_result.reserve(keys.size());

   for (auto &key : keys) {

      address a1(pts_address(key, false, 56));
      address a2(pts_address(key, true, 56));
      address a3(pts_address(key, false, 0));
      address a4(pts_address(key, true, 0));
      address a5(key);

      subscribe_to_item(key);
      subscribe_to_item(a1);
      subscribe_to_item(a2);
      subscribe_to_item(a3);
      subscribe_to_item(a4);
      subscribe_to_item(a5);

      vector<account_id_type> result;

      for (auto &a : {a1, a2, a3, a4, a5}) {
         auto itr = refs.account_to_address_memberships.find(a);
         if (itr != refs.account_to_address_memberships.end()) {
            result.reserve(result.size() + itr->second.size());
            for (auto item : itr->second) {
               wdump((a)(item)(item(_db).name));
               result.push_back(item);
            }
         }
      }

      auto itr = refs.account_to_key_memberships.find(key);
      if (itr != refs.account_to_key_memberships.end()) {
         result.reserve(result.size() + itr->second.size());
         for (auto item : itr->second)
            result.push_back(item);
      }
      final_result.emplace_back(std::move(result));
   }

   for (auto i : final_result)
      subscribe_to_item(i);

   return final_result;
}

bool database_api::is_public_key_registered(string public_key) const {
   return my->is_public_key_registered(public_key);
}

bool database_api_impl::is_public_key_registered(string public_key) const {
   // Short-circuit
   if (public_key.empty()) {
      return false;
   }

   // Search among all keys using an existing map of *current* account keys
   public_key_type key;
   try {
      key = public_key_type(public_key);
   } catch (...) {
      // An invalid public key was detected
      return false;
   }
   const auto &idx = _db.get_index_type<account_index>();
   const auto &aidx = dynamic_cast<const base_primary_index &>(idx);
   const auto &refs = aidx.get_secondary_index<graphene::chain::account_member_index>();
   auto itr = refs.account_to_key_memberships.find(key);
   bool is_known = itr != refs.account_to_key_memberships.end();

   return is_known;
}

//////////////////////////////////////////////////////////////////////
//                                                                  //
// Accounts                                                         //
//                                                                  //
//////////////////////////////////////////////////////////////////////

account_id_type database_api::get_account_id_from_string(const std::string &name_or_id) const {
   return my->get_account_from_string(name_or_id)->id;
}

vector<optional<account_object>> database_api::get_accounts(const vector<std::string> &account_names_or_ids) const {
   return my->get_accounts(account_names_or_ids);
}

vector<optional<account_object>> database_api_impl::get_accounts(const vector<std::string> &account_names_or_ids) const {
   vector<optional<account_object>> result;
   result.reserve(account_names_or_ids.size());
   std::transform(account_names_or_ids.begin(), account_names_or_ids.end(), std::back_inserter(result),
                  [this](std::string id_or_name) -> optional<account_object> {
                     const account_object *account = get_account_from_string(id_or_name, false);
                     if (account == nullptr)
                        return {};

                     subscribe_to_item(account->id);
                     return *account;
                  });
   return result;
}

std::map<string, full_account> database_api::get_full_accounts(const vector<string> &names_or_ids, bool subscribe) {
   return my->get_full_accounts(names_or_ids, subscribe);
}

std::map<std::string, full_account> database_api_impl::get_full_accounts(const vector<std::string> &names_or_ids, bool subscribe) {
   const auto &proposal_idx = _db.get_index_type<proposal_index>();
   const auto &pidx = dynamic_cast<const base_primary_index &>(proposal_idx);
   const auto &proposals_by_account = pidx.get_secondary_index<graphene::chain::required_approval_index>();

   std::map<std::string, full_account> results;

   for (const std::string &account_name_or_id : names_or_ids) {
      const account_object *account = nullptr;
      if (std::isdigit(account_name_or_id[0]))
         account = _db.find(fc::variant(account_name_or_id, 1).as<account_id_type>(1));
      else {
         const auto &idx = _db.get_index_type<account_index>().indices().get<by_name>();
         auto itr = idx.find(account_name_or_id);
         if (itr != idx.end())
            account = &*itr;
      }
      if (account == nullptr)
         continue;

      if (subscribe) {
         FC_ASSERT(std::distance(_subscribed_accounts.begin(), _subscribed_accounts.end()) <= 100);
         _subscribed_accounts.insert(account->get_id());
         subscribe_to_item(account->id);
      }

      full_account acnt;
      acnt.account = *account;
      acnt.statistics = account->statistics(_db);
      acnt.registrar_name = account->registrar(_db).name;
      acnt.referrer_name = account->referrer(_db).name;
      acnt.lifetime_referrer_name = account->lifetime_referrer(_db).name;
      acnt.votes = lookup_vote_ids(vector<vote_id_type>(account->options.votes.begin(), account->options.votes.end()));

      if (account->cashback_vb) {
         acnt.cashback_balance = account->cashback_balance(_db);
      }
      // Add the account's proposals
      auto required_approvals_itr = proposals_by_account._account_to_proposals.find(account->id);
      if (required_approvals_itr != proposals_by_account._account_to_proposals.end()) {
         acnt.proposals.reserve(required_approvals_itr->second.size());
         for (auto proposal_id : required_approvals_itr->second)
            acnt.proposals.push_back(proposal_id(_db));
      }

      // Add the account's balances
      const auto &balances = _db.get_index_type<primary_index<account_balance_index>>().get_secondary_index<balances_by_account_index>().get_account_balances(account->id);
      for (const auto balance : balances)
         acnt.balances.emplace_back(*balance.second);

      // Add the account's vesting balances
      auto vesting_range = _db.get_index_type<vesting_balance_index>().indices().get<by_account>().equal_range(account->id);
      std::for_each(vesting_range.first, vesting_range.second,
                    [&acnt](const vesting_balance_object &balance) {
                       acnt.vesting_balances.emplace_back(balance);
                    });

      // Add the account's orders
      auto order_range = _db.get_index_type<limit_order_index>().indices().get<by_account>().equal_range(account->id);
      std::for_each(order_range.first, order_range.second,
                    [&acnt](const limit_order_object &order) {
                       acnt.limit_orders.emplace_back(order);
                    });
      auto call_range = _db.get_index_type<call_order_index>().indices().get<by_account>().equal_range(account->id);
      std::for_each(call_range.first, call_range.second,
                    [&acnt](const call_order_object &call) {
                       acnt.call_orders.emplace_back(call);
                    });
      auto settle_range = _db.get_index_type<force_settlement_index>().indices().get<by_account>().equal_range(account->id);
      std::for_each(settle_range.first, settle_range.second,
                    [&acnt](const force_settlement_object &settle) {
                       acnt.settle_orders.emplace_back(settle);
                    });

      // get assets issued by user
      auto asset_range = _db.get_index_type<asset_index>().indices().get<by_issuer>().equal_range(account->id);
      std::for_each(asset_range.first, asset_range.second,
                    [&acnt](const asset_object &asset) {
                       acnt.assets.emplace_back(asset.id);
                    });

      // get withdraws permissions
      auto withdraw_range = _db.get_index_type<withdraw_permission_index>().indices().get<by_from>().equal_range(account->id);
      std::for_each(withdraw_range.first, withdraw_range.second,
                    [&acnt](const withdraw_permission_object &withdraw) {
                       acnt.withdraws.emplace_back(withdraw);
                    });

      auto pending_payouts_range =
            _db.get_index_type<pending_dividend_payout_balance_for_holder_object_index>().indices().get<by_account_dividend_payout>().equal_range(boost::make_tuple(account->id));

      std::copy(pending_payouts_range.first, pending_payouts_range.second, std::back_inserter(acnt.pending_dividend_payments));

      results[account_name_or_id] = acnt;
   }
   return results;
}

optional<account_object> database_api::get_account_by_name(string name) const {
   return my->get_account_by_name(name);
}

optional<account_object> database_api_impl::get_account_by_name(string name) const {
   const auto &idx = _db.get_index_type<account_index>().indices().get<by_name>();
   auto itr = idx.find(name);
   if (itr != idx.end())
      return *itr;
   return optional<account_object>();
}

vector<account_id_type> database_api::get_account_references(const std::string account_id_or_name) const {
   return my->get_account_references(account_id_or_name);
}

vector<account_id_type> database_api_impl::get_account_references(const std::string account_id_or_name) const {
   const auto &idx = _db.get_index_type<account_index>();
   const auto &aidx = dynamic_cast<const base_primary_index &>(idx);
   const auto &refs = aidx.get_secondary_index<graphene::chain::account_member_index>();
   const account_id_type account_id = get_account_from_string(account_id_or_name)->id;
   auto itr = refs.account_to_account_memberships.find(account_id);
   vector<account_id_type> result;

   if (itr != refs.account_to_account_memberships.end()) {
      result.reserve(itr->second.size());
      for (auto item : itr->second)
         result.push_back(item);
   }
   return result;
}

vector<optional<account_object>> database_api::lookup_account_names(const vector<string> &account_names) const {
   return my->lookup_account_names(account_names);
}

vector<optional<account_object>> database_api_impl::lookup_account_names(const vector<string> &account_names) const {
   const auto &accounts_by_name = _db.get_index_type<account_index>().indices().get<by_name>();
   vector<optional<account_object>> result;
   result.reserve(account_names.size());
   std::transform(account_names.begin(), account_names.end(), std::back_inserter(result),
                  [&accounts_by_name](const string &name) -> optional<account_object> {
                     auto itr = accounts_by_name.find(name);
                     return itr == accounts_by_name.end() ? optional<account_object>() : *itr;
                  });
   return result;
}

map<string, account_id_type> database_api::lookup_accounts(const string &lower_bound_name, uint32_t limit) const {
   return my->lookup_accounts(lower_bound_name, limit);
}

map<string, account_id_type> database_api_impl::lookup_accounts(const string &lower_bound_name, uint32_t limit) const {
   FC_ASSERT(limit <= api_limit_lookup_accounts,
             "Number of querying accounts can not be greater than ${configured_limit}",
             ("configured_limit", api_limit_lookup_accounts));
   const auto &accounts_by_name = _db.get_index_type<account_index>().indices().get<by_name>();
   map<string, account_id_type> result;

   for (auto itr = accounts_by_name.lower_bound(lower_bound_name);
        limit-- && itr != accounts_by_name.end();
        ++itr) {
      result.insert(make_pair(itr->name, itr->get_id()));
      if (limit == 1)
         subscribe_to_item(itr->get_id());
   }

   return result;
}

uint64_t database_api::get_account_count() const {
   return my->get_account_count();
}

uint64_t database_api_impl::get_account_count() const {
   return _db.get_index_type<account_index>().indices().size();
}

//////////////////////////////////////////////////////////////////////
//                                                                  //
// Balances                                                         //
//                                                                  //
//////////////////////////////////////////////////////////////////////

vector<asset> database_api::get_account_balances(const std::string &account_name_or_id, const flat_set<asset_id_type> &assets) const {
   return my->get_account_balances(account_name_or_id, assets);
}

vector<asset> database_api_impl::get_account_balances(const std::string &account_name_or_id,
                                                      const flat_set<asset_id_type> &assets) const {
   const account_object *account = get_account_from_string(account_name_or_id);
   account_id_type acnt = account->id;
   vector<asset> result;
   if (assets.empty()) {
      // if the caller passes in an empty list of assets, return balances for all assets the account owns
      const auto &balance_index = _db.get_index_type<primary_index<account_balance_index>>();
      const auto &balances = balance_index.get_secondary_index<balances_by_account_index>().get_account_balances(acnt);
      for (const auto balance : balances)
         result.push_back(balance.second->get_balance());
   } else {
      result.reserve(assets.size());

      std::transform(assets.begin(), assets.end(), std::back_inserter(result),
                     [this, acnt](asset_id_type id) {
                        return _db.get_balance(acnt, id);
                     });
   }

   return result;
}

vector<asset> database_api::get_named_account_balances(const std::string &name, const flat_set<asset_id_type> &assets) const {
   return my->get_account_balances(name, assets);
}

vector<balance_object> database_api::get_balance_objects(const vector<address> &addrs) const {
   return my->get_balance_objects(addrs);
}

vector<balance_object> database_api_impl::get_balance_objects(const vector<address> &addrs) const {
   try {
      const auto &bal_idx = _db.get_index_type<balance_index>();
      const auto &by_owner_idx = bal_idx.indices().get<by_owner>();

      vector<balance_object> result;

      for (const auto &owner : addrs) {
         subscribe_to_item(owner);
         auto itr = by_owner_idx.lower_bound(boost::make_tuple(owner, asset_id_type(0)));
         while (itr != by_owner_idx.end() && itr->owner == owner) {
            result.push_back(*itr);
            ++itr;
         }
      }
      return result;
   }
   FC_CAPTURE_AND_RETHROW((addrs))
}

vector<asset> database_api::get_vested_balances(const vector<balance_id_type> &objs) const {
   return my->get_vested_balances(objs);
}

vector<asset> database_api_impl::get_vested_balances(const vector<balance_id_type> &objs) const {
   try {
      vector<asset> result;
      result.reserve(objs.size());
      auto now = _db.head_block_time();
      for (auto obj : objs)
         result.push_back(obj(_db).available(now));
      return result;
   }
   FC_CAPTURE_AND_RETHROW((objs))
}

vector<vesting_balance_object> database_api::get_vesting_balances(const std::string account_id_or_name) const {
   return my->get_vesting_balances(account_id_or_name);
}

vector<vesting_balance_object> database_api_impl::get_vesting_balances(const std::string account_id_or_name) const {
   try {
      const account_id_type account_id = get_account_from_string(account_id_or_name)->id;
      vector<vesting_balance_object> result;
      auto vesting_range = _db.get_index_type<vesting_balance_index>().indices().get<by_account>().equal_range(account_id);
      std::for_each(vesting_range.first, vesting_range.second,
                    [&result](const vesting_balance_object &balance) {
                       if (balance.balance.amount > 0)
                          result.emplace_back(balance);
                    });
      return result;
   }
   FC_CAPTURE_AND_RETHROW((account_id_or_name));
}

//////////////////////////////////////////////////////////////////////
//                                                                  //
// Assets                                                           //
//                                                                  //
//////////////////////////////////////////////////////////////////////

asset_id_type database_api::get_asset_id_from_string(const std::string &symbol_or_id) const {
   return my->get_asset_from_string(symbol_or_id)->id;
}

const asset_object *database_api_impl::get_asset_from_string(const std::string &symbol_or_id,
                                                             bool throw_if_not_found) const {
   // TODO cache the result to avoid repeatly fetching from db
   FC_ASSERT(symbol_or_id.size() > 0);
   const asset_object *asset = nullptr;
   if (std::isdigit(symbol_or_id[0]))
      asset = _db.find(fc::variant(symbol_or_id, 1).as<asset_id_type>(1));
   else {
      const auto &idx = _db.get_index_type<asset_index>().indices().get<by_symbol>();
      auto itr = idx.find(symbol_or_id);
      if (itr != idx.end())
         asset = &*itr;
   }
   if (throw_if_not_found)
      FC_ASSERT(asset, "no such asset");
   return asset;
}

vector<optional<asset_object>> database_api::get_assets(const vector<std::string> &asset_symbols_or_ids) const {
   return my->get_assets(asset_symbols_or_ids);
}

vector<optional<asset_object>> database_api_impl::get_assets(const vector<std::string> &asset_symbols_or_ids) const {
   vector<optional<asset_object>> result;
   result.reserve(asset_symbols_or_ids.size());
   std::transform(asset_symbols_or_ids.begin(), asset_symbols_or_ids.end(), std::back_inserter(result),
                  [this](std::string id_or_name) -> optional<asset_object> {
                     const asset_object *asset_obj = get_asset_from_string(id_or_name, false);
                     if (asset_obj == nullptr)
                        return {};
                     subscribe_to_item(asset_obj->id);
                     return asset_object(*asset_obj);
                  });
   return result;
}

vector<optional<asset_object>> database_api_impl::get_assets(const vector<asset_id_type> &asset_ids) const {
   vector<optional<asset_object>> result;
   result.reserve(asset_ids.size());
   std::transform(asset_ids.begin(), asset_ids.end(), std::back_inserter(result),
                  [this](asset_id_type id) -> optional<asset_object> {
                     if (auto o = _db.find(id)) {
                        subscribe_to_item(id);
                        return *o;
                     }
                     return {};
                  });
   return result;
}

vector<asset_object> database_api::list_assets(const string &lower_bound_symbol, uint32_t limit) const {
   return my->list_assets(lower_bound_symbol, limit);
}

vector<asset_object> database_api_impl::list_assets(const string &lower_bound_symbol, uint32_t limit) const {
   FC_ASSERT(limit <= api_limit_get_lower_bound_symbol,
             "Number of querying accounts can not be greater than ${configured_limit}",
             ("configured_limit", api_limit_get_lower_bound_symbol));
   const auto &assets_by_symbol = _db.get_index_type<asset_index>().indices().get<by_symbol>();
   vector<asset_object> result;
   result.reserve(limit);

   auto itr = assets_by_symbol.lower_bound(lower_bound_symbol);

   if (lower_bound_symbol == "")
      itr = assets_by_symbol.begin();

   while (limit-- && itr != assets_by_symbol.end())
      result.emplace_back(*itr++);

   return result;
}

vector<optional<asset_object>> database_api::lookup_asset_symbols(const vector<string> &symbols_or_ids) const {
   return my->lookup_asset_symbols(symbols_or_ids);
}

vector<optional<asset_object>> database_api_impl::lookup_asset_symbols(const vector<string> &symbols_or_ids) const {
   const auto &assets_by_symbol = _db.get_index_type<asset_index>().indices().get<by_symbol>();
   vector<optional<asset_object>> result;
   result.reserve(symbols_or_ids.size());
   std::transform(symbols_or_ids.begin(), symbols_or_ids.end(), std::back_inserter(result),
                  [this, &assets_by_symbol](const string &symbol_or_id) -> optional<asset_object> {
                     if (!symbol_or_id.empty() && std::isdigit(symbol_or_id[0])) {
                        auto ptr = _db.find(variant(symbol_or_id, 1).as<asset_id_type>(1));
                        return ptr == nullptr ? optional<asset_object>() : *ptr;
                     }
                     auto itr = assets_by_symbol.find(symbol_or_id);
                     return itr == assets_by_symbol.end() ? optional<asset_object>() : *itr;
                  });
   return result;
}

uint64_t database_api::get_asset_count() const {
   return my->get_asset_count();
}

uint64_t database_api_impl::get_asset_count() const {
   return _db.get_index_type<asset_index>().indices().size();
}
////////////////////
// Lottery Assets //
////////////////////

vector<asset_object> database_api::get_lotteries(asset_id_type stop,
                                                 unsigned limit,
                                                 asset_id_type start) const {
   return my->get_lotteries(stop, limit, start);
}
vector<asset_object> database_api_impl::get_lotteries(asset_id_type stop,
                                                      unsigned limit,
                                                      asset_id_type start) const {
   vector<asset_object> result;
   if (limit > 100)
      limit = 100;
   const auto &assets = _db.get_index_type<asset_index>().indices().get<by_lottery>();

   const auto range = assets.equal_range(boost::make_tuple(true));
   for (const auto &a : boost::make_iterator_range(range.first, range.second)) {
      if (start == asset_id_type() || (a.get_id().instance.value <= start.instance.value))
         result.push_back(a);
      if (a.get_id().instance.value < stop.instance.value || result.size() >= limit)
         break;
   }

   return result;
}
vector<asset_object> database_api::get_account_lotteries(account_id_type issuer,
                                                         asset_id_type stop,
                                                         unsigned limit,
                                                         asset_id_type start) const {
   return my->get_account_lotteries(issuer, stop, limit, start);
}

vector<asset_object> database_api_impl::get_account_lotteries(account_id_type issuer,
                                                              asset_id_type stop,
                                                              unsigned limit,
                                                              asset_id_type start) const {
   vector<asset_object> result;
   if (limit > 100)
      limit = 100;
   const auto &assets = _db.get_index_type<asset_index>().indices().get<by_lottery_owner>();

   const auto range = assets.equal_range(boost::make_tuple(true, issuer.instance.value));
   for (const auto &a : boost::make_iterator_range(range.first, range.second)) {
      if (start == asset_id_type() || (a.get_id().instance.value <= start.instance.value))
         result.push_back(a);
      if (a.get_id().instance.value < stop.instance.value || result.size() >= limit)
         break;
   }

   return result;
}

asset database_api::get_lottery_balance(asset_id_type lottery_id) const {
   return my->get_lottery_balance(lottery_id);
}

asset database_api_impl::get_lottery_balance(asset_id_type lottery_id) const {
   auto lottery_asset = lottery_id(_db);
   FC_ASSERT(lottery_asset.is_lottery());
   return _db.get_balance(lottery_id);
}

sweeps_vesting_balance_object database_api::get_sweeps_vesting_balance_object(account_id_type account) const {
   return my->get_sweeps_vesting_balance_object(account);
}

sweeps_vesting_balance_object database_api_impl::get_sweeps_vesting_balance_object(account_id_type account) const {
   const auto &vesting_idx = _db.get_index_type<sweeps_vesting_balance_index>().indices().get<by_owner>();
   auto account_balance = vesting_idx.find(account);
   FC_ASSERT(account_balance != vesting_idx.end(), "NO SWEEPS VESTING BALANCE");
   return *account_balance;
}

asset database_api::get_sweeps_vesting_balance_available_for_claim(account_id_type account) const {
   return my->get_sweeps_vesting_balance_available_for_claim(account);
}

asset database_api_impl::get_sweeps_vesting_balance_available_for_claim(account_id_type account) const {
   const auto &vesting_idx = _db.get_index_type<sweeps_vesting_balance_index>().indices().get<by_owner>();
   auto account_balance = vesting_idx.find(account);
   FC_ASSERT(account_balance != vesting_idx.end(), "NO SWEEPS VESTING BALANCE");
   return account_balance->available_for_claim();
}

//////////////////////////////////////////////////////////////////////
// Peerplays                                                        //
//////////////////////////////////////////////////////////////////////
vector<sport_object> database_api::list_sports() const {
   return my->list_sports();
}

vector<sport_object> database_api_impl::list_sports() const {
   const auto &sport_object_idx = _db.get_index_type<sport_object_index>().indices().get<by_id>();
   return boost::copy_range<vector<sport_object>>(sport_object_idx);
}

vector<event_group_object> database_api::list_event_groups(sport_id_type sport_id) const {
   return my->list_event_groups(sport_id);
}

vector<event_group_object> database_api_impl::list_event_groups(sport_id_type sport_id) const {
   const auto &event_group_idx = _db.get_index_type<event_group_object_index>().indices().get<by_sport_id>();
   return boost::copy_range<vector<event_group_object>>(event_group_idx.equal_range(sport_id));
}

vector<event_object> database_api::list_events_in_group(event_group_id_type event_group_id) const {
   return my->list_events_in_group(event_group_id);
}

vector<event_object> database_api_impl::list_events_in_group(event_group_id_type event_group_id) const {
   const auto &event_idx = _db.get_index_type<event_object_index>().indices().get<by_event_group_id>();
   return boost::copy_range<vector<event_object>>(event_idx.equal_range(event_group_id));
}

vector<betting_market_group_object> database_api::list_betting_market_groups(event_id_type event_id) const {
   return my->list_betting_market_groups(event_id);
}

vector<betting_market_group_object> database_api_impl::list_betting_market_groups(event_id_type event_id) const {
   const auto &betting_market_group_idx = _db.get_index_type<betting_market_group_object_index>().indices().get<by_event_id>();
   return boost::copy_range<vector<betting_market_group_object>>(betting_market_group_idx.equal_range(event_id));
}

vector<betting_market_object> database_api::list_betting_markets(betting_market_group_id_type betting_market_group_id) const {
   return my->list_betting_markets(betting_market_group_id);
}

vector<betting_market_object> database_api_impl::list_betting_markets(betting_market_group_id_type betting_market_group_id) const {
   const auto &betting_market_idx = _db.get_index_type<betting_market_object_index>().indices().get<by_betting_market_group_id>();
   return boost::copy_range<vector<betting_market_object>>(betting_market_idx.equal_range(betting_market_group_id));
}

vector<bet_object> database_api::get_unmatched_bets_for_bettor(betting_market_id_type betting_market_id, account_id_type bettor_id) const {
   return my->get_unmatched_bets_for_bettor(betting_market_id, bettor_id);
}

vector<bet_object> database_api_impl::get_unmatched_bets_for_bettor(betting_market_id_type betting_market_id, account_id_type bettor_id) const {
   const auto &bet_idx = _db.get_index_type<bet_object_index>().indices().get<by_bettor_and_odds>();
   return boost::copy_range<vector<bet_object>>(bet_idx.equal_range(std::make_tuple(bettor_id, betting_market_id)));
}

vector<bet_object> database_api::get_all_unmatched_bets_for_bettor(account_id_type bettor_id) const {
   return my->get_all_unmatched_bets_for_bettor(bettor_id);
}

vector<bet_object> database_api_impl::get_all_unmatched_bets_for_bettor(account_id_type bettor_id) const {
   const auto &bet_idx = _db.get_index_type<bet_object_index>().indices().get<by_bettor_and_odds>();
   return boost::copy_range<vector<bet_object>>(bet_idx.equal_range(std::make_tuple(bettor_id)));
}

//////////////////////////////////////////////////////////////////////
//                                                                  //
// Markets / feeds                                                  //
//                                                                  //
//////////////////////////////////////////////////////////////////////

vector<limit_order_object> database_api::get_limit_orders(const std::string &a, const std::string &b, const uint32_t limit) const {
   return my->get_limit_orders(a, b, limit);
}

/**
 *  @return the limit orders for both sides of the book for the two assets specified up to limit number on each side.
 */
vector<limit_order_object> database_api_impl::get_limit_orders(const std::string &a, const std::string &b, const uint32_t limit) const {
   const asset_id_type asset_a_id = get_asset_from_string(a)->id;
   const asset_id_type asset_b_id = get_asset_from_string(b)->id;

   return get_limit_orders(asset_a_id, asset_b_id, limit);
}

vector<limit_order_object> database_api_impl::get_limit_orders(const asset_id_type a, const asset_id_type b,
                                                               const uint32_t limit) const {
   const auto &limit_order_idx = _db.get_index_type<limit_order_index>();
   const auto &limit_price_idx = limit_order_idx.indices().get<by_price>();

   vector<limit_order_object> result;
   result.reserve(limit * 2);

   uint32_t count = 0;
   auto limit_itr = limit_price_idx.lower_bound(price::max(a, b));
   auto limit_end = limit_price_idx.upper_bound(price::min(a, b));
   while (limit_itr != limit_end && count < limit) {
      result.push_back(*limit_itr);
      ++limit_itr;
      ++count;
   }
   count = 0;
   limit_itr = limit_price_idx.lower_bound(price::max(b, a));
   limit_end = limit_price_idx.upper_bound(price::min(b, a));
   while (limit_itr != limit_end && count < limit) {
      result.push_back(*limit_itr);
      ++limit_itr;
      ++count;
   }

   return result;
}

vector<call_order_object> database_api::get_call_orders(const std::string &a, uint32_t limit) const {
   return my->get_call_orders(a, limit);
}

vector<call_order_object> database_api_impl::get_call_orders(const std::string &a, uint32_t limit) const {
   const auto &call_index = _db.get_index_type<call_order_index>().indices().get<by_price>();
   const asset_object *mia = get_asset_from_string(a);
   price index_price = price::min(mia->bitasset_data(_db).options.short_backing_asset, mia->get_id());

   return vector<call_order_object>(call_index.lower_bound(index_price.min()),
                                    call_index.lower_bound(index_price.max()));
}

vector<force_settlement_object> database_api::get_settle_orders(const std::string &a, uint32_t limit) const {
   return my->get_settle_orders(a, limit);
}

vector<force_settlement_object> database_api_impl::get_settle_orders(const std::string &a, uint32_t limit) const {
   const auto &settle_index = _db.get_index_type<force_settlement_index>().indices().get<by_expiration>();
   const asset_object *mia = get_asset_from_string(a);
   return vector<force_settlement_object>(settle_index.lower_bound(mia->get_id()),
                                          settle_index.upper_bound(mia->get_id()));
}

vector<call_order_object> database_api::get_margin_positions(const std::string account_id_or_name) const {
   return my->get_margin_positions(account_id_or_name);
}

vector<call_order_object> database_api_impl::get_margin_positions(const std::string account_id_or_name) const {
   try {
      const auto &idx = _db.get_index_type<call_order_index>();
      const auto &aidx = idx.indices().get<by_account>();
      const account_id_type id = get_account_from_string(account_id_or_name)->id;
      auto start = aidx.lower_bound(boost::make_tuple(id, asset_id_type(0)));
      auto end = aidx.lower_bound(boost::make_tuple(id + 1, asset_id_type(0)));
      vector<call_order_object> result;
      while (start != end) {
         result.push_back(*start);
         ++start;
      }
      return result;
   }
   FC_CAPTURE_AND_RETHROW((account_id_or_name))
}

void database_api::subscribe_to_market(std::function<void(const variant &)> callback, const std::string &a, const std::string &b) {
   my->subscribe_to_market(callback, a, b);
}

void database_api_impl::subscribe_to_market(std::function<void(const variant &)> callback, const std::string &a, const std::string &b) {
   auto asset_a_id = get_asset_from_string(a)->id;
   auto asset_b_id = get_asset_from_string(b)->id;

   if (asset_a_id > asset_b_id)
      std::swap(asset_a_id, asset_b_id);
   FC_ASSERT(asset_a_id != asset_b_id);
   _market_subscriptions[std::make_pair(asset_a_id, asset_b_id)] = callback;
}

void database_api::unsubscribe_from_market(const std::string &a, const std::string &b) {
   my->unsubscribe_from_market(a, b);
}

void database_api_impl::unsubscribe_from_market(const std::string &a, const std::string &b) {
   auto asset_a_id = get_asset_from_string(a)->id;
   auto asset_b_id = get_asset_from_string(b)->id;

   if (asset_a_id > asset_b_id)
      std::swap(asset_a_id, asset_b_id);
   FC_ASSERT(asset_a_id != asset_b_id);
   _market_subscriptions.erase(std::make_pair(asset_a_id, asset_b_id));
}

market_ticker database_api::get_ticker(const string &base, const string &quote) const {
   return my->get_ticker(base, quote);
}

market_ticker database_api_impl::get_ticker(const string &base, const string &quote) const {
   const auto assets = lookup_asset_symbols({base, quote});
   FC_ASSERT(assets[0], "Invalid base asset symbol: ${s}", ("s", base));
   FC_ASSERT(assets[1], "Invalid quote asset symbol: ${s}", ("s", quote));

   market_ticker result;
   result.base = base;
   result.quote = quote;
   result.latest = 0;
   result.lowest_ask = 0;
   result.highest_bid = 0;
   result.percent_change = 0;
   result.base_volume = 0;
   result.quote_volume = 0;

   try {
      const fc::time_point_sec now = fc::time_point::now();
      const fc::time_point_sec yesterday = fc::time_point_sec(now.sec_since_epoch() - 86400);
      const auto batch_size = 100;

      vector<market_trade> trades = get_trade_history(base, quote, now, yesterday, batch_size);
      if (!trades.empty()) {
         result.latest = trades[0].price;

         while (!trades.empty()) {
            for (const market_trade &t : trades) {
               result.base_volume += t.value;
               result.quote_volume += t.amount;
            }

            trades = get_trade_history(base, quote, trades.back().date, yesterday, batch_size);
         }

         const auto last_trade_yesterday = get_trade_history(base, quote, yesterday, fc::time_point_sec(), 1);
         if (!last_trade_yesterday.empty()) {
            const auto price_yesterday = last_trade_yesterday[0].price;
            result.percent_change = ((result.latest / price_yesterday) - 1) * 100;
         }
      } else {
         const auto last_trade = get_trade_history(base, quote, now, fc::time_point_sec(), 1);
         if (!last_trade.empty())
            result.latest = last_trade[0].price;
      }

      const auto orders = get_order_book(base, quote, 1);
      if (!orders.asks.empty())
         result.lowest_ask = orders.asks[0].price;
      if (!orders.bids.empty())
         result.highest_bid = orders.bids[0].price;
   }
   FC_CAPTURE_AND_RETHROW((base)(quote))

   return result;
}

market_volume database_api::get_24_volume(const string &base, const string &quote) const {
   return my->get_24_volume(base, quote);
}

market_volume database_api_impl::get_24_volume(const string &base, const string &quote) const {
   const auto ticker = get_ticker(base, quote);

   market_volume result;
   result.base = ticker.base;
   result.quote = ticker.quote;
   result.base_volume = ticker.base_volume;
   result.quote_volume = ticker.quote_volume;

   return result;
}

order_book database_api::get_order_book(const string &base, const string &quote, unsigned limit) const {
   return my->get_order_book(base, quote, limit);
}

order_book database_api_impl::get_order_book(const string &base, const string &quote, unsigned limit) const {
   using boost::multiprecision::uint128_t;
   FC_ASSERT(limit <= api_limit_get_order_book,
             "Number of querying accounts can not be greater than ${configured_limit}",
             ("configured_limit", api_limit_get_order_book));

   order_book result;
   result.base = base;
   result.quote = quote;

   auto assets = lookup_asset_symbols({base, quote});
   FC_ASSERT(assets[0], "Invalid base asset symbol: ${s}", ("s", base));
   FC_ASSERT(assets[1], "Invalid quote asset symbol: ${s}", ("s", quote));

   auto base_id = assets[0]->id;
   auto quote_id = assets[1]->id;
   auto orders = get_limit_orders(base_id, quote_id, limit);

   auto asset_to_real = [&](const asset &a, int p) {
      return double(a.amount.value) / pow(10, p);
   };
   auto price_to_real = [&](const price &p) {
      if (p.base.asset_id == base_id)
         return asset_to_real(p.base, assets[0]->precision) / asset_to_real(p.quote, assets[1]->precision);
      else
         return asset_to_real(p.quote, assets[0]->precision) / asset_to_real(p.base, assets[1]->precision);
   };

   for (const auto &o : orders) {
      if (o.sell_price.base.asset_id == base_id) {
         order ord;
         ord.price = price_to_real(o.sell_price);
         ord.quote = asset_to_real(share_type((uint128_t(o.for_sale.value) * o.sell_price.quote.amount.value) / o.sell_price.base.amount.value), assets[1]->precision);
         ord.base = asset_to_real(o.for_sale, assets[0]->precision);
         result.bids.push_back(ord);
      } else {
         order ord;
         ord.price = price_to_real(o.sell_price);
         ord.quote = asset_to_real(o.for_sale, assets[1]->precision);
         ord.base = asset_to_real(share_type((uint128_t(o.for_sale.value) * o.sell_price.quote.amount.value) / o.sell_price.base.amount.value), assets[0]->precision);
         result.asks.push_back(ord);
      }
   }

   return result;
}

vector<market_trade> database_api::get_trade_history(const string &base,
                                                     const string &quote,
                                                     fc::time_point_sec start,
                                                     fc::time_point_sec stop,
                                                     unsigned limit) const {
   return my->get_trade_history(base, quote, start, stop, limit);
}

vector<market_trade> database_api_impl::get_trade_history(const string &base,
                                                          const string &quote,
                                                          fc::time_point_sec start,
                                                          fc::time_point_sec stop,
                                                          unsigned limit) const {
   FC_ASSERT(limit <= api_limit_get_trade_history,
             "Number of querying accounts can not be greater than ${configured_limit}",
             ("configured_limit", api_limit_get_trade_history));

   auto assets = lookup_asset_symbols({base, quote});
   FC_ASSERT(assets[0], "Invalid base asset symbol: ${s}", ("s", base));
   FC_ASSERT(assets[1], "Invalid quote asset symbol: ${s}", ("s", quote));

   auto base_id = assets[0]->id;
   auto quote_id = assets[1]->id;

   if (base_id > quote_id)
      std::swap(base_id, quote_id);
   const auto &history_idx = _db.get_index_type<graphene::market_history::history_index>().indices().get<by_key>();
   history_key hkey;
   hkey.base = base_id;
   hkey.quote = quote_id;
   hkey.sequence = std::numeric_limits<int64_t>::min();

   auto price_to_real = [&](const share_type a, int p) {
      return double(a.value) / pow(10, p);
   };

   if (start.sec_since_epoch() == 0)
      start = fc::time_point_sec(fc::time_point::now());

   uint32_t count = 0;
   auto itr = history_idx.lower_bound(hkey);
   vector<market_trade> result;

   while (itr != history_idx.end() && count < limit && !(itr->key.base != base_id || itr->key.quote != quote_id || itr->time < stop)) {
      if (itr->time < start) {
         market_trade trade;

         if (assets[0]->id == itr->op.receives.asset_id) {
            trade.amount = price_to_real(itr->op.pays.amount, assets[1]->precision);
            trade.value = price_to_real(itr->op.receives.amount, assets[0]->precision);
         } else {
            trade.amount = price_to_real(itr->op.receives.amount, assets[1]->precision);
            trade.value = price_to_real(itr->op.pays.amount, assets[0]->precision);
         }

         trade.date = itr->time;
         trade.price = trade.value / trade.amount;

         result.push_back(trade);
         ++count;
      }

      // Trades are tracked in each direction.
      ++itr;
      ++itr;
   }

   return result;
}

//////////////////////////////////////////////////////////////////////
//                                                                  //
// Witnesses                                                        //
//                                                                  //
//////////////////////////////////////////////////////////////////////

vector<optional<witness_object>> database_api::get_witnesses(const vector<witness_id_type> &witness_ids) const {
   return my->get_witnesses(witness_ids);
}

vector<optional<witness_object>> database_api_impl::get_witnesses(const vector<witness_id_type> &witness_ids) const {
   vector<optional<witness_object>> result;
   result.reserve(witness_ids.size());
   std::transform(witness_ids.begin(), witness_ids.end(), std::back_inserter(result),
                  [this](witness_id_type id) -> optional<witness_object> {
                     if (auto o = _db.find(id))
                        return *o;
                     return {};
                  });
   return result;
}

fc::optional<witness_object> database_api::get_witness_by_account_id(account_id_type account) const {
   return my->get_witness_by_account_id(account);
}

fc::optional<witness_object> database_api_impl::get_witness_by_account_id(account_id_type account) const {
   const auto &idx = _db.get_index_type<witness_index>().indices().get<by_account>();
   auto itr = idx.find(account);
   if (itr != idx.end())
      return *itr;
   return {};
}

fc::optional<witness_object> database_api::get_witness_by_account(const std::string account_id_or_name) const {
   return my->get_witness_by_account(account_id_or_name);
}

fc::optional<witness_object> database_api_impl::get_witness_by_account(const std::string account_id_or_name) const {
   const account_id_type account = get_account_from_string(account_id_or_name)->id;
   return get_witness_by_account_id(account);
}

map<string, witness_id_type> database_api::lookup_witness_accounts(const string &lower_bound_name, uint32_t limit) const {
   return my->lookup_witness_accounts(lower_bound_name, limit);
}

map<string, witness_id_type> database_api_impl::lookup_witness_accounts(const string &lower_bound_name, uint32_t limit) const {
   FC_ASSERT(limit <= api_limit_lookup_witness_accounts,
             "Number of querying accounts can not be greater than ${configured_limit}",
             ("configured_limit", api_limit_lookup_witness_accounts));
   const auto &witnesses_by_id = _db.get_index_type<witness_index>().indices().get<by_id>();

   // we want to order witnesses by account name, but that name is in the account object
   // so the witness_index doesn't have a quick way to access it.
   // get all the names and look them all up, sort them, then figure out what
   // records to return.  This could be optimized, but we expect the
   // number of witnesses to be few and the frequency of calls to be rare
   std::map<std::string, witness_id_type> witnesses_by_account_name;
   for (const witness_object &witness : witnesses_by_id)
      if (auto account_iter = _db.find(witness.witness_account))
         if (account_iter->name >= lower_bound_name) // we can ignore anything below lower_bound_name
            witnesses_by_account_name.insert(std::make_pair(account_iter->name, witness.id));

   auto end_iter = witnesses_by_account_name.begin();
   while (end_iter != witnesses_by_account_name.end() && limit--)
      ++end_iter;
   witnesses_by_account_name.erase(end_iter, witnesses_by_account_name.end());
   return witnesses_by_account_name;
}

uint64_t database_api::get_witness_count() const {
   return my->get_witness_count();
}

uint64_t database_api_impl::get_witness_count() const {
   return _db.get_index_type<witness_index>().indices().size();
}

//////////////////////////////////////////////////////////////////////
//                                                                  //
// Committee members                                                //
//                                                                  //
//////////////////////////////////////////////////////////////////////

vector<optional<committee_member_object>> database_api::get_committee_members(const vector<committee_member_id_type> &committee_member_ids) const {
   return my->get_committee_members(committee_member_ids);
}

vector<optional<committee_member_object>> database_api_impl::get_committee_members(const vector<committee_member_id_type> &committee_member_ids) const {
   vector<optional<committee_member_object>> result;
   result.reserve(committee_member_ids.size());
   std::transform(committee_member_ids.begin(), committee_member_ids.end(), std::back_inserter(result),
                  [this](committee_member_id_type id) -> optional<committee_member_object> {
                     if (auto o = _db.find(id))
                        return *o;
                     return {};
                  });
   return result;
}

fc::optional<committee_member_object> database_api::get_committee_member_by_account_id(account_id_type account) const {
   return my->get_committee_member_by_account_id(account);
}

fc::optional<committee_member_object> database_api_impl::get_committee_member_by_account_id(account_id_type account) const {
   const auto &idx = _db.get_index_type<committee_member_index>().indices().get<by_account>();
   auto itr = idx.find(account);
   if (itr != idx.end())
      return *itr;
   return {};
}

fc::optional<committee_member_object> database_api::get_committee_member_by_account(const std::string account_id_or_name) const {
   return my->get_committee_member_by_account(account_id_or_name);
}

fc::optional<committee_member_object> database_api_impl::get_committee_member_by_account(const std::string account_id_or_name) const {
   const account_id_type account = get_account_from_string(account_id_or_name)->id;
   return get_committee_member_by_account_id(account);
}

map<string, committee_member_id_type> database_api::lookup_committee_member_accounts(const string &lower_bound_name, uint32_t limit) const {
   return my->lookup_committee_member_accounts(lower_bound_name, limit);
}

map<string, committee_member_id_type> database_api_impl::lookup_committee_member_accounts(const string &lower_bound_name, uint32_t limit) const {
   FC_ASSERT(limit <= api_limit_lookup_committee_member_accounts,
             "Number of querying accounts can not be greater than ${configured_limit}",
             ("configured_limit", api_limit_lookup_committee_member_accounts));
   const auto &committee_members_by_id = _db.get_index_type<committee_member_index>().indices().get<by_id>();

   // we want to order committee_members by account name, but that name is in the account object
   // so the committee_member_index doesn't have a quick way to access it.
   // get all the names and look them all up, sort them, then figure out what
   // records to return.  This could be optimized, but we expect the
   // number of committee_members to be few and the frequency of calls to be rare
   std::map<std::string, committee_member_id_type> committee_members_by_account_name;
   for (const committee_member_object &committee_member : committee_members_by_id)
      if (auto account_iter = _db.find(committee_member.committee_member_account))
         if (account_iter->name >= lower_bound_name) // we can ignore anything below lower_bound_name
            committee_members_by_account_name.insert(std::make_pair(account_iter->name, committee_member.id));

   auto end_iter = committee_members_by_account_name.begin();
   while (end_iter != committee_members_by_account_name.end() && limit--)
      ++end_iter;
   committee_members_by_account_name.erase(end_iter, committee_members_by_account_name.end());
   return committee_members_by_account_name;
}

uint64_t database_api::get_committee_member_count() const {
   return my->get_committee_member_count();
}

uint64_t database_api_impl::get_committee_member_count() const {
   return _db.get_index_type<committee_member_index>().indices().size();
}

//////////////////////////////////////////////////////////////////////
//                                                                  //
// SON members                                                      //
//                                                                  //
//////////////////////////////////////////////////////////////////////

vector<optional<son_object>> database_api::get_sons(const vector<son_id_type> &son_ids) const {
   return my->get_sons(son_ids);
}

vector<optional<son_object>> database_api_impl::get_sons(const vector<son_id_type> &son_ids) const {
   vector<optional<son_object>> result;
   result.reserve(son_ids.size());
   std::transform(son_ids.begin(), son_ids.end(), std::back_inserter(result),
                  [this](son_id_type id) -> optional<son_object> {
                     if (auto o = _db.find(id))
                        return *o;
                     return {};
                  });
   return result;
}

fc::optional<son_object> database_api::get_son_by_account_id(account_id_type account) const {
   return my->get_son_by_account_id(account);
}

fc::optional<son_object> database_api_impl::get_son_by_account_id(account_id_type account) const {
   const auto &idx = _db.get_index_type<son_index>().indices().get<by_account>();
   auto itr = idx.find(account);
   if (itr != idx.end())
      return *itr;
   return {};
}

fc::optional<son_object> database_api::get_son_by_account(const std::string account_id_or_name) const {
   return my->get_son_by_account(account_id_or_name);
}

fc::optional<son_object> database_api_impl::get_son_by_account(const std::string account_id_or_name) const {
   const account_id_type account = get_account_from_string(account_id_or_name)->id;
   return get_son_by_account_id(account);
}

map<string, son_id_type> database_api::lookup_son_accounts(const string &lower_bound_name, uint32_t limit) const {
   return my->lookup_son_accounts(lower_bound_name, limit);
}

map<string, son_id_type> database_api_impl::lookup_son_accounts(const string &lower_bound_name, uint32_t limit) const {
   FC_ASSERT(limit <= api_limit_lookup_son_accounts,
             "Number of querying accounts can not be greater than ${configured_limit}",
             ("configured_limit", api_limit_lookup_son_accounts));

   const auto &sons_by_id = _db.get_index_type<son_index>().indices().get<by_id>();

   // we want to order sons by account name, but that name is in the account object
   // so the son_index doesn't have a quick way to access it.
   // get all the names and look them all up, sort them, then figure out what
   // records to return.  This could be optimized, but we expect the
   // number of witnesses to be few and the frequency of calls to be rare
   std::map<std::string, son_id_type> sons_by_account_name;
   for (const son_object &son : sons_by_id)
      if (auto account_iter = _db.find(son.son_account))
         if (account_iter->name >= lower_bound_name) // we can ignore anything below lower_bound_name
            sons_by_account_name.insert(std::make_pair(account_iter->name, son.id));

   auto end_iter = sons_by_account_name.begin();
   while (end_iter != sons_by_account_name.end() && limit--)
      ++end_iter;
   sons_by_account_name.erase(end_iter, sons_by_account_name.end());
   return sons_by_account_name;
}

uint64_t database_api::get_son_count() const {
   return my->get_son_count();
}

uint64_t database_api_impl::get_son_count() const {
   return _db.get_index_type<son_index>().indices().size();
}

//////////////////////////////////////////////////////////////////////
//                                                                  //
// SON Wallets                                                      //
//                                                                  //
//////////////////////////////////////////////////////////////////////

optional<son_wallet_object> database_api::get_active_son_wallet() {
   return my->get_active_son_wallet();
}

optional<son_wallet_object> database_api_impl::get_active_son_wallet() {
   const auto &idx = _db.get_index_type<son_wallet_index>().indices().get<by_id>();
   auto obj = idx.rbegin();
   if (obj != idx.rend()) {
      return *obj;
   }
   return {};
}

optional<son_wallet_object> database_api::get_son_wallet_by_time_point(time_point_sec time_point) {
   return my->get_son_wallet_by_time_point(time_point);
}

optional<son_wallet_object> database_api_impl::get_son_wallet_by_time_point(time_point_sec time_point) {
   const auto &son_wallets_by_id = _db.get_index_type<son_wallet_index>().indices().get<by_id>();
   for (const son_wallet_object &swo : son_wallets_by_id) {
      if ((time_point >= swo.valid_from) && (time_point < swo.expires))
         return swo;
   }
   return {};
}

vector<optional<son_wallet_object>> database_api::get_son_wallets(uint32_t limit) {
   return my->get_son_wallets(limit);
}

vector<optional<son_wallet_object>> database_api_impl::get_son_wallets(uint32_t limit) {
   FC_ASSERT(limit <= 1000);
   vector<optional<son_wallet_object>> result;
   const auto &son_wallets_by_id = _db.get_index_type<son_wallet_index>().indices().get<by_id>();
   for (const son_wallet_object &swo : son_wallets_by_id)
      result.push_back(swo);
   return result;
}

//////////////////////////////////////////////////////////////////////
//                                                                  //
// Sidechain Accounts                                               //
//                                                                  //
//////////////////////////////////////////////////////////////////////

vector<optional<sidechain_address_object>> database_api::get_sidechain_addresses(const vector<sidechain_address_id_type> &sidechain_address_ids) const {
   return my->get_sidechain_addresses(sidechain_address_ids);
}

vector<optional<sidechain_address_object>> database_api_impl::get_sidechain_addresses(const vector<sidechain_address_id_type> &sidechain_address_ids) const {
   vector<optional<sidechain_address_object>> result;
   result.reserve(sidechain_address_ids.size());
   std::transform(sidechain_address_ids.begin(), sidechain_address_ids.end(), std::back_inserter(result),
                  [this](sidechain_address_id_type id) -> optional<sidechain_address_object> {
                     if (auto o = _db.find(id))
                        return *o;
                     return {};
                  });
   return result;
}

vector<optional<sidechain_address_object>> database_api::get_sidechain_addresses_by_account(account_id_type account) const {
   return my->get_sidechain_addresses_by_account(account);
}

vector<optional<sidechain_address_object>> database_api_impl::get_sidechain_addresses_by_account(account_id_type account) const {
   vector<optional<sidechain_address_object>> result;
   const auto &sidechain_addresses_range = _db.get_index_type<sidechain_address_index>().indices().get<by_account>().equal_range(account);
   std::for_each(sidechain_addresses_range.first, sidechain_addresses_range.second,
                 [&result](const sidechain_address_object &sao) {
                    if (sao.expires == time_point_sec::maximum())
                       result.push_back(sao);
                 });
   return result;
}

vector<optional<sidechain_address_object>> database_api::get_sidechain_addresses_by_sidechain(sidechain_type sidechain) const {
   return my->get_sidechain_addresses_by_sidechain(sidechain);
}

vector<optional<sidechain_address_object>> database_api_impl::get_sidechain_addresses_by_sidechain(sidechain_type sidechain) const {
   vector<optional<sidechain_address_object>> result;
   const auto &sidechain_addresses_range = _db.get_index_type<sidechain_address_index>().indices().get<by_sidechain>().equal_range(sidechain);
   std::for_each(sidechain_addresses_range.first, sidechain_addresses_range.second,
                 [&result](const sidechain_address_object &sao) {
                    if (sao.expires == time_point_sec::maximum())
                       result.push_back(sao);
                 });
   return result;
}

fc::optional<sidechain_address_object> database_api::get_sidechain_address_by_account_and_sidechain(account_id_type account, sidechain_type sidechain) const {
   return my->get_sidechain_address_by_account_and_sidechain(account, sidechain);
}

fc::optional<sidechain_address_object> database_api_impl::get_sidechain_address_by_account_and_sidechain(account_id_type account, sidechain_type sidechain) const {
   const auto &idx = _db.get_index_type<sidechain_address_index>().indices().get<by_account_and_sidechain_and_expires>();
   auto itr = idx.find(boost::make_tuple(account, sidechain, time_point_sec::maximum()));
   if (itr != idx.end())
      return *itr;
   return {};
}

uint64_t database_api::get_sidechain_addresses_count() const {
   return my->get_sidechain_addresses_count();
}

uint64_t database_api_impl::get_sidechain_addresses_count() const {
   return _db.get_index_type<sidechain_address_index>().indices().size();
}

//////////////////////////////////////////////////////////////////////
//                                                                  //
// Workers                                                          //
//                                                                  //
//////////////////////////////////////////////////////////////////////

vector<optional<worker_object>> database_api::get_workers(const vector<worker_id_type> &worker_ids) const {
   return my->get_workers(worker_ids);
}

vector<worker_object> database_api::get_workers_by_account_id(account_id_type account) const {
   return my->get_workers_by_account_id(account);
}

vector<worker_object> database_api::get_workers_by_account(const std::string account_id_or_name) const {
   return my->get_workers_by_account(account_id_or_name);
}

map<string, worker_id_type> database_api::lookup_worker_accounts(const string &lower_bound_name, uint32_t limit) const {
   return my->lookup_worker_accounts(lower_bound_name, limit);
}

uint64_t database_api::get_worker_count() const {
   return my->get_worker_count();
}

vector<optional<worker_object>> database_api_impl::get_workers(const vector<worker_id_type> &worker_ids) const {
   vector<optional<worker_object>> result;
   result.reserve(worker_ids.size());
   std::transform(worker_ids.begin(), worker_ids.end(), std::back_inserter(result),
                  [this](worker_id_type id) -> optional<worker_object> {
                     if (auto o = _db.find(id))
                        return *o;
                     return {};
                  });
   return result;
}

vector<worker_object> database_api_impl::get_workers_by_account_id(account_id_type account) const {
   const auto &idx = _db.get_index_type<worker_index>().indices().get<by_account>();
   auto itr = idx.find(account);
   vector<worker_object> result;

   if (itr != idx.end() && itr->worker_account == account) {
      result.emplace_back(*itr);
      ++itr;
   }

   return result;
}

vector<worker_object> database_api_impl::get_workers_by_account(const std::string account_id_or_name) const {
   const account_id_type account = get_account_from_string(account_id_or_name)->id;
   return get_workers_by_account_id(account);
}

map<string, worker_id_type> database_api_impl::lookup_worker_accounts(const string &lower_bound_name, uint32_t limit) const {
   FC_ASSERT(limit <= api_limit_lookup_worker_accounts,
             "Number of querying accounts can not be greater than ${configured_limit}",
             ("configured_limit", api_limit_lookup_worker_accounts));

   const auto &workers_by_id = _db.get_index_type<worker_index>().indices().get<by_id>();

   // we want to order workers by account name, but that name is in the account object
   // so the worker_index doesn't have a quick way to access it.
   // get all the names and look them all up, sort them, then figure out what
   // records to return.  This could be optimized, but we expect the
   // number of witnesses to be few and the frequency of calls to be rare
   std::map<std::string, worker_id_type> workers_by_account_name;
   for (const worker_object &worker : workers_by_id)
      if (auto account_iter = _db.find(worker.worker_account))
         if (account_iter->name >= lower_bound_name) // we can ignore anything below lower_bound_name
            workers_by_account_name.insert(std::make_pair(account_iter->name, worker.id));

   auto end_iter = workers_by_account_name.begin();
   while (end_iter != workers_by_account_name.end() && limit--)
      ++end_iter;
   workers_by_account_name.erase(end_iter, workers_by_account_name.end());
   return workers_by_account_name;
}

uint64_t database_api_impl::get_worker_count() const {
   return _db.get_index_type<worker_index>().indices().size();
}

//////////////////////////////////////////////////////////////////////
//                                                                  //
// Votes                                                            //
//                                                                  //
//////////////////////////////////////////////////////////////////////

vector<variant> database_api::lookup_vote_ids(const vector<vote_id_type> &votes) const {
   return my->lookup_vote_ids(votes);
}

vector<vote_id_type> database_api::get_votes_ids(const string &account_name_or_id) const {
   return my->get_votes_ids(account_name_or_id);
}

votes_info database_api::get_votes(const string &account_name_or_id) const {
   return my->get_votes(account_name_or_id);
}

vector<account_object> database_api::get_voters_by_id(const vote_id_type &vote_id) const {
   return my->get_voters_by_id(vote_id);
}

voters_info database_api::get_voters(const string &account_name_or_id) const {
   return my->get_voters(account_name_or_id);
}

vector<variant> database_api_impl::lookup_vote_ids(const vector<vote_id_type> &votes) const {
   FC_ASSERT(votes.size() < 1000, "Only 1000 votes can be queried at a time");

   const auto &witness_idx = _db.get_index_type<witness_index>().indices().get<by_vote_id>();
   const auto &committee_idx = _db.get_index_type<committee_member_index>().indices().get<by_vote_id>();
   const auto &for_worker_idx = _db.get_index_type<worker_index>().indices().get<by_vote_for>();
   const auto &against_worker_idx = _db.get_index_type<worker_index>().indices().get<by_vote_against>();
   const auto &son_idx = _db.get_index_type<son_index>().indices().get<by_vote_id>();

   vector<variant> result;
   result.reserve(votes.size());
   for (auto id : votes) {
      switch (id.type()) {
      case vote_id_type::committee: {
         auto itr = committee_idx.find(id);
         if (itr != committee_idx.end())
            result.emplace_back(variant(*itr, 1));
         else
            result.emplace_back(variant());
         break;
      }
      case vote_id_type::witness: {
         auto itr = witness_idx.find(id);
         if (itr != witness_idx.end())
            result.emplace_back(variant(*itr, 1));
         else
            result.emplace_back(variant());
         break;
      }
      case vote_id_type::worker: {
         auto itr = for_worker_idx.find(id);
         if (itr != for_worker_idx.end()) {
            result.emplace_back(variant(*itr, 1));
         } else {
            auto itr = against_worker_idx.find(id);
            if (itr != against_worker_idx.end()) {
               result.emplace_back(variant(*itr, 1));
            } else {
               result.emplace_back(variant());
            }
         }
         break;
      }
      case vote_id_type::son: {
         auto itr = son_idx.find(id);
         if (itr != son_idx.end())
            result.emplace_back(variant(*itr, 5));
         else
            result.emplace_back(variant());
         break;
      }

      case vote_id_type::VOTE_TYPE_COUNT:
         break; // supress unused enum value warnings
      default:
         FC_CAPTURE_AND_THROW(fc::out_of_range_exception, (id));
      }
   }
   return result;
}

vector<vote_id_type> database_api_impl::get_votes_ids(const string &account_name_or_id) const {
   vector<vote_id_type> result;
   const account_object *account = get_account_from_string(account_name_or_id);

   //! Iterate throug votes and fill vector
   for (const auto &vote : account->options.votes) {
      result.emplace_back(vote);
   }

   return result;
}

votes_info database_api_impl::get_votes(const string &account_name_or_id) const {
   votes_info result;

   const auto &votes_ids = get_votes_ids(account_name_or_id);
   const auto &committee_ids = get_votes_objects<committee_member_index, by_vote_id>(votes_ids);
   const auto &witness_ids = get_votes_objects<witness_index, by_vote_id>(votes_ids);
   const auto &for_worker_ids = get_votes_objects<worker_index, by_vote_for>(votes_ids);
   const auto &against_worker_ids = get_votes_objects<worker_index, by_vote_against>(votes_ids);
   const auto &son_ids = get_votes_objects<son_index, by_vote_id>(votes_ids, 5);

   //! Fill votes info
   if (!committee_ids.empty()) {
      vector<votes_info_object> votes_for_committee_members;
      votes_for_committee_members.reserve(committee_ids.size());
      for (const auto &committee : committee_ids) {
         const auto &committee_obj = committee.as<committee_member_object>(2);
         votes_for_committee_members.emplace_back(votes_info_object{committee_obj.vote_id, committee_obj.id});
      }
      result.votes_for_committee_members = std::move(votes_for_committee_members);
   }

   if (!witness_ids.empty()) {
      vector<votes_info_object> votes_for_witnesses;
      votes_for_witnesses.reserve(witness_ids.size());
      for (const auto &witness : witness_ids) {
         const auto &witness_obj = witness.as<witness_object>(2);
         votes_for_witnesses.emplace_back(votes_info_object{witness_obj.vote_id, witness_obj.id});
      }
      result.votes_for_witnesses = std::move(votes_for_witnesses);
   }

   if (!for_worker_ids.empty()) {
      vector<votes_info_object> votes_for_workers;
      votes_for_workers.reserve(for_worker_ids.size());
      for (const auto &for_worker : for_worker_ids) {
         const auto &for_worker_obj = for_worker.as<worker_object>(2);
         votes_for_workers.emplace_back(votes_info_object{for_worker_obj.vote_for, for_worker_obj.id});
      }
      result.votes_for_workers = std::move(votes_for_workers);
   }

   if (!against_worker_ids.empty()) {
      vector<votes_info_object> votes_against_workers;
      votes_against_workers.reserve(against_worker_ids.size());
      for (const auto &against_worker : against_worker_ids) {
         const auto &against_worker_obj = against_worker.as<worker_object>(2);
         votes_against_workers.emplace_back(votes_info_object{against_worker_obj.vote_against, against_worker_obj.id});
      }
      result.votes_against_workers = std::move(votes_against_workers);
   }

   if (!son_ids.empty()) {
      vector<votes_info_object> votes_for_sons;
      votes_for_sons.reserve(son_ids.size());
      for (const auto &son : son_ids) {
         const auto &son_obj = son.as<son_object>(6);
         votes_for_sons.emplace_back(votes_info_object{son_obj.vote_id, son_obj.id});
      }
      result.votes_for_sons = std::move(votes_for_sons);
   }

   return result;
}

vector<account_object> database_api_impl::get_voters_by_id(const vote_id_type &vote_id) const {
   vector<account_object> result;

   //! We search all accounts that have voted for this vote_id
   const auto &account_index = _db.get_index_type<graphene::chain::account_index>().indices().get<by_id>();
   for (const auto &account : account_index) {
      if (account.options.votes.count(vote_id) != 0)
         result.emplace_back(account);
   }

   return result;
}

voters_info database_api_impl::get_voters(const string &account_name_or_id) const {
   voters_info result;

   //! Find account name
   bool owner_account_found = false;
   std::string owner_account_id;

   //! Check if we have account by name
   const auto &account_object = get_account_by_name(account_name_or_id);
   if (account_object) {
      //! It is account
      owner_account_id = object_id_to_string(account_object->get_id());
      owner_account_found = true;
   } else {
      //! Check if we have account id
      const auto &account_id = maybe_id<account_id_type>(account_name_or_id);
      if (account_id) {
         //! It may be account id
         const auto &account_objects = get_accounts({account_name_or_id});
         if (!account_objects.empty()) {
            const auto &account_object = account_objects.front();
            if (account_object) {
               //! It is account object
               owner_account_id = object_id_to_string(account_object->get_id());
               owner_account_found = true;
            }
         }
      } else {
         //! Check if we have committee member id
         const auto &committee_member_id = maybe_id<committee_member_id_type>(account_name_or_id);
         if (committee_member_id) {
            //! It may be committee member id
            const auto &committee_member_objects = get_committee_members({*committee_member_id});
            if (!committee_member_objects.empty()) {
               const auto &committee_member_object = committee_member_objects.front();
               if (committee_member_object) {
                  //! It is committee member object
                  owner_account_id = object_id_to_string(committee_member_object->committee_member_account);
                  owner_account_found = true;
               }
            }
         } else {
            //! Check if we have witness id
            const auto &witness_id = maybe_id<witness_id_type>(account_name_or_id);
            if (witness_id) {
               //! It may be witness id
               const auto &witness_objects = get_witnesses({*witness_id});
               if (!witness_objects.empty()) {
                  const auto &witness_object = witness_objects.front();
                  if (witness_object) {
                     //! It is witness object
                     owner_account_id = object_id_to_string(witness_object->witness_account);
                     owner_account_found = true;
                  }
               }
            } else {
               //! Check if we have worker id
               const auto &worker_id = maybe_id<worker_id_type>(account_name_or_id);
               if (worker_id) {
                  //! It may be worker id
                  const auto &worker_objects = get_workers({*worker_id});
                  if (!worker_objects.empty()) {
                     const auto &worker_object = worker_objects.front();
                     if (worker_object) {
                        //! It is worker object
                        owner_account_id = object_id_to_string(worker_object->worker_account);
                        owner_account_found = true;
                     }
                  }
               } else {
                  //! Check if we have son id
                  const auto &son_id = maybe_id<son_id_type>(account_name_or_id);
                  if (son_id) {
                     //! It may be son id
                     const auto &son_objects = get_sons({*son_id});
                     if (!son_objects.empty()) {
                        const auto &son_object = son_objects.front();
                        if (son_object) {
                           //! It is son object
                           owner_account_id = object_id_to_string(son_object->son_account);
                           owner_account_found = true;
                        }
                     }
                  }
               }
            }
         }
      }
   }

   //! We didn't find who it was
   if (!owner_account_found)
      FC_THROW_EXCEPTION(database_query_exception, "Wrong account_name_or_id: ${account_name_or_id}", ("account_name_or_id", account_name_or_id));

   //! Fill voters_info
   const auto &committee_member_object = get_committee_member_by_account(owner_account_id);
   const auto &witness_object = get_witness_by_account(owner_account_id);
   const auto &worker_objects = get_workers_by_account(owner_account_id);
   const auto &son_object = get_son_by_account(owner_account_id);

   //! Info for committee member voters
   if (committee_member_object) {
      const auto &committee_member_voters = get_voters_by_id(committee_member_object->vote_id);
      voters_info_object voters_for_committee_member;
      voters_for_committee_member.vote_id = committee_member_object->vote_id;
      voters_for_committee_member.voters.reserve(committee_member_voters.size());
      for (const auto &voter : committee_member_voters) {
         voters_for_committee_member.voters.emplace_back(voter.get_id());
      }
      result.voters_for_committee_member = std::move(voters_for_committee_member);
   }

   //! Info for witness voters
   if (witness_object) {
      const auto &witness_voters = get_voters_by_id(witness_object->vote_id);
      voters_info_object voters_for_witness;
      voters_for_witness.vote_id = witness_object->vote_id;
      voters_for_witness.voters.reserve(witness_voters.size());
      for (const auto &voter : witness_voters) {
         voters_for_witness.voters.emplace_back(voter.get_id());
      }
      result.voters_for_witness = std::move(voters_for_witness);
   }

   //! Info for worker voters
   if (!worker_objects.empty()) {
      vector<voters_info_object> voters_for_workers(worker_objects.size());
      vector<voters_info_object> voters_against_workers(worker_objects.size());
      for (const auto &worker_object : worker_objects) {
         voters_info_object voters_for_worker;
         const auto &for_worker_voters = get_voters_by_id(worker_object.vote_for);
         voters_for_worker.vote_id = worker_object.vote_for;
         voters_for_worker.voters.reserve(for_worker_voters.size());
         for (const auto &voter : for_worker_voters) {
            voters_for_worker.voters.emplace_back(voter.get_id());
         }
         voters_for_workers.emplace_back(std::move(voters_for_worker));

         voters_info_object voters_against_worker;
         const auto &against_worker_voters = get_voters_by_id(worker_object.vote_against);
         voters_against_worker.vote_id = worker_object.vote_against;
         voters_against_worker.voters.reserve(against_worker_voters.size());
         for (const auto &voter : against_worker_voters) {
            voters_against_worker.voters.emplace_back(voter.get_id());
         }
         voters_against_workers.emplace_back(std::move(voters_against_worker));
      }
      result.voters_for_workers = std::move(voters_for_workers);
      result.voters_against_workers = std::move(voters_against_workers);
   }

   //! Info for son voters
   if (son_object) {
      const auto &son_voters = get_voters_by_id(son_object->vote_id);
      voters_info_object voters_for_son;
      voters_for_son.vote_id = son_object->vote_id;
      voters_for_son.voters.reserve(son_voters.size());
      for (const auto &voter : son_voters) {
         voters_for_son.voters.emplace_back(voter.get_id());
      }
      result.voters_for_son = std::move(voters_for_son);
   }

   return result;
}

//////////////////////////////////////////////////////////////////////
//                                                                  //
// Authority / validation                                           //
//                                                                  //
//////////////////////////////////////////////////////////////////////

std::string database_api::get_transaction_hex(const signed_transaction &trx) const {
   return my->get_transaction_hex(trx);
}

std::string database_api_impl::get_transaction_hex(const signed_transaction &trx) const {
   return fc::to_hex(fc::raw::pack(trx));
}

set<public_key_type> database_api::get_required_signatures(const signed_transaction &trx, const flat_set<public_key_type> &available_keys) const {
   return my->get_required_signatures(trx, available_keys);
}

set<public_key_type> database_api_impl::get_required_signatures(const signed_transaction &trx, const flat_set<public_key_type> &available_keys) const {
   wdump((trx)(available_keys));
   auto result = trx.get_required_signatures(
         _db.get_chain_id(),
         available_keys,
         [&](account_id_type id) {
            return &id(_db).active;
         },
         [&](account_id_type id) {
            return &id(_db).owner;
         },
         [&](account_id_type id, const operation &op) {
            return _db.get_account_custom_authorities(id, op);
         },
         _db.get_global_properties().parameters.max_authority_depth);
   wdump((result));
   return result;
}

set<public_key_type> database_api::get_potential_signatures(const signed_transaction &trx) const {
   return my->get_potential_signatures(trx);
}
set<address> database_api::get_potential_address_signatures(const signed_transaction &trx) const {
   return my->get_potential_address_signatures(trx);
}

set<public_key_type> database_api_impl::get_potential_signatures(const signed_transaction &trx) const {
   wdump((trx));
   set<public_key_type> result;
   trx.get_required_signatures(
         _db.get_chain_id(),
         flat_set<public_key_type>(),
         [&](account_id_type id) {
            const auto &auth = id(_db).active;
            for (const auto &k : auth.get_keys())
               result.insert(k);
            return &auth;
         },
         [&](account_id_type id) {
            const auto &auth = id(_db).owner;
            for (const auto &k : auth.get_keys())
               result.insert(k);
            return &auth;
         },
         [&](account_id_type id, const operation &op) {
            vector<authority> custom_auths = _db.get_account_custom_authorities(id, op);
            for (const auto &cauth : custom_auths) {
               for (const auto &k : cauth.get_keys()) {
                  result.insert(k);
               }
            }
            return custom_auths;
         },
         _db.get_global_properties().parameters.max_authority_depth);

   wdump((result));
   return result;
}

set<address> database_api_impl::get_potential_address_signatures(const signed_transaction &trx) const {
   set<address> result;
   trx.get_required_signatures(
         _db.get_chain_id(),
         flat_set<public_key_type>(),
         [&](account_id_type id) {
            const auto &auth = id(_db).active;
            for (const auto &k : auth.get_addresses())
               result.insert(k);
            return &auth;
         },
         [&](account_id_type id) {
            const auto &auth = id(_db).owner;
            for (const auto &k : auth.get_addresses())
               result.insert(k);
            return &auth;
         },
         [&](account_id_type id, const operation &op) {
            return _db.get_account_custom_authorities(id, op);
         },
         _db.get_global_properties().parameters.max_authority_depth);
   return result;
}

bool database_api::verify_authority(const signed_transaction &trx) const {
   return my->verify_authority(trx);
}

bool database_api_impl::verify_authority(const signed_transaction &trx) const {
   trx.verify_authority(
         _db.get_chain_id(),
         [this](account_id_type id) {
            return &id(_db).active;
         },
         [this](account_id_type id) {
            return &id(_db).owner;
         },
         [this](account_id_type id, const operation &op) {
            return _db.get_account_custom_authorities(id, op);
         },
         _db.get_global_properties().parameters.max_authority_depth);
   return true;
}

bool database_api::verify_account_authority(const string &name_or_id, const flat_set<public_key_type> &signers) const {
   return my->verify_account_authority(name_or_id, signers);
}

bool database_api_impl::verify_account_authority(const string &name_or_id, const flat_set<public_key_type> &keys) const {
   FC_ASSERT(name_or_id.size() > 0);
   const account_object *account = nullptr;
   if (std::isdigit(name_or_id[0]))
      account = _db.find(fc::variant(name_or_id, 1).as<account_id_type>(1));
   else {
      const auto &idx = _db.get_index_type<account_index>().indices().get<by_name>();
      auto itr = idx.find(name_or_id);
      if (itr != idx.end())
         account = &*itr;
   }
   FC_ASSERT(account, "no such account");

   /// reuse trx.verify_authority by creating a dummy transfer
   signed_transaction trx;
   transfer_operation op;
   op.from = account->id;
   trx.operations.emplace_back(op);

   return verify_authority(trx);
}

processed_transaction database_api::validate_transaction(const signed_transaction &trx) const {
   return my->validate_transaction(trx);
}

processed_transaction database_api_impl::validate_transaction(const signed_transaction &trx) const {
   return _db.validate_transaction(trx);
}

vector<fc::variant> database_api::get_required_fees(const vector<operation> &ops, const std::string &asset_id_or_symbol) const {
   return my->get_required_fees(ops, asset_id_or_symbol);
}

/**
 * Container method for mutually recursive functions used to
 * implement get_required_fees() with potentially nested proposals.
 */
struct get_required_fees_helper {
   get_required_fees_helper(
         const fee_schedule &_current_fee_schedule,
         const price &_core_exchange_rate,
         uint32_t _max_recursion) :
         current_fee_schedule(_current_fee_schedule),
         core_exchange_rate(_core_exchange_rate),
         max_recursion(_max_recursion) {
   }

   fc::variant set_op_fees(operation &op) {
      if (op.which() == operation::tag<proposal_create_operation>::value) {
         return set_proposal_create_op_fees(op);
      } else {
         asset fee = current_fee_schedule.set_fee(op, core_exchange_rate);
         fc::variant result;
         fc::to_variant(fee, result, GRAPHENE_NET_MAX_NESTED_OBJECTS);
         return result;
      }
   }

   fc::variant set_proposal_create_op_fees(operation &proposal_create_op) {
      proposal_create_operation &op = proposal_create_op.get<proposal_create_operation>();
      std::pair<asset, fc::variants> result;
      for (op_wrapper &prop_op : op.proposed_ops) {
         FC_ASSERT(current_recursion < max_recursion);
         ++current_recursion;
         result.second.push_back(set_op_fees(prop_op.op));
         --current_recursion;
      }
      // we need to do this on the boxed version, which is why we use
      // two mutually recursive functions instead of a visitor
      result.first = current_fee_schedule.set_fee(proposal_create_op, core_exchange_rate);
      fc::variant vresult;
      fc::to_variant(result, vresult, GRAPHENE_NET_MAX_NESTED_OBJECTS);
      return vresult;
   }

   const fee_schedule &current_fee_schedule;
   const price &core_exchange_rate;
   uint32_t max_recursion;
   uint32_t current_recursion = 0;
};

vector<fc::variant> database_api_impl::get_required_fees(const vector<operation> &ops, const std::string &asset_id_or_symbol) const {
   vector<operation> _ops = ops;
   //
   // we copy the ops because we need to mutate an operation to reliably
   // determine its fee, see #435
   //

   vector<fc::variant> result;
   result.reserve(ops.size());
   const asset_object &a = *get_asset_from_string(asset_id_or_symbol);
   get_required_fees_helper helper(
         _db.current_fee_schedule(),
         a.options.core_exchange_rate,
         GET_REQUIRED_FEES_MAX_RECURSION);
   for (operation &op : _ops) {
      result.push_back(helper.set_op_fees(op));
   }
   return result;
}

//////////////////////////////////////////////////////////////////////
//                                                                  //
// Proposed transactions                                            //
//                                                                  //
//////////////////////////////////////////////////////////////////////

vector<proposal_object> database_api::get_proposed_transactions(const std::string account_id_or_name) const {
   return my->get_proposed_transactions(account_id_or_name);
}

/** TODO: add secondary index that will accelerate this process */
vector<proposal_object> database_api_impl::get_proposed_transactions(const std::string account_id_or_name) const {
   const auto &idx = _db.get_index_type<proposal_index>();
   vector<proposal_object> result;
   const account_id_type id = get_account_from_string(account_id_or_name)->id;

   idx.inspect_all_objects([&](const object &obj) {
      const proposal_object &p = static_cast<const proposal_object &>(obj);
      if (p.required_active_approvals.find(id) != p.required_active_approvals.end())
         result.push_back(p);
      else if (p.required_owner_approvals.find(id) != p.required_owner_approvals.end())
         result.push_back(p);
      else if (p.available_active_approvals.find(id) != p.available_active_approvals.end())
         result.push_back(p);
   });
   return result;
}

//////////////////////////////////////////////////////////////////////
//                                                                  //
// Blinded balances                                                 //
//                                                                  //
//////////////////////////////////////////////////////////////////////

vector<blinded_balance_object> database_api::get_blinded_balances(const flat_set<commitment_type> &commitments) const {
   return my->get_blinded_balances(commitments);
}

vector<blinded_balance_object> database_api_impl::get_blinded_balances(const flat_set<commitment_type> &commitments) const {
   vector<blinded_balance_object> result;
   result.reserve(commitments.size());
   const auto &bal_idx = _db.get_index_type<blinded_balance_index>();
   const auto &by_commitment_idx = bal_idx.indices().get<by_commitment>();
   for (const auto &c : commitments) {
      auto itr = by_commitment_idx.find(c);
      if (itr != by_commitment_idx.end())
         result.push_back(*itr);
   }
   return result;
}

//////////////////////////////////////////////////////////////////////
//                                                                  //
// Tournament methods                                               //
//                                                                  //
//////////////////////////////////////////////////////////////////////
vector<tournament_object> database_api::get_tournaments_in_state(tournament_state state, uint32_t limit) const {
   return my->get_tournaments_in_state(state, limit);
}

vector<tournament_object> database_api_impl::get_tournaments_in_state(tournament_state state, uint32_t limit) const {
   vector<tournament_object> result;
   const auto &registration_deadline_index = _db.get_index_type<tournament_index>().indices().get<by_registration_deadline>();
   const auto range = registration_deadline_index.equal_range(boost::make_tuple(state));
   for (const tournament_object &tournament_obj : boost::make_iterator_range(range.first, range.second)) {
      result.emplace_back(tournament_obj);
      subscribe_to_item(tournament_obj.id);

      if (result.size() >= limit)
         break;
   }
   return result;
}

vector<tournament_object> database_api::get_tournaments(tournament_id_type stop,
                                                        unsigned limit,
                                                        tournament_id_type start) {
   return my->get_tournaments(stop, limit, start);
}

vector<tournament_object> database_api_impl::get_tournaments(tournament_id_type stop,
                                                             unsigned limit,
                                                             tournament_id_type start) {
   vector<tournament_object> result;
   const auto &tournament_idx = _db.get_index_type<tournament_index>().indices().get<by_id>();
   for (auto elem : tournament_idx) {
      if (result.size() >= limit)
         break;
      if (((elem.get_id().instance.value <= start.instance.value) || start == tournament_id_type()) &&
          ((elem.get_id().instance.value >= stop.instance.value) || stop == tournament_id_type()))
         result.push_back(elem);
   }

   return result;
}

vector<tournament_object> database_api::get_tournaments_by_state(tournament_id_type stop,
                                                                 unsigned limit,
                                                                 tournament_id_type start,
                                                                 tournament_state state) {
   return my->get_tournaments_by_state(stop, limit, start, state);
}

vector<tournament_object> database_api_impl::get_tournaments_by_state(tournament_id_type stop,
                                                                      unsigned limit,
                                                                      tournament_id_type start,
                                                                      tournament_state state) {
   vector<tournament_object> result;
   const auto &tournament_idx = _db.get_index_type<tournament_index>().indices().get<by_id>();
   for (auto elem : tournament_idx) {
      if (result.size() >= limit)
         break;
      if (((elem.get_id().instance.value <= start.instance.value) || start == tournament_id_type()) &&
          ((elem.get_id().instance.value >= stop.instance.value) || stop == tournament_id_type()) &&
          elem.get_state() == state)
         result.push_back(elem);
   }

   return result;
}

const account_object *database_api_impl::get_account_from_string(const std::string &name_or_id,
                                                                 bool throw_if_not_found) const {
   // TODO cache the result to avoid repeatly fetching from db
   FC_ASSERT(name_or_id.size() > 0);
   const account_object *account = nullptr;
   if (std::isdigit(name_or_id[0]))
      account = _db.find(fc::variant(name_or_id, 1).as<account_id_type>(1));
   else {
      const auto &idx = _db.get_index_type<account_index>().indices().get<by_name>();
      auto itr = idx.find(name_or_id);
      if (itr != idx.end())
         account = &*itr;
   }
   if (throw_if_not_found)
      FC_ASSERT(account, "no such account");
   return account;
}

vector<tournament_id_type> database_api::get_registered_tournaments(account_id_type account_filter, uint32_t limit) const {
   return my->get_registered_tournaments(account_filter, limit);
}

vector<tournament_id_type> database_api_impl::get_registered_tournaments(account_id_type account_filter, uint32_t limit) const {
   const auto &tournament_details_idx = _db.get_index_type<tournament_details_index>();
   const auto &tournament_details_primary_idx = dynamic_cast<const primary_index<tournament_details_index> &>(tournament_details_idx);
   const auto &players_idx = tournament_details_primary_idx.get_secondary_index<graphene::chain::tournament_players_index>();

   vector<tournament_id_type> tournament_ids = players_idx.get_registered_tournaments_for_account(account_filter);
   if (tournament_ids.size() >= limit)
      tournament_ids.resize(limit);
   return tournament_ids;
}

//////////////////////////////////////////////////////////////////////
//                                                                  //
// GPOS methods                                                     //
//                                                                  //
//////////////////////////////////////////////////////////////////////

graphene::app::gpos_info database_api::get_gpos_info(const account_id_type account) const {
   return my->get_gpos_info(account);
}

graphene::app::gpos_info database_api_impl::get_gpos_info(const account_id_type account) const {
   FC_ASSERT(_db.head_block_time() > HARDFORK_GPOS_TIME); //Can be deleted after GPOS hardfork time
   gpos_info result;

   result.vesting_factor = _db.calculate_vesting_factor(account(_db));
   result.current_subperiod = _db.get_gpos_current_subperiod();
   result.last_voted_time = account(_db).statistics(_db).last_vote_time;

   const auto &dividend_data = asset_id_type()(_db).dividend_data(_db);
   const account_object &dividend_distribution_account = dividend_data.dividend_distribution_account(_db);
   result.award = _db.get_balance(dividend_distribution_account, asset_id_type()(_db));

   share_type total_amount;
   auto balance_type = vesting_balance_type::gpos;

   // get only once a collection of accounts that hold nonzero vesting balances of the dividend asset
   const vesting_balance_index &vesting_index = _db.get_index_type<vesting_balance_index>();
   const auto &vesting_balances = vesting_index.indices().get<by_id>();
   for (const vesting_balance_object &vesting_balance_obj : vesting_balances) {
      if (vesting_balance_obj.balance.asset_id == asset_id_type() && vesting_balance_obj.balance_type == balance_type) {
         total_amount += vesting_balance_obj.balance.amount;
      }
   }

   vector<vesting_balance_object> account_vbos;
   const time_point_sec now = _db.head_block_time();
   auto vesting_range = _db.get_index_type<vesting_balance_index>().indices().get<by_account>().equal_range(account);
   std::for_each(vesting_range.first, vesting_range.second,
                 [&account_vbos, now](const vesting_balance_object &balance) {
                    if (balance.balance.amount > 0 && balance.balance_type == vesting_balance_type::gpos && balance.balance.asset_id == asset_id_type())
                       account_vbos.emplace_back(balance);
                 });

   share_type allowed_withdraw_amount = 0, account_vested_balance = 0;

   for (const vesting_balance_object &vesting_balance_obj : account_vbos) {
      account_vested_balance += vesting_balance_obj.balance.amount;
      if (vesting_balance_obj.is_withdraw_allowed(_db.head_block_time(), vesting_balance_obj.balance.amount))
         allowed_withdraw_amount += vesting_balance_obj.balance.amount;
   }

   result.total_amount = total_amount;
   result.allowed_withdraw_amount = allowed_withdraw_amount;
   result.account_vested_balance = account_vested_balance;
   return result;
}

//////////////////////////////////////////////////////////////////////
//                                                                  //
// RBAC methods                                                     //
//                                                                  //
//////////////////////////////////////////////////////////////////////

vector<custom_permission_object> database_api::get_custom_permissions(const account_id_type account) const {
   return my->get_custom_permissions(account);
}

vector<custom_permission_object> database_api_impl::get_custom_permissions(const account_id_type account) const {
   const auto &pindex = _db.get_index_type<custom_permission_index>().indices().get<by_account_and_permission>();
   auto prange = pindex.equal_range(boost::make_tuple(account));
   vector<custom_permission_object> custom_permissions;
   for (const custom_permission_object &pobj : boost::make_iterator_range(prange.first, prange.second)) {
      custom_permissions.push_back(pobj);
   }
   return custom_permissions;
}

fc::optional<custom_permission_object> database_api::get_custom_permission_by_name(const account_id_type account, const string &permission_name) const {
   return my->get_custom_permission_by_name(account, permission_name);
}

fc::optional<custom_permission_object> database_api_impl::get_custom_permission_by_name(const account_id_type account, const string &permission_name) const {
   const auto &pindex = _db.get_index_type<custom_permission_index>().indices().get<by_account_and_permission>();
   auto prange = pindex.equal_range(boost::make_tuple(account, permission_name));
   for (const custom_permission_object &pobj : boost::make_iterator_range(prange.first, prange.second)) {
      return pobj;
   }
   return {};
}

//////////////////////////////////////////////////////////////////////
//                                                                  //
// NFT methods                                                      //
//                                                                  //
//////////////////////////////////////////////////////////////////////

uint64_t database_api::nft_get_balance(const account_id_type owner) const {
   return my->nft_get_balance(owner);
}

uint64_t database_api_impl::nft_get_balance(const account_id_type owner) const {
   const auto &idx_nft = _db.get_index_type<nft_index>().indices().get<by_owner>();
   const auto &idx_nft_range = idx_nft.equal_range(owner);
   return std::distance(idx_nft_range.first, idx_nft_range.second);
}

optional<account_id_type> database_api::nft_owner_of(const nft_id_type token_id) const {
   return my->nft_owner_of(token_id);
}

optional<account_id_type> database_api_impl::nft_owner_of(const nft_id_type token_id) const {
   const auto &idx_nft = _db.get_index_type<nft_index>().indices().get<by_id>();
   auto itr_nft = idx_nft.find(token_id);
   if (itr_nft != idx_nft.end()) {
      return itr_nft->owner;
   }
   return {};
}

optional<account_id_type> database_api::nft_get_approved(const nft_id_type token_id) const {
   return my->nft_get_approved(token_id);
}

optional<account_id_type> database_api_impl::nft_get_approved(const nft_id_type token_id) const {
   const auto &idx_nft = _db.get_index_type<nft_index>().indices().get<by_id>();
   auto itr_nft = idx_nft.find(token_id);
   if (itr_nft != idx_nft.end()) {
      return itr_nft->approved;
   }
   return {};
}

bool database_api::nft_is_approved_for_all(const account_id_type owner, const account_id_type operator_) const {
   return my->nft_is_approved_for_all(owner, operator_);
}

bool database_api_impl::nft_is_approved_for_all(const account_id_type owner, const account_id_type operator_) const {
   const auto &idx_nft = _db.get_index_type<nft_index>().indices().get<by_owner>();
   const auto &idx_nft_range = idx_nft.equal_range(owner);
   if (std::distance(idx_nft_range.first, idx_nft_range.second) == 0) {
      return false;
   }
   bool result = true;
   std::for_each(idx_nft_range.first, idx_nft_range.second, [&](const nft_object &obj) {
      result = result && (obj.approved == operator_);
   });
   return result;
}

string database_api::nft_get_name(const nft_metadata_id_type nft_metadata_id) const {
   return my->nft_get_name(nft_metadata_id);
}

string database_api_impl::nft_get_name(const nft_metadata_id_type nft_metadata_id) const {
   const auto &idx_nft_md = _db.get_index_type<nft_metadata_index>().indices().get<by_id>();
   auto itr_nft_md = idx_nft_md.find(nft_metadata_id);
   if (itr_nft_md != idx_nft_md.end()) {
      return itr_nft_md->name;
   }
   return "";
}

string database_api::nft_get_symbol(const nft_metadata_id_type nft_metadata_id) const {
   return my->nft_get_symbol(nft_metadata_id);
}

string database_api_impl::nft_get_symbol(const nft_metadata_id_type nft_metadata_id) const {
   const auto &idx_nft_md = _db.get_index_type<nft_metadata_index>().indices().get<by_id>();
   auto itr_nft_md = idx_nft_md.find(nft_metadata_id);
   if (itr_nft_md != idx_nft_md.end()) {
      return itr_nft_md->symbol;
   }
   return "";
}

string database_api::nft_get_token_uri(const nft_id_type token_id) const {
   return my->nft_get_token_uri(token_id);
}

string database_api_impl::nft_get_token_uri(const nft_id_type token_id) const {
   string result = "";
   const auto &idx_nft = _db.get_index_type<nft_index>().indices().get<by_id>();
   auto itr_nft = idx_nft.find(token_id);
   if (itr_nft != idx_nft.end()) {
      result = itr_nft->token_uri;
      const auto &idx_nft_md = _db.get_index_type<nft_metadata_index>().indices().get<by_id>();
      auto itr_nft_md = idx_nft_md.find(itr_nft->nft_metadata_id);
      if (itr_nft_md != idx_nft_md.end()) {
         result = itr_nft_md->base_uri + itr_nft->token_uri;
      }
   }
   return result;
}

uint64_t database_api::nft_get_total_supply(const nft_metadata_id_type nft_metadata_id) const {
   return my->nft_get_total_supply(nft_metadata_id);
}

uint64_t database_api_impl::nft_get_total_supply(const nft_metadata_id_type nft_metadata_id) const {
   const auto &idx_nft_md = _db.get_index_type<nft_metadata_index>().indices().get<by_id>();
   return idx_nft_md.size();
}

nft_object database_api::nft_token_by_index(const nft_metadata_id_type nft_metadata_id, const uint64_t token_idx) const {
   return my->nft_token_by_index(nft_metadata_id, token_idx);
}

nft_object database_api_impl::nft_token_by_index(const nft_metadata_id_type nft_metadata_id, const uint64_t token_idx) const {
   const auto &idx_nft = _db.get_index_type<nft_index>().indices().get<by_metadata>();
   auto idx_nft_range = idx_nft.equal_range(nft_metadata_id);
   uint64_t tmp_idx = token_idx;
   for (auto itr = idx_nft_range.first; itr != idx_nft_range.second; ++itr) {
      if (tmp_idx == 0) {
         return *itr;
      }
      tmp_idx = tmp_idx - 1;
   }
   return {};
}

nft_object database_api::nft_token_of_owner_by_index(const nft_metadata_id_type nft_metadata_id, const account_id_type owner, const uint64_t token_idx) const {
   return my->nft_token_of_owner_by_index(nft_metadata_id, owner, token_idx);
}

nft_object database_api_impl::nft_token_of_owner_by_index(const nft_metadata_id_type nft_metadata_id, const account_id_type owner, const uint64_t token_idx) const {
   const auto &idx_nft = _db.get_index_type<nft_index>().indices().get<by_metadata_and_owner>();
   auto idx_nft_range = idx_nft.equal_range(std::make_tuple(nft_metadata_id, owner));
   uint64_t tmp_idx = token_idx;
   for (auto itr = idx_nft_range.first; itr != idx_nft_range.second; ++itr) {
      if (tmp_idx == 0) {
         return *itr;
      }
      tmp_idx = tmp_idx - 1;
   }
   return {};
}

vector<nft_object> database_api::nft_get_all_tokens() const {
   return my->nft_get_all_tokens();
}

vector<nft_object> database_api_impl::nft_get_all_tokens() const {
   const auto &idx_nft = _db.get_index_type<nft_index>().indices().get<by_id>();
   vector<nft_object> result;
   for (auto itr = idx_nft.begin(); itr != idx_nft.end(); ++itr) {
      result.push_back(*itr);
   }
   return result;
}

vector<nft_object> database_api::nft_get_tokens_by_owner(const account_id_type owner) const {
   return my->nft_get_tokens_by_owner(owner);
}

vector<nft_object> database_api_impl::nft_get_tokens_by_owner(const account_id_type owner) const {
   const auto &idx_nft = _db.get_index_type<nft_index>().indices().get<by_owner>();
   auto idx_nft_range = idx_nft.equal_range(owner);
   vector<nft_object> result;
   for (auto itr = idx_nft_range.first; itr != idx_nft_range.second; ++itr) {
      result.push_back(*itr);
   }
   return result;
}

vector<custom_account_authority_object> database_api::get_custom_account_authorities(const account_id_type account) const {
   return my->get_custom_account_authorities(account);
}

vector<custom_account_authority_object> database_api_impl::get_custom_account_authorities(const account_id_type account) const {
   const auto &pindex = _db.get_index_type<custom_permission_index>().indices().get<by_account_and_permission>();
   const auto &cindex = _db.get_index_type<custom_account_authority_index>().indices().get<by_permission_and_op>();
   vector<custom_account_authority_object> custom_account_auths;
   auto prange = pindex.equal_range(boost::make_tuple(account));
   for (const custom_permission_object &pobj : boost::make_iterator_range(prange.first, prange.second)) {
      auto crange = cindex.equal_range(boost::make_tuple(pobj.id));
      for (const custom_account_authority_object &cobj : boost::make_iterator_range(crange.first, crange.second)) {
         custom_account_auths.push_back(cobj);
      }
   }
   return custom_account_auths;
}

vector<custom_account_authority_object> database_api::get_custom_account_authorities_by_permission_id(const custom_permission_id_type permission_id) const {
   return my->get_custom_account_authorities_by_permission_id(permission_id);
}

vector<custom_account_authority_object> database_api_impl::get_custom_account_authorities_by_permission_id(const custom_permission_id_type permission_id) const {
   const auto &cindex = _db.get_index_type<custom_account_authority_index>().indices().get<by_permission_and_op>();
   vector<custom_account_authority_object> custom_account_auths;
   auto crange = cindex.equal_range(boost::make_tuple(permission_id));
   for (const custom_account_authority_object &cobj : boost::make_iterator_range(crange.first, crange.second)) {
      custom_account_auths.push_back(cobj);
   }
   return custom_account_auths;
}

vector<custom_account_authority_object> database_api::get_custom_account_authorities_by_permission_name(const account_id_type account, const string &permission_name) const {
   return my->get_custom_account_authorities_by_permission_name(account, permission_name);
}

vector<custom_account_authority_object> database_api_impl::get_custom_account_authorities_by_permission_name(const account_id_type account, const string &permission_name) const {
   vector<custom_account_authority_object> custom_account_auths;
   fc::optional<custom_permission_object> pobj = get_custom_permission_by_name(account, permission_name);
   if (!pobj) {
      return custom_account_auths;
   }
   const auto &cindex = _db.get_index_type<custom_account_authority_index>().indices().get<by_permission_and_op>();
   auto crange = cindex.equal_range(boost::make_tuple(pobj->id));
   for (const custom_account_authority_object &cobj : boost::make_iterator_range(crange.first, crange.second)) {
      custom_account_auths.push_back(cobj);
   }
   return custom_account_auths;
}

vector<authority> database_api::get_active_custom_account_authorities_by_operation(const account_id_type account, int operation_type) const {
   return my->get_active_custom_account_authorities_by_operation(account, operation_type);
}

vector<authority> database_api_impl::get_active_custom_account_authorities_by_operation(const account_id_type account, int operation_type) const {
   operation op;
   op.set_which(operation_type);
   return _db.get_account_custom_authorities(account, op);
}

// Marketplace
vector<offer_object> database_api::list_offers(const offer_id_type lower_id, uint32_t limit) const {
   return my->list_offers(lower_id, limit);
}

vector<offer_object> database_api_impl::list_offers(const offer_id_type lower_id, uint32_t limit) const {
   FC_ASSERT(limit <= api_limit_all_offers_count,
             "Number of querying offers can not be greater than ${configured_limit}",
             ("configured_limit", api_limit_all_offers_count));
   const auto &offers_idx = _db.get_index_type<offer_index>().indices().get<by_id>();
   vector<offer_object> result;
   result.reserve(limit);

   auto itr = offers_idx.lower_bound(lower_id);

   while (limit-- && itr != offers_idx.end())
      result.emplace_back(*itr++);

   return result;
}

vector<offer_object> database_api::list_sell_offers(const offer_id_type lower_id, uint32_t limit) const {
   return my->list_sell_offers(lower_id, limit);
}

vector<offer_object> database_api_impl::list_sell_offers(const offer_id_type lower_id, uint32_t limit) const {
   FC_ASSERT(limit <= api_limit_all_offers_count,
             "Number of querying offers can not be greater than ${configured_limit}",
             ("configured_limit", api_limit_all_offers_count));
   const auto &offers_idx = _db.get_index_type<offer_index>().indices().get<by_id>();
   vector<offer_object> result;
   result.reserve(limit);

   auto itr = offers_idx.lower_bound(lower_id);

   while (limit && itr != offers_idx.end()) {
      if (itr->buying_item == false) {
         result.emplace_back(*itr);
         limit--;
      }
      itr++;
   }
   return result;
}

vector<offer_object> database_api::list_buy_offers(const offer_id_type lower_id, uint32_t limit) const {
   return my->list_buy_offers(lower_id, limit);
}

vector<offer_object> database_api_impl::list_buy_offers(const offer_id_type lower_id, uint32_t limit) const {
   FC_ASSERT(limit <= api_limit_all_offers_count,
             "Number of querying offers can not be greater than ${configured_limit}",
             ("configured_limit", api_limit_all_offers_count));
   const auto &offers_idx = _db.get_index_type<offer_index>().indices().get<by_id>();
   vector<offer_object> result;
   result.reserve(limit);

   auto itr = offers_idx.lower_bound(lower_id);

   while (limit && itr != offers_idx.end()) {
      if (itr->buying_item == true) {
         result.emplace_back(*itr);
         limit--;
      }
      itr++;
   }

   return result;
}

vector<offer_history_object> database_api::list_offer_history(const offer_history_id_type lower_id, uint32_t limit) const {
   return my->list_offer_history(lower_id, limit);
}

vector<offer_history_object> database_api_impl::list_offer_history(const offer_history_id_type lower_id, uint32_t limit) const {
   FC_ASSERT(limit <= api_limit_all_offers_count,
             "Number of querying offers can not be greater than ${configured_limit}",
             ("configured_limit", api_limit_all_offers_count));
   const auto &oh_idx = _db.get_index_type<offer_history_index>().indices().get<by_id>();
   vector<offer_history_object> result;
   result.reserve(limit);

   auto itr = oh_idx.lower_bound(lower_id);

   while (limit-- && itr != oh_idx.end())
      result.emplace_back(*itr++);

   return result;
}

vector<offer_object> database_api::get_offers_by_issuer(const offer_id_type lower_id, const account_id_type issuer_account_id, uint32_t limit) const {
   return my->get_offers_by_issuer(lower_id, issuer_account_id, limit);
}

vector<offer_object> database_api_impl::get_offers_by_issuer(const offer_id_type lower_id, const account_id_type issuer_account_id, uint32_t limit) const {
   FC_ASSERT(limit <= api_limit_all_offers_count,
             "Number of querying offers can not be greater than ${configured_limit}",
             ("configured_limit", api_limit_all_offers_count));
   const auto &offers_idx = _db.get_index_type<offer_index>().indices().get<by_id>();
   vector<offer_object> result;
   result.reserve(limit);
   auto itr = offers_idx.lower_bound(lower_id);
   while (limit && itr != offers_idx.end()) {
      if (itr->issuer == issuer_account_id) {
         result.emplace_back(*itr);
         limit--;
      }
      itr++;
   }
   return result;
}

vector<offer_object> database_api::get_offers_by_item(const offer_id_type lower_id, const nft_id_type item, uint32_t limit) const {
   return my->get_offers_by_item(lower_id, item, limit);
}

vector<offer_object> database_api_impl::get_offers_by_item(const offer_id_type lower_id, const nft_id_type item, uint32_t limit) const {
   FC_ASSERT(limit <= api_limit_all_offers_count,
             "Number of querying offers can not be greater than ${configured_limit}",
             ("configured_limit", api_limit_all_offers_count));
   const auto &offers_idx = _db.get_index_type<offer_index>().indices().get<by_id>();
   vector<offer_object> result;
   result.reserve(limit);

   auto itr = offers_idx.lower_bound(lower_id);
   while (limit && itr != offers_idx.end()) {
      if (itr->item_ids.find(item) != itr->item_ids.end()) {
         result.emplace_back(*itr);
         limit--;
      }
      itr++;
   }
   return result;
}

vector<offer_history_object> database_api::get_offer_history_by_issuer(const offer_history_id_type lower_id, const account_id_type issuer_account_id, uint32_t limit) const {
   return my->get_offer_history_by_issuer(lower_id, issuer_account_id, limit);
}

vector<offer_history_object> database_api::get_offer_history_by_item(const offer_history_id_type lower_id, const nft_id_type item, uint32_t limit) const {
   return my->get_offer_history_by_item(lower_id, item, limit);
}

vector<offer_history_object> database_api::get_offer_history_by_bidder(const offer_history_id_type lower_id, const account_id_type bidder_account_id, uint32_t limit) const {
   return my->get_offer_history_by_bidder(lower_id, bidder_account_id, limit);
}

vector<offer_history_object> database_api_impl::get_offer_history_by_issuer(const offer_history_id_type lower_id, const account_id_type issuer_account_id, uint32_t limit) const {
   FC_ASSERT(limit <= api_limit_all_offers_count,
             "Number of querying offers can not be greater than ${configured_limit}",
             ("configured_limit", api_limit_all_offers_count));
   const auto &oh_idx = _db.get_index_type<offer_history_index>().indices().get<by_id>();
   vector<offer_history_object> result;
   result.reserve(limit);

   auto itr = oh_idx.lower_bound(lower_id);

   while (limit && itr != oh_idx.end()) {
      if (itr->issuer == issuer_account_id) {
         result.emplace_back(*itr);
         limit--;
      }
      itr++;
   }
   return result;
}

vector<offer_history_object> database_api_impl::get_offer_history_by_item(const offer_history_id_type lower_id, const nft_id_type item, uint32_t limit) const {
   FC_ASSERT(limit <= api_limit_all_offers_count,
             "Number of querying offers can not be greater than ${configured_limit}",
             ("configured_limit", api_limit_all_offers_count));
   const auto &oh_idx = _db.get_index_type<offer_history_index>().indices().get<by_id>();
   vector<offer_history_object> result;
   result.reserve(limit);

   auto itr = oh_idx.lower_bound(lower_id);

   while (limit && itr != oh_idx.end()) {
      if (itr->item_ids.find(item) != itr->item_ids.end()) {
         result.emplace_back(*itr);
         limit--;
      }
      itr++;
   }

   return result;
}

vector<offer_history_object> database_api_impl::get_offer_history_by_bidder(const offer_history_id_type lower_id, const account_id_type bidder_account_id, uint32_t limit) const {
   FC_ASSERT(limit <= api_limit_all_offers_count,
             "Number of querying offers can not be greater than ${configured_limit}",
             ("configured_limit", api_limit_all_offers_count));
   const auto &oh_idx = _db.get_index_type<offer_history_index>().indices().get<by_id>();
   vector<offer_history_object> result;
   result.reserve(limit);

   auto itr = oh_idx.lower_bound(lower_id);

   while (limit && itr != oh_idx.end()) {
      if (itr->bidder && *itr->bidder == bidder_account_id) {
         result.emplace_back(*itr);
         limit--;
      }
      itr++;
   }

   return result;
}

vector<account_role_object> database_api::get_account_roles_by_owner(account_id_type owner) const {
   return my->get_account_roles_by_owner(owner);
}

vector<account_role_object> database_api_impl::get_account_roles_by_owner(account_id_type owner) const {
   const auto &idx_aro = _db.get_index_type<account_role_index>().indices().get<by_owner>();
   auto idx_aro_range = idx_aro.equal_range(owner);
   vector<account_role_object> result;
   for (auto itr = idx_aro_range.first; itr != idx_aro_range.second; ++itr) {
      result.push_back(*itr);
   }
   return result;
}

//////////////////////////////////////////////////////////////////////
//                                                                  //
// Private methods                                                  //
//                                                                  //
//////////////////////////////////////////////////////////////////////

void database_api_impl::broadcast_updates(const vector<variant> &updates) {
   if (updates.size() && _subscribe_callback) {
      auto capture_this = shared_from_this();
      fc::async([capture_this, updates]() {
         if (capture_this->_subscribe_callback)
            capture_this->_subscribe_callback(fc::variant(updates));
      });
   }
}

void database_api_impl::broadcast_market_updates(const market_queue_type &queue) {
   if (queue.size()) {
      auto capture_this = shared_from_this();
      fc::async([capture_this, this, queue]() {
         for (const auto &item : queue) {
            auto sub = _market_subscriptions.find(item.first);
            if (sub != _market_subscriptions.end())
               sub->second(fc::variant(item.second));
         }
      });
   }
}

void database_api_impl::on_objects_removed(const vector<object_id_type> &ids, const vector<const object *> &objs, const flat_set<account_id_type> &impacted_accounts) {
   handle_object_changed(_notify_remove_create, false, ids, impacted_accounts,
                         [objs](object_id_type id) -> const object * {
                            auto it = std::find_if(
                                  objs.begin(), objs.end(),
                                  [id](const object *o) {
                                     return o != nullptr && o->id == id;
                                  });

                            if (it != objs.end())
                               return *it;

                            return nullptr;
                         });
}

void database_api_impl::on_objects_new(const vector<object_id_type> &ids, const flat_set<account_id_type> &impacted_accounts) {
   handle_object_changed(_notify_remove_create, true, ids, impacted_accounts,
                         std::bind(&object_database::find_object, &_db, std::placeholders::_1));
}

void database_api_impl::on_objects_changed(const vector<object_id_type> &ids, const flat_set<account_id_type> &impacted_accounts) {
   handle_object_changed(false, true, ids, impacted_accounts,
                         std::bind(&object_database::find_object, &_db, std::placeholders::_1));
}

void database_api_impl::handle_object_changed(bool force_notify, bool full_object, const vector<object_id_type> &ids, const flat_set<account_id_type> &impacted_accounts, std::function<const object *(object_id_type id)> find_object) {
   if (_subscribe_callback) {
      vector<variant> updates;

      for (auto id : ids) {
         if (force_notify || is_subscribed_to_item(id) || is_impacted_account(impacted_accounts)) {
            if (full_object) {
               auto obj = find_object(id);
               if (obj) {
                  updates.emplace_back(obj->to_variant());
               }
            } else {
               updates.emplace_back(fc::variant(id, 1));
            }
         }
      }

      broadcast_updates(updates);
   }

   if (_market_subscriptions.size()) {
      market_queue_type broadcast_queue;
      /// pushing the future back / popping the prior future if it is complete.
      /// if a connection hangs then this could get backed up and result in
      /// a failure to exit cleanly.
      //fc::async([capture_this,this,updates,market_broadcast_queue](){
      //if( _subscribe_callback )
      //         _subscribe_callback( updates );

      for (auto id : ids) {
         if (id.is<call_order_object>()) {
            enqueue_if_subscribed_to_market<call_order_object>(find_object(id), broadcast_queue, full_object);
         } else if (id.is<limit_order_object>()) {
            enqueue_if_subscribed_to_market<limit_order_object>(find_object(id), broadcast_queue, full_object);
         }
      }

      broadcast_market_updates(broadcast_queue);
   }
}

/** note: this method cannot yield because it is called in the middle of
 * apply a block.
 */
void database_api_impl::on_applied_block() {
   if (_block_applied_callback) {
      auto capture_this = shared_from_this();
      block_id_type block_id = _db.head_block_id();
      fc::async([this, capture_this, block_id]() {
         _block_applied_callback(fc::variant(block_id, 1));
      });
   }

   if (_market_subscriptions.size() == 0)
      return;

   const auto &ops = _db.get_applied_operations();
   map<std::pair<asset_id_type, asset_id_type>, vector<pair<operation, operation_result>>> subscribed_markets_ops;
   for (const optional<operation_history_object> &o_op : ops) {
      if (!o_op.valid())
         continue;
      const operation_history_object &op = *o_op;

      std::pair<asset_id_type, asset_id_type> market;
      switch (op.op.which()) {
      /*  This is sent via the object_changed callback
         case operation::tag<limit_order_create_operation>::value:
            market = op.op.get<limit_order_create_operation>().get_market();
            break;
         */
      case operation::tag<fill_order_operation>::value:
         market = op.op.get<fill_order_operation>().get_market();
         break;
         /*
         case operation::tag<limit_order_cancel_operation>::value:
         */
      default:
         break;
      }
      if (_market_subscriptions.count(market))
         subscribed_markets_ops[market].push_back(std::make_pair(op.op, op.result));
   }
   /// we need to ensure the database_api is not deleted for the life of the async operation
   auto capture_this = shared_from_this();
   fc::async([this, capture_this, subscribed_markets_ops]() {
      for (auto item : subscribed_markets_ops) {
         auto itr = _market_subscriptions.find(item.first);
         if (itr != _market_subscriptions.end())
            itr->second(fc::variant(item.second, GRAPHENE_NET_MAX_NESTED_OBJECTS));
      }
   });
}

}} // namespace graphene::app
