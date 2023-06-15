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
#pragma once

#include <graphene/app/full_account.hpp>

#include <graphene/chain/protocol/types.hpp>

#include <graphene/chain/database.hpp>

#include <graphene/chain/account_object.hpp>
#include <graphene/chain/asset_object.hpp>
#include <graphene/chain/balance_object.hpp>
#include <graphene/chain/betting_market_object.hpp>
#include <graphene/chain/chain_property_object.hpp>
#include <graphene/chain/committee_member_object.hpp>
#include <graphene/chain/confidential_object.hpp>
#include <graphene/chain/event_group_object.hpp>
#include <graphene/chain/event_object.hpp>
#include <graphene/chain/global_betting_statistics_object.hpp>
#include <graphene/chain/market_object.hpp>
#include <graphene/chain/operation_history_object.hpp>
#include <graphene/chain/proposal_object.hpp>
#include <graphene/chain/sidechain_address_object.hpp>
#include <graphene/chain/son_object.hpp>
#include <graphene/chain/son_wallet_object.hpp>
#include <graphene/chain/sport_object.hpp>

#include <graphene/chain/tournament_object.hpp>
#include <graphene/chain/witness_object.hpp>
#include <graphene/chain/worker_object.hpp>

#include <graphene/chain/account_role_object.hpp>
#include <graphene/chain/custom_account_authority_object.hpp>
#include <graphene/chain/custom_permission_object.hpp>
#include <graphene/chain/nft_object.hpp>
#include <graphene/chain/offer_object.hpp>
#include <graphene/chain/voters_info.hpp>
#include <graphene/chain/votes_info.hpp>

#include <graphene/market_history/market_history_plugin.hpp>

#include <fc/api.hpp>
#include <fc/optional.hpp>
#include <fc/variant_object.hpp>

#include <fc/network/ip.hpp>

#include <boost/container/flat_set.hpp>

#include <functional>
#include <map>
#include <memory>
#include <vector>

namespace graphene { namespace app {

using namespace graphene::chain;
using namespace graphene::market_history;
using namespace std;

class database_api_impl;

struct order {
   double price;
   double quote;
   double base;
};

struct order_book {
   string base;
   string quote;
   vector<order> bids;
   vector<order> asks;
};

struct market_ticker {
   string base;
   string quote;
   double latest;
   double lowest_ask;
   double highest_bid;
   double percent_change;
   double base_volume;
   double quote_volume;
};

struct market_volume {
   string base;
   string quote;
   double base_volume;
   double quote_volume;
};

struct market_trade {
   fc::time_point_sec date;
   double price;
   double amount;
   double value;
};

struct gpos_info {
   double vesting_factor;
   asset award;
   share_type total_amount;
   uint32_t current_subperiod;
   fc::time_point_sec last_voted_time;
   share_type allowed_withdraw_amount;
   share_type account_vested_balance;
};

struct version_info {
   string version;
   string git_revision;
   string built;
   string openssl;
   string boost;
};

/**
 * @brief The database_api class implements the RPC API for the chain database.
 *
 * This API exposes accessors on the database which query state tracked by a blockchain validating node. This API is
 * read-only; all modifications to the database must be performed via transactions. Transactions are broadcast via
 * the @ref network_broadcast_api.
 */
class database_api {
public:
   database_api(graphene::chain::database &db);
   ~database_api();

   /////////////
   // Objects //
   /////////////

   /**
    * @brief Get the objects corresponding to the provided IDs
    * @param ids IDs of the objects to retrieve
    * @return The objects retrieved, in the order they are mentioned in ids
    *
    * If any of the provided IDs does not map to an object, a null variant is returned in its position.
    */
   fc::variants get_objects(const vector<object_id_type> &ids) const;

   ///////////////////
   // Subscriptions //
   ///////////////////

   void set_subscribe_callback(std::function<void(const variant &)> cb, bool clear_filter);
   void set_pending_transaction_callback(std::function<void(const variant &)> cb);
   void set_block_applied_callback(std::function<void(const variant &block_id)> cb);
   /**
    * @brief Stop receiving any notifications
    *
    * This unsubscribes from all subscribed markets and objects.
    */
   void cancel_all_subscriptions();

   /////////////////////////////
   // Blocks and transactions //
   /////////////////////////////

   /**
    * @brief Retrieve a block header
    * @param block_num Height of the block whose header should be returned
    * @return header of the referenced block, or null if no matching block was found
    */
   optional<block_header> get_block_header(uint32_t block_num) const;

   /**
      * @brief Retrieve multiple block header by block numbers
      * @param block_num vector containing heights of the block whose header should be returned
      * @return array of headers of the referenced blocks, or null if no matching block was found
      */
   map<uint32_t, optional<block_header>> get_block_header_batch(const vector<uint32_t> block_nums) const;

   /**
    * @brief Retrieve a full, signed block
    * @param block_num Height of the block to be returned
    * @return the referenced block, or null if no matching block was found
    */
   optional<signed_block> get_block(uint32_t block_num) const;

   /**
    * @brief Retrieve a list of signed blocks
    * @param block_num_from start
    * @param block_num_to end
    * @return list of referenced blocks
    */
   vector<optional<signed_block>> get_blocks(uint32_t block_num_from, uint32_t block_num_to) const;

   /**
    * @brief used to fetch an individual transaction.
    */
   processed_transaction get_transaction(uint32_t block_num, uint32_t trx_in_block) const;

   /**
    * If the transaction has not expired, this method will return the transaction for the given ID or
    * it will return NULL if it is not known.  Just because it is not known does not mean it wasn't
    * included in the blockchain.
    */
   optional<signed_transaction> get_recent_transaction_by_id(const transaction_id_type &id) const;

   /////////////
   // Globals //
   /////////////

   /**
    * @brief Retrieve the @ref version_info associated with the witness node
    */
   version_info get_version_info() const;

   /**
    * @brief Retrieve the @ref chain_property_object associated with the chain
    */
   chain_property_object get_chain_properties() const;

   /**
    * @brief Retrieve the current @ref global_property_object
    */
   global_property_object get_global_properties() const;

   /**
    * @brief Retrieve compile-time constants
    */
   fc::variant_object get_config() const;

   /**
    * @brief Get the chain ID
    */
   chain_id_type get_chain_id() const;

   /**
    * @brief Retrieve the current @ref dynamic_global_property_object
    */
   dynamic_global_property_object get_dynamic_global_properties() const;

   //////////
   // Keys //
   //////////

   vector<vector<account_id_type>> get_key_references(vector<public_key_type> key) const;

   /**
      * Determine whether a textual representation of a public key
      * (in Base-58 format) is *currently* linked
      * to any *registered* (i.e. non-stealth) account on the blockchain
      * @param public_key Public key
      * @return Whether a public key is known
      */
   bool is_public_key_registered(string public_key) const;

   //////////////
   // Accounts //
   //////////////

   /**
    * @brief Get account object from a name or ID
    * @param name_or_id name or ID of the account
    * @return Account ID
    *
    */
   account_id_type get_account_id_from_string(const std::string &name_or_id) const;

   /**
    * @brief Get a list of accounts by ID or Name
    * @param account_ids IDs of the accounts to retrieve
    * @return The accounts corresponding to the provided IDs
    *
    * This function has semantics identical to @ref get_objects
    */
   vector<optional<account_object>> get_accounts(const vector<std::string> &account_names_or_ids) const;

   /**
    * @brief Fetch all objects relevant to the specified accounts and subscribe to updates
    * @param callback Function to call with updates
    * @param names_or_ids Each item must be the name or ID of an account to retrieve
    * @return Map of string from @ref names_or_ids to the corresponding account
    *
    * This function fetches all relevant objects for the given accounts, and subscribes to updates to the given
    * accounts. If any of the strings in @ref names_or_ids cannot be tied to an account, that input will be
    * ignored. All other accounts will be retrieved and subscribed.
    *
    */
   std::map<string, full_account> get_full_accounts(const vector<string> &names_or_ids, bool subscribe);

   optional<account_object> get_account_by_name(string name) const;

   /**
    *  @return all accounts that referr to the key or account id in their owner or active authorities.
    */
   vector<account_id_type> get_account_references(const std::string account_name_or_id) const;

   /**
    * @brief Get a list of accounts by name
    * @param account_names Names of the accounts to retrieve
    * @return The accounts holding the provided names
    *
    * This function has semantics identical to @ref get_objects
    */
   vector<optional<account_object>> lookup_account_names(const vector<string> &account_names) const;

   /**
    * @brief Get names and IDs for registered accounts
    * @param lower_bound_name Lower bound of the first name to return
    * @param limit Maximum number of results to return -- must not exceed 1000
    * @return Map of account names to corresponding IDs
    */
   map<string, account_id_type> lookup_accounts(const string &lower_bound_name, uint32_t limit) const;

   //////////////
   // Balances //
   //////////////

   /**
    * @brief Get an account's balances in various assets
    * @param id ID of the account to get balances for
    * @param assets IDs of the assets to get balances of; if empty, get all assets account has a balance in
    * @return Balances of the account
    */
   vector<asset> get_account_balances(const std::string &account_name_or_id,
                                      const flat_set<asset_id_type> &assets) const;

   /// Semantically equivalent to @ref get_account_balances, but takes a name instead of an ID.
   vector<asset> get_named_account_balances(const std::string &name, const flat_set<asset_id_type> &assets) const;

   /** @return all unclaimed balance objects for a set of addresses */
   vector<balance_object> get_balance_objects(const vector<address> &addrs) const;

   vector<asset> get_vested_balances(const vector<balance_id_type> &objs) const;

   vector<vesting_balance_object> get_vesting_balances(const std::string account_id_or_name) const;

   /**
    * @brief Get the total number of accounts registered with the blockchain
    */
   uint64_t get_account_count() const;

   ////////////
   // Assets //
   ////////////

   /**
    * @brief Get asset ID from an asset symbol or ID
    * @param symbol_or_id symbol name or ID of the asset
    * @return asset ID
    */
   asset_id_type get_asset_id_from_string(const std::string &symbol_or_id) const;

   /**
    * @brief Get a list of assets by ID
    * @param asset_symbols_or_ids IDs or names of the assets to retrieve
    * @return The assets corresponding to the provided IDs
    *
    * This function has semantics identical to @ref get_objects
    */
   vector<optional<asset_object>> get_assets(const vector<std::string> &asset_symbols_or_ids) const;

   /**
    * @brief Get assets alphabetically by symbol name
    * @param lower_bound_symbol Lower bound of symbol names to retrieve
    * @param limit Maximum number of assets to fetch (must not exceed 100)
    * @return The assets found
    */
   vector<asset_object> list_assets(const string &lower_bound_symbol, uint32_t limit) const;

   /**
    * @brief Get a list of assets by symbol
    * @param asset_symbols Symbols or stringified IDs of the assets to retrieve
    * @return The assets corresponding to the provided symbols or IDs
    *
    * This function has semantics identical to @ref get_objects
    */
   vector<optional<asset_object>> lookup_asset_symbols(const vector<string> &symbols_or_ids) const;

   /**
    * @brief Get assets count
    * @return The assets count
    */
   uint64_t get_asset_count() const;

   ////////////////////
   // Lottery Assets //
   ////////////////////
   /**
    * @brief Get a list of lottery assets
    * @return The lottery assets between start and stop ids
    */
   vector<asset_object> get_lotteries(asset_id_type stop = asset_id_type(),
                                      unsigned limit = 100,
                                      asset_id_type start = asset_id_type()) const;
   vector<asset_object> get_account_lotteries(account_id_type issuer,
                                              asset_id_type stop,
                                              unsigned limit,
                                              asset_id_type start) const;
   sweeps_vesting_balance_object get_sweeps_vesting_balance_object(account_id_type account) const;
   asset get_sweeps_vesting_balance_available_for_claim(account_id_type account) const;
   /**
    * @brief Get balance of lottery assets
    */
   asset get_lottery_balance(asset_id_type lottery_id) const;

   /////////////////////
   // Peerplays    //
   /////////////////////

   /**
    * @brief Get global betting statistics
    */
   global_betting_statistics_object get_global_betting_statistics() const;

   /**
    * @brief Get a list of all sports
    */
   vector<sport_object> list_sports() const;

   /**
    * @brief Return a list of all event groups for a sport (e.g. all soccer leagues in soccer)
    */
   vector<event_group_object> list_event_groups(sport_id_type sport_id) const;

   /**
    * @brief Return a list of all events in an event group
    */
   vector<event_object> list_events_in_group(event_group_id_type event_group_id) const;

   /**
    * @brief Return a list of all betting market groups for an event
    */
   vector<betting_market_group_object> list_betting_market_groups(event_id_type) const;

   /**
    * @brief Return a list of all betting markets for a betting market group
    */
   vector<betting_market_object> list_betting_markets(betting_market_group_id_type) const;

   /**
    * @brief Return a list of all unmatched bets for a given account on a specific betting market
    */
   vector<bet_object> get_unmatched_bets_for_bettor(betting_market_id_type, account_id_type) const;

   /**
    * @brief Return a list of all unmatched bets for a given account (includes bets on all markets)
    */
   vector<bet_object> get_all_unmatched_bets_for_bettor(account_id_type) const;

   /////////////////////
   // Markets / feeds //
   /////////////////////

   /**
    * @brief Get limit orders in a given market
    * @param a ID of asset being sold
    * @param b ID of asset being purchased
    * @param limit Maximum number of orders to retrieve
    * @return The limit orders, ordered from least price to greatest
    */
   vector<limit_order_object> get_limit_orders(const std::string &a, const std::string &b, uint32_t limit) const;

   /**
    * @brief Get call orders in a given asset
    * @param a ID or name of asset being called
    * @param limit Maximum number of orders to retrieve
    * @return The call orders, ordered from earliest to be called to latest
    */
   vector<call_order_object> get_call_orders(const std::string &a, uint32_t limit) const;

   /**
    * @brief Get forced settlement orders in a given asset
    * @param a ID or name of asset being settled
    * @param limit Maximum number of orders to retrieve
    * @return The settle orders, ordered from earliest settlement date to latest
    */
   vector<force_settlement_object> get_settle_orders(const std::string &a, uint32_t limit) const;

   /**
    *  @return all open margin positions for a given account id.
    */
   vector<call_order_object> get_margin_positions(const std::string account_id_or_name) const;

   /**
    * @brief Request notification when the active orders in the market between two assets changes
    * @param callback Callback method which is called when the market changes
    * @param a First asset ID or name
    * @param b Second asset ID or name
    *
    * Callback will be passed a variant containing a vector<pair<operation, operation_result>>. The vector will
    * contain, in order, the operations which changed the market, and their results.
    */
   void subscribe_to_market(std::function<void(const variant &)> callback,
                            const std::string &a, const std::string &b);

   /**
    * @brief Unsubscribe from updates to a given market
    * @param a First asset ID or name
    * @param b Second asset ID or name
    */
   void unsubscribe_from_market(const std::string &a, const std::string &b);

   /**
    * @brief Returns the ticker for the market assetA:assetB
    * @param a String name of the first asset
    * @param b String name of the second asset
    * @return The market ticker for the past 24 hours.
    */
   market_ticker get_ticker(const string &base, const string &quote) const;

   /**
    * @brief Returns the 24 hour volume for the market assetA:assetB
    * @param a String name of the first asset
    * @param b String name of the second asset
    * @return The market volume over the past 24 hours
    */
   market_volume get_24_volume(const string &base, const string &quote) const;

   /**
    * @brief Returns the order book for the market base:quote
    * @param base String name of the first asset
    * @param quote String name of the second asset
    * @param depth of the order book. Up to depth of each asks and bids, capped at 50. Prioritizes most moderate of each
    * @return Order book of the market
    */
   order_book get_order_book(const string &base, const string &quote, unsigned limit = 50) const;

   /**
    * @brief Returns recent trades for the market assetA:assetB
    * Note: Currentlt, timezone offsets are not supported. The time must be UTC.
    * @param a String name of the first asset
    * @param b String name of the second asset
    * @param stop Stop time as a UNIX timestamp
    * @param limit Number of trasactions to retrieve, capped at 100
    * @param start Start time as a UNIX timestamp
    * @return Recent transactions in the market
    */
   vector<market_trade> get_trade_history(const string &base, const string &quote, fc::time_point_sec start, fc::time_point_sec stop, unsigned limit = 100) const;

   ///////////////
   // Witnesses //
   ///////////////

   /**
    * @brief Get a list of witnesses by ID
    * @param witness_ids IDs of the witnesses to retrieve
    * @return The witnesses corresponding to the provided IDs
    *
    * This function has semantics identical to @ref get_objects
    */
   vector<optional<witness_object>> get_witnesses(const vector<witness_id_type> &witness_ids) const;

   /**
    * @brief Get the witness owned by a given account
    * @param account The ID of the account whose witness should be retrieved
    * @return The witness object, or null if the account does not have a witness
    */
   fc::optional<witness_object> get_witness_by_account_id(account_id_type account) const;

   /**
    * @brief Get the witness owned by a given account
    * @param account_id_or_name The ID or name of the account whose witness should be retrieved
    * @return The witness object, or null if the account does not have a witness
    */
   fc::optional<witness_object> get_witness_by_account(const std::string account_name_or_id) const;

   /**
    * @brief Get names and IDs for registered witnesses
    * @param lower_bound_name Lower bound of the first name to return
    * @param limit Maximum number of results to return -- must not exceed 1000
    * @return Map of witness names to corresponding IDs
    */
   map<string, witness_id_type> lookup_witness_accounts(const string &lower_bound_name, uint32_t limit) const;

   /**
    * @brief Get the total number of witnesses registered with the blockchain
    */
   uint64_t get_witness_count() const;

   ///////////////////////
   // Committee members //
   ///////////////////////

   /**
    * @brief Get a list of committee_members by ID
    * @param committee_member_ids IDs of the committee_members to retrieve
    * @return The committee_members corresponding to the provided IDs
    *
    * This function has semantics identical to @ref get_objects
    */
   vector<optional<committee_member_object>> get_committee_members(const vector<committee_member_id_type> &committee_member_ids) const;

   /**
    * @brief Get the committee_member owned by a given account
    * @param account The ID of the account whose committee_member should be retrieved
    * @return The committee_member object, or null if the account does not have a committee_member
    */
   fc::optional<committee_member_object> get_committee_member_by_account_id(account_id_type account) const;

   /**
    * @brief Get the committee_member owned by a given account
    * @param account_id_or_name The ID or name of the account whose committee_member should be retrieved
    * @return The committee_member object, or null if the account does not have a committee_member
    */
   fc::optional<committee_member_object> get_committee_member_by_account(const std::string account_id_or_name) const;

   /**
    * @brief Get names and IDs for registered committee_members
    * @param lower_bound_name Lower bound of the first name to return
    * @param limit Maximum number of results to return -- must not exceed 1000
    * @return Map of committee_member names to corresponding IDs
    */
   map<string, committee_member_id_type> lookup_committee_member_accounts(const string &lower_bound_name, uint32_t limit) const;

   /**
    * @brief Get the total number of committee_members registered with the blockchain
    */
   uint64_t get_committee_member_count() const;

   /////////////////
   // SON members //
   /////////////////

   /**
    * @brief Get a list of SONs by ID
    * @param son_ids IDs of the SONs to retrieve
    * @return The SONs corresponding to the provided IDs
    *
    * This function has semantics identical to @ref get_objects
    */
   vector<optional<son_object>> get_sons(const vector<son_id_type> &son_ids) const;

   /**
    * @brief Get the SON owned by a given account
    * @param account The ID of the account whose SON should be retrieved
    * @return The SON object, or null if the account does not have a SON
    */
   fc::optional<son_object> get_son_by_account_id(account_id_type account) const;

   /**
    * @brief Get the SON owned by a given account
    * @param account_id_or_name The ID of the account whose SON should be retrieved
    * @return The SON object, or null if the account does not have a SON
    */
   fc::optional<son_object> get_son_by_account(const std::string account_id_or_name) const;

   /**
    * @brief Get names and IDs for registered SONs
    * @param lower_bound_name Lower bound of the first name to return
    * @param limit Maximum number of results to return -- must not exceed 1000
    * @return Map of SON names to corresponding IDs
    */
   map<string, son_id_type> lookup_son_accounts(const string &lower_bound_name, uint32_t limit) const;

   /**
    * @brief Get the total number of SONs registered with the blockchain
    */
   uint64_t get_son_count() const;

   /////////////////////////
   // SON Wallets      //
   /////////////////////////

   /**
    * @brief Get active SON wallet
    * @return Active SON wallet object
    */
   optional<son_wallet_object> get_active_son_wallet();

   /**
    * @brief Get SON wallet that was active for a given time point
    * @param time_point Time point
    * @return SON wallet object, for the wallet that was active for a given time point
    */
   optional<son_wallet_object> get_son_wallet_by_time_point(time_point_sec time_point);

   /**
    * @brief Get full list of SON wallets
    * @param limit Maximum number of results to return
    * @return A list of SON wallet objects
    */
   vector<optional<son_wallet_object>> get_son_wallets(uint32_t limit);

   /////////////////////////
   // Sidechain Addresses //
   /////////////////////////

   /**
    * @brief Get a list of sidechain addresses
    * @param sidechain_address_ids IDs of the sidechain addresses to retrieve
    * @return The sidechain accounts corresponding to the provided IDs
    *
    * This function has semantics identical to @ref get_objects
    */
   vector<optional<sidechain_address_object>> get_sidechain_addresses(const vector<sidechain_address_id_type> &sidechain_address_ids) const;

   /**
    * @brief Get the sidechain addresses for a given account
    * @param account The ID of the account whose sidechain addresses should be retrieved
    * @return The sidechain addresses objects, or null if the account does not have a sidechain addresses
    */
   vector<optional<sidechain_address_object>> get_sidechain_addresses_by_account(account_id_type account) const;

   /**
    * @brief Get the sidechain addresses for a given sidechain
    * @param sidechain Sidechain for which addresses should be retrieved
    * @return The sidechain addresses objects, or null if the sidechain does not have any addresses
    */
   vector<optional<sidechain_address_object>> get_sidechain_addresses_by_sidechain(sidechain_type sidechain) const;

   /**
    * @brief Get the sidechain addresses for a given account and sidechain
    * @param account The ID of the account whose sidechain addresses should be retrieved
    * @param sidechain Sidechain for which address should be retrieved
    * @return The sidechain addresses objects, or null if the account does not have a sidechain addresses for a given sidechain
    */
   fc::optional<sidechain_address_object> get_sidechain_address_by_account_and_sidechain(account_id_type account, sidechain_type sidechain) const;

   /**
    * @brief Get the total number of sidechain addresses registered with the blockchain
    */
   uint64_t get_sidechain_addresses_count() const;

   /////////////
   // Workers //
   /////////////

   /**
    * @brief Get a list of workers by ID
    * @param worker_ids IDs of the workers to retrieve
    * @return The workers corresponding to the provided IDs
    *
    * This function has semantics identical to @ref get_objects
    */
   vector<optional<worker_object>> get_workers(const vector<worker_id_type> &worker_ids) const;

   /**
    * @brief Return the worker objects associated with this account.
    * @param account The ID of the account whose workers should be retrieved
    * @return The worker object or null if the account does not have a worker
    */
   vector<worker_object> get_workers_by_account_id(account_id_type account) const;

   /**
    * @brief Return the worker objects associated with this account.
    * @param account_id_or_name The ID or name of the account whose workers should be retrieved
    * @return The worker object or null if the account does not have a worker
    */
   vector<worker_object> get_workers_by_account(const std::string account_id_or_name) const;

   /**
    * @brief Get names and IDs for registered workers
    * @param lower_bound_name Lower bound of the first name to return
    * @param limit Maximum number of results to return -- must not exceed 1000
    * @return Map of worker names to corresponding IDs
    */
   map<string, worker_id_type> lookup_worker_accounts(const string &lower_bound_name, uint32_t limit) const;

   /**
    * @brief Get the total number of workers registered with the blockchain
    */
   uint64_t get_worker_count() const;

   ///////////
   // Votes //
   ///////////

   /**
    *  @brief Given a set of votes, return the objects they are voting for.
    *
    *  This will be a mixture of committee_member_object, witness_objects, and worker_objects
    *
    *  The results will be in the same order as the votes.  Null will be returned for
    *  any vote ids that are not found.
    */
   vector<variant> lookup_vote_ids(const vector<vote_id_type> &votes) const;

   /**
    * @brief Get a list of vote_id_type that ID votes for
    * @param account_name_or_id ID or name of the account to get votes for
    * @return The list of vote_id_type ID votes for
    *
    */
   vector<vote_id_type> get_votes_ids(const string &account_name_or_id) const;

   /**
    * @brief Return the objects account_name_or_id votes for
    * @param account_name_or_id ID or name of the account to get votes for
    * @return The votes_info account_name_or_id votes for
    *
    */
   votes_info get_votes(const string &account_name_or_id) const;

   /**
    *
    * @brief Get a list of accounts that votes for vote_id
    * @param vote_id We search accounts that vote for this ID
    * @return The accounts that votes for provided ID
    *
    */
   vector<account_object> get_voters_by_id(const vote_id_type &vote_id) const;

   /**
    * @brief Return the accounts that votes for account_name_or_id
    * @param account_name_or_id ID or name of the account to get voters for
    * @return The voters_info for account_name_or_id
    *
    */
   voters_info get_voters(const string &account_name_or_id) const;

   ////////////////////////////
   // Authority / validation //
   ////////////////////////////

   /// @brief Get a hexdump of the serialized binary form of a transaction
   std::string get_transaction_hex(const signed_transaction &trx) const;

   /**
    *  This API will take a partially signed transaction and a set of public keys that the owner has the ability to sign for
    *  and return the minimal subset of public keys that should add signatures to the transaction.
    */
   set<public_key_type> get_required_signatures(const signed_transaction &trx, const flat_set<public_key_type> &available_keys) const;

   /**
    *  This method will return the set of all public keys that could possibly sign for a given transaction.  This call can
    *  be used by wallets to filter their set of public keys to just the relevant subset prior to calling @ref get_required_signatures
    *  to get the minimum subset.
    */
   set<public_key_type> get_potential_signatures(const signed_transaction &trx) const;
   set<address> get_potential_address_signatures(const signed_transaction &trx) const;

   /**
    * @return true of the @ref trx has all of the required signatures, otherwise throws an exception
    */
   bool verify_authority(const signed_transaction &trx) const;

   /**
    * @return true if the signers have enough authority to authorize an account
    */
   bool verify_account_authority(const string &name_or_id, const flat_set<public_key_type> &signers) const;

   /**
    *  Validates a transaction against the current state without broadcasting it on the network.
    */
   processed_transaction validate_transaction(const signed_transaction &trx) const;

   /**
    *  For each operation calculate the required fee in the specified asset type.  If the asset type does
    *  not have a valid core_exchange_rate
    */
   vector<fc::variant> get_required_fees(const vector<operation> &ops, const std::string &asset_id_or_symbol) const;

   ///////////////////////////
   // Proposed transactions //
   ///////////////////////////

   /**
    *  @return the set of proposed transactions relevant to the specified account id.
    */
   vector<proposal_object> get_proposed_transactions(const std::string account_id_or_name) const;

   //////////////////////
   // Blinded balances //
   //////////////////////

   /**
    *  @return the set of blinded balance objects by commitment ID
    */
   vector<blinded_balance_object> get_blinded_balances(const flat_set<commitment_type> &commitments) const;

   /////////////////
   // Tournaments //
   /////////////////
   /**
    * @return the list of tournaments in the given state
    */
   vector<tournament_object> get_tournaments_in_state(tournament_state state, uint32_t limit) const;

   vector<tournament_object> get_tournaments(tournament_id_type stop = tournament_id_type(),
                                             unsigned limit = 100,
                                             tournament_id_type start = tournament_id_type());

   vector<tournament_object> get_tournaments_by_state(tournament_id_type stop = tournament_id_type(),
                                                      unsigned limit = 100,
                                                      tournament_id_type start = tournament_id_type(),
                                                      tournament_state state = tournament_state::accepting_registrations);

   /**
    * @return the list of tournaments that a given account is registered to play in
    */
   vector<tournament_id_type> get_registered_tournaments(account_id_type account_filter, uint32_t limit) const;

   //////////
   // GPOS //
   //////////
   /**
    * @return account and network GPOS information
    */
   gpos_info get_gpos_info(const account_id_type account) const;

   //////////
   // RBAC //
   //////////
   /**
    * @return account and custom permissions/account-authorities info
    */
   vector<custom_permission_object> get_custom_permissions(const account_id_type account) const;
   fc::optional<custom_permission_object> get_custom_permission_by_name(const account_id_type account, const string &permission_name) const;
   vector<custom_account_authority_object> get_custom_account_authorities(const account_id_type account) const;
   vector<custom_account_authority_object> get_custom_account_authorities_by_permission_id(const custom_permission_id_type permission_id) const;
   vector<custom_account_authority_object> get_custom_account_authorities_by_permission_name(const account_id_type account, const string &permission_name) const;
   vector<authority> get_active_custom_account_authorities_by_operation(const account_id_type account, int operation_type) const;

   /////////
   // NFT //
   /////////
   /**
    * @brief Returns the number of NFT owned by account
    * @param owner Owner account ID
    * @return Number of NFTs owned by account
    */
   uint64_t nft_get_balance(const account_id_type owner) const;

   /**
    * @brief Returns the NFT owner
    * @param token_id NFT ID
    * @return NFT owner account ID
    */
   optional<account_id_type> nft_owner_of(const nft_id_type token_id) const;

   /**
    * @brief Returns the NFT approved account ID
    * @param token_id NFT ID
    * @return NFT approved account ID
    */
   optional<account_id_type> nft_get_approved(const nft_id_type token_id) const;

   /**
    * @brief Returns operator approved state for all NFT owned by owner
    * @param owner NFT owner account ID
    * @param token_id NFT ID
    * @return True if operator is approved for all NFT owned by owner, else False
    */
   bool nft_is_approved_for_all(const account_id_type owner, const account_id_type operator_) const;

   /**
    * @brief Returns NFT name from NFT metadata
    * @param nft_metadata_id NFT metadata ID
    * @return NFT name
    */
   string nft_get_name(const nft_metadata_id_type nft_metadata_id) const;

   /**
    * @brief Returns NFT symbol from NFT metadata
    * @param nft_metadata_id NFT metadata ID
    * @return NFT symbol
    */
   string nft_get_symbol(const nft_metadata_id_type nft_metadata_id) const;

   /**
    * @brief Returns NFT URI
    * @param token_id NFT ID
    * @return NFT URI
    */
   string nft_get_token_uri(const nft_id_type token_id) const;

   /**
    * @brief Returns total number of NFTs assigned to NFT metadata
    * @param nft_metadata_id NFT metadata ID
    * @return Total number of NFTs assigned to NFT metadata
    */
   uint64_t nft_get_total_supply(const nft_metadata_id_type nft_metadata_id) const;

   /**
    * @brief Returns NFT by index from NFT metadata
    * @param nft_metadata_id NFT metadata ID
    * @param token_idx NFT index in the list of tokens
    * @return NFT symbol
    */
   nft_object nft_token_by_index(const nft_metadata_id_type nft_metadata_id, const uint64_t token_idx) const;

   /**
    * @brief Returns NFT by owner and index
    * @param nft_metadata_id NFT metadata ID
    * @param owner NFT owner
    * @param token_idx NFT index in the list of tokens
    * @return NFT object
    */
   nft_object nft_token_of_owner_by_index(const nft_metadata_id_type nft_metadata_id, const account_id_type owner, const uint64_t token_idx) const;

   /**
    * @brief Returns list of all available NTF's
    * @return List of all available NFT's
    */
   vector<nft_object> nft_get_all_tokens() const;

   /**
    * @brief Returns NFT's owned by owner
    * @param owner NFT owner
    * @return List of NFT owned by owner
    */
   vector<nft_object> nft_get_tokens_by_owner(const account_id_type owner) const;

   //////////////////
   // MARKET PLACE //
   //////////////////
   vector<offer_object> list_offers(const offer_id_type lower_id, uint32_t limit) const;
   vector<offer_object> list_sell_offers(const offer_id_type lower_id, uint32_t limit) const;
   vector<offer_object> list_buy_offers(const offer_id_type lower_id, uint32_t limit) const;
   vector<offer_history_object> list_offer_history(const offer_history_id_type lower_id, uint32_t limit) const;
   vector<offer_object> get_offers_by_issuer(const offer_id_type lower_id, const account_id_type issuer_account_id, uint32_t limit) const;
   vector<offer_object> get_offers_by_item(const offer_id_type lower_id, const nft_id_type item, uint32_t limit) const;
   vector<offer_history_object> get_offer_history_by_issuer(const offer_history_id_type lower_id, const account_id_type issuer_account_id, uint32_t limit) const;
   vector<offer_history_object> get_offer_history_by_item(const offer_history_id_type lower_id, const nft_id_type item, uint32_t limit) const;
   vector<offer_history_object> get_offer_history_by_bidder(const offer_history_id_type lower_id, const account_id_type bidder_account_id, uint32_t limit) const;

   //////////////////
   // ACCOUNT ROLE //
   //////////////////
   vector<account_role_object> get_account_roles_by_owner(account_id_type owner) const;

private:
   std::shared_ptr<database_api_impl> my;
};

}} // namespace graphene::app

extern template class fc::api<graphene::app::database_api>;

// clang-format off

FC_REFLECT(graphene::app::order, (price)(quote)(base));
FC_REFLECT(graphene::app::order_book, (base)(quote)(bids)(asks));
FC_REFLECT(graphene::app::market_ticker, (base)(quote)(latest)(lowest_ask)(highest_bid)(percent_change)(base_volume)(quote_volume));
FC_REFLECT(graphene::app::market_volume, (base)(quote)(base_volume)(quote_volume));
FC_REFLECT(graphene::app::market_trade, (date)(price)(amount)(value));
FC_REFLECT(graphene::app::gpos_info, (vesting_factor)(award)(total_amount)(current_subperiod)(last_voted_time)(allowed_withdraw_amount)(account_vested_balance));
FC_REFLECT(graphene::app::version_info, (version)(git_revision)(built)(openssl)(boost));

FC_API(graphene::app::database_api,
   // Objects
   (get_objects)

   // Subscriptions
   (set_subscribe_callback)
   (set_pending_transaction_callback)
   (set_block_applied_callback)
   (cancel_all_subscriptions)

   // Blocks and transactions
   (get_block_header)
   (get_block_header_batch)
   (get_block)
   (get_blocks)
   (get_transaction)
   (get_recent_transaction_by_id)

   // Globals
   (get_version_info)
   (get_chain_properties)
   (get_global_properties)
   (get_config)
   (get_chain_id)
   (get_dynamic_global_properties)

   // Keys
   (get_key_references)
   (is_public_key_registered)

   // Accounts
   (get_account_id_from_string)
   (get_accounts)
   (get_full_accounts)
   (get_account_by_name)
   (get_account_references)
   (lookup_account_names)
   (lookup_accounts)
   (get_account_count)

   // Balances
   (get_account_balances)
   (get_named_account_balances)
   (get_balance_objects)
   (get_vested_balances)
   (get_vesting_balances)

   // Assets
   (get_assets)
   (list_assets)
   (lookup_asset_symbols)
   (get_asset_count)
   (get_asset_id_from_string)

   // Peerplays
   (list_sports)
   (get_global_betting_statistics)
   (list_event_groups)
   (list_events_in_group)
   (list_betting_market_groups)
   (list_betting_markets)
   (get_unmatched_bets_for_bettor)
   (get_all_unmatched_bets_for_bettor)

   // Sweeps
   (get_lotteries)
   (get_account_lotteries)
   (get_lottery_balance)
   (get_sweeps_vesting_balance_object)
   (get_sweeps_vesting_balance_available_for_claim)
   
   // Markets / feeds
   (get_order_book)
   (get_limit_orders)
   (get_call_orders)
   (get_settle_orders)
   (get_margin_positions)
   (subscribe_to_market)
   (unsubscribe_from_market)
   (get_ticker)
   (get_24_volume)
   (get_trade_history)

   // Witnesses
   (get_witnesses)
   (get_witness_by_account_id)
   (get_witness_by_account)
   (lookup_witness_accounts)
   (get_witness_count)

   // Committee members
   (get_committee_members)
   (get_committee_member_by_account_id)
   (get_committee_member_by_account)
   (lookup_committee_member_accounts)
   (get_committee_member_count)

   // SON members
   (get_sons)
   (get_son_by_account_id)
   (get_son_by_account)
   (lookup_son_accounts)
   (get_son_count)

   // SON wallets
   (get_active_son_wallet)
   (get_son_wallet_by_time_point)
   (get_son_wallets)

   // Sidechain addresses
   (get_sidechain_addresses)
   (get_sidechain_addresses_by_account)
   (get_sidechain_addresses_by_sidechain)
   (get_sidechain_address_by_account_and_sidechain)
   (get_sidechain_addresses_count)

   // Workers
   (get_workers)
   (get_workers_by_account_id)
   (get_workers_by_account)
   (lookup_worker_accounts)
   (get_worker_count)

   // Votes
   (lookup_vote_ids)
   (get_votes_ids)
   (get_votes)
   (get_voters_by_id)
   (get_voters)

   // Authority / validation
   (get_transaction_hex)
   (get_required_signatures)
   (get_potential_signatures)
   (get_potential_address_signatures)
   (verify_authority)
   (verify_account_authority)
   (validate_transaction)
   (get_required_fees)

   // Proposed transactions
   (get_proposed_transactions)

   // Blinded balances
   (get_blinded_balances)

   // Tournaments
   (get_tournaments_in_state)
   (get_tournaments_by_state)
   (get_tournaments )
   (get_registered_tournaments)

   // gpos
   (get_gpos_info)

   //rbac
   (get_custom_permissions)
   (get_custom_permission_by_name)
   (get_custom_account_authorities)
   (get_custom_account_authorities_by_permission_id)
   (get_custom_account_authorities_by_permission_name)
   (get_active_custom_account_authorities_by_operation)

   // NFT
   (nft_get_balance)
   (nft_owner_of)
   (nft_get_approved)
   (nft_is_approved_for_all)
   (nft_get_name)
   (nft_get_symbol)
   (nft_get_token_uri)
   (nft_get_total_supply)
   (nft_token_by_index)
   (nft_token_of_owner_by_index)
   (nft_get_all_tokens)
   (nft_get_tokens_by_owner)

   // Marketplace
   (list_offers)
   (list_sell_offers)
   (list_buy_offers)
   (list_offer_history)
   (get_offers_by_issuer)
   (get_offers_by_item)
   (get_offer_history_by_issuer)
   (get_offer_history_by_item)
   (get_offer_history_by_bidder)

   // Account Roles
   (get_account_roles_by_owner)
)

// clang-format on
