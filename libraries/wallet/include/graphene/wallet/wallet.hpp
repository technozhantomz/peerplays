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

#include <graphene/app/api.hpp>
#include <graphene/utilities/key_conversion.hpp>

using namespace graphene::app;
using namespace graphene::chain;
using namespace graphene::utilities;
using namespace graphene::bookie;
using namespace std;

namespace fc
{
   void to_variant( const account_multi_index_type& accts, variant& vo, uint32_t max_depth );
   void from_variant( const variant &var, account_multi_index_type &vo, uint32_t max_depth );
}

namespace graphene { namespace wallet {

typedef uint16_t transaction_handle_type;

/**
 * This class takes a variant and turns it into an object
 * of the given type, with the new operator.
 */

object* create_object( const variant& v );

struct plain_keys
{
   map<public_key_type, string>  keys;
   fc::sha512                    checksum;
};

struct brain_key_info
{
   string brain_priv_key;
   string wif_priv_key;
   public_key_type pub_key;
};

enum authority_type
{
   owner,
   active
};

/**
 *  Contains the confirmation receipt the sender must give the receiver and
 *  the meta data about the receipt that helps the sender identify which receipt is
 *  for the receiver and which is for the change address.
 */
struct blind_confirmation
{
   struct output
   {
      string                          label;
      public_key_type                 pub_key;
      stealth_confirmation::memo_data decrypted_memo;
      stealth_confirmation            confirmation;
      authority                       auth;
      string                          confirmation_receipt;
   };

   signed_transaction     trx;
   vector<output>         outputs;
};

struct blind_balance
{
   asset                     amount;
   public_key_type           from; ///< the account this balance came from
   public_key_type           to; ///< the account this balance is logically associated with
   public_key_type           one_time_key; ///< used to derive the authority key and blinding factor
   fc::sha256                blinding_factor;
   fc::ecc::commitment_type  commitment;
   bool                      used = false;
};

struct blind_receipt
{
   std::pair<public_key_type,fc::time_point>        from_date()const { return std::make_pair(from_key,date); }
   std::pair<public_key_type,fc::time_point>        to_date()const   { return std::make_pair(to_key,date);   }
   std::tuple<public_key_type,asset_id_type,bool>   to_asset_used()const   { return std::make_tuple(to_key,amount.asset_id,used);   }
   const commitment_type& commitment()const        { return data.commitment; }

   fc::time_point                  date;
   public_key_type                 from_key;
   string                          from_label;
   public_key_type                 to_key;
   string                          to_label;
   asset                           amount;
   string                          memo;
   authority                       control_authority;
   stealth_confirmation::memo_data data;
   bool                            used = false;
   stealth_confirmation            conf;
};

struct by_from;
struct by_to;
struct by_to_asset_used;
struct by_commitment;

typedef multi_index_container< blind_receipt,
   indexed_by<
      ordered_unique< tag<by_commitment>, const_mem_fun< blind_receipt, const commitment_type&, &blind_receipt::commitment > >,
      ordered_unique< tag<by_to>, const_mem_fun< blind_receipt, std::pair<public_key_type,fc::time_point>, &blind_receipt::to_date > >,
      ordered_non_unique< tag<by_to_asset_used>, const_mem_fun< blind_receipt, std::tuple<public_key_type,asset_id_type,bool>, &blind_receipt::to_asset_used > >,
      ordered_unique< tag<by_from>, const_mem_fun< blind_receipt, std::pair<public_key_type,fc::time_point>, &blind_receipt::from_date > >
   >
> blind_receipt_index_type;


struct key_label
{
   string          label;
   public_key_type key;
};


struct by_label;
struct by_key;
typedef multi_index_container<
   key_label,
   indexed_by<
      ordered_unique< tag<by_label>, member< key_label, string, &key_label::label > >,
      ordered_unique< tag<by_key>, member< key_label, public_key_type, &key_label::key > >
   >
> key_label_index_type;


struct wallet_data
{
   /** Chain ID this wallet is used with */
   chain_id_type chain_id;
   account_multi_index_type my_accounts;
   /// @return IDs of all accounts in @ref my_accounts
   vector<object_id_type> my_account_ids()const
   {
      vector<object_id_type> ids;
      ids.reserve(my_accounts.size());
      std::transform(my_accounts.begin(), my_accounts.end(), std::back_inserter(ids),
                     [](const account_object& ao) { return ao.id; });
      return ids;
   }
   /// Add acct to @ref my_accounts, or update it if it is already in @ref my_accounts
   /// @return true if the account was newly inserted; false if it was only updated
   bool update_account(const account_object& acct)
   {
      auto& idx = my_accounts.get<by_id>();
      auto itr = idx.find(acct.get_id());
      if( itr != idx.end() )
      {
         idx.replace(itr, acct);
         return false;
      } else {
         idx.insert(acct);
         return true;
      }
   }

   /** encrypted keys */
   vector<char>              cipher_keys;

   /** map an account to a set of extra keys that have been imported for that account */
   map<account_id_type, set<public_key_type> >  extra_keys;

   // map of account_name -> base58_private_key for
   //    incomplete account regs
   map<string, vector<string> > pending_account_registrations;
   map<string, string> pending_witness_registrations;

   key_label_index_type                                              labeled_keys;
   blind_receipt_index_type                                          blind_receipts;

   std::map<rock_paper_scissors_throw_commit, rock_paper_scissors_throw_reveal> committed_game_moves;

   string                    ws_server = "ws://localhost:8090";
   string                    ws_user;
   string                    ws_password;
};

struct exported_account_keys
{
    string account_name;
    vector<vector<char>> encrypted_private_keys;
    vector<public_key_type> public_keys;
};

struct exported_keys
{
    fc::sha512 password_checksum;
    vector<exported_account_keys> account_keys;
};

struct approval_delta
{
   vector<string> active_approvals_to_add;
   vector<string> active_approvals_to_remove;
   vector<string> owner_approvals_to_add;
   vector<string> owner_approvals_to_remove;
   vector<string> key_approvals_to_add;
   vector<string> key_approvals_to_remove;
};

struct worker_vote_delta
{
   flat_set<worker_id_type> vote_for;
   flat_set<worker_id_type> vote_against;
   flat_set<worker_id_type> vote_abstain;
};

struct signed_block_with_info : public signed_block
{
   signed_block_with_info();
   signed_block_with_info( const signed_block& block );
   signed_block_with_info( const signed_block_with_info& block ) = default;

   block_id_type block_id;
   public_key_type signing_key;
   vector< transaction_id_type > transaction_ids;
};

struct vesting_balance_object_with_info : public vesting_balance_object
{
   vesting_balance_object_with_info();
   vesting_balance_object_with_info( const vesting_balance_object& vbo, fc::time_point_sec now );
   vesting_balance_object_with_info( const vesting_balance_object_with_info& vbo ) = default;

   /**
    * How much is allowed to be withdrawn.
    */
   asset allowed_withdraw;

   /**
    * The time at which allowed_withdrawal was calculated.
    */
   fc::time_point_sec allowed_withdraw_time;
};

namespace detail {
class wallet_api_impl;
}

/***
 * A utility class for performing various state-less actions that are related to wallets
 */
class utility {
   public:
      /**
       * Derive any number of *possible* owner keys from a given brain key.
       *
       * NOTE: These keys may or may not match with the owner keys of any account.
       * This function is merely intended to assist with account or key recovery.
       *
       * @see suggest_brain_key()
       *
       * @param brain_key    Brain key
       * @param number_of_desired_keys  Number of desired keys
       * @return A list of keys that are deterministically derived from the brainkey
       */
      static vector<brain_key_info> derive_owner_keys_from_brain_key(string brain_key, int number_of_desired_keys = 1);
};

struct operation_detail {
   string                   memo;
   string                   description;
   operation_history_object op;
};

/**
 * This wallet assumes it is connected to the database server with a high-bandwidth, low-latency connection and
 * performs minimal caching. This API could be provided locally to be used by a web interface.
 */
class wallet_api
{
   public:
      wallet_api( const wallet_data& initial_data, fc::api<login_api> rapi );
      virtual ~wallet_api();

      bool copy_wallet_file( string destination_filename );

      fc::ecc::private_key derive_private_key(const std::string& prefix_string, int sequence_number) const;

      variant                           info();
      /** Returns info such as client version, git version of graphene/fc, version of boost, openssl.
       * @returns compile time info and client and dependencies versions
       */
      variant_object                    about() const;
      optional<signed_block_with_info>    get_block( uint32_t num );
      vector<optional<signed_block>>      get_blocks(uint32_t block_num_from, uint32_t block_num_to)const;
      /** Returns the number of accounts registered on the blockchain
       * @returns the number of registered accounts
       */
      uint64_t                          get_account_count()const;
      /** Lists all accounts controlled by this wallet.
       * This returns a list of the full account objects for all accounts whose private keys
       * we possess.
       * @returns a list of account objects
       */
      vector<account_object>            list_my_accounts();
      /** Lists all accounts registered in the blockchain.
       * This returns a list of all account names and their account ids, sorted by account name.
       *
       * Use the \c lowerbound and limit parameters to page through the list.  To retrieve all accounts,
       * start by setting \c lowerbound to the empty string \c "", and then each iteration, pass
       * the last account name returned as the \c lowerbound for the next \c list_accounts() call.
       *
       * @param lowerbound the name of the first account to return.  If the named account does not exist,
       *                   the list will start at the account that comes after \c lowerbound
       * @param limit the maximum number of accounts to return (max: 1000)
       * @returns a list of accounts mapping account names to account ids
       */
      map<string,account_id_type>       list_accounts(const string& lowerbound, uint32_t limit);
      /** List the balances of an account.
       * Each account can have multiple balances, one for each type of asset owned by that
       * account.  The returned list will only contain assets for which the account has a
       * nonzero balance
       * @param id the name or id of the account whose balances you want
       * @returns a list of the given account's balances
       */
      vector<asset>                     list_account_balances(const string& id);
      /** Lists all assets registered on the blockchain.
       *
       * To list all assets, pass the empty string \c "" for the lowerbound to start
       * at the beginning of the list, and iterate as necessary.
       *
       * @param lowerbound  the symbol of the first asset to include in the list.
       * @param limit the maximum number of assets to return (max: 100)
       * @returns the list of asset objects, ordered by symbol
       */
      vector<asset_object>              list_assets(const string& lowerbound, uint32_t limit)const;

      /** Returns assets count registered on the blockchain.
       *
       * @returns assets count
       */
      uint64_t get_asset_count()const;


      vector<asset_object>              get_lotteries( asset_id_type stop = asset_id_type(),
                                                       unsigned limit = 100,
                                                       asset_id_type start = asset_id_type() )const;
      vector<asset_object>              get_account_lotteries( account_id_type issuer,
                                                               asset_id_type stop = asset_id_type(),
                                                               unsigned limit = 100,
                                                               asset_id_type start = asset_id_type() )const;

      asset get_lottery_balance( asset_id_type lottery_id ) const;
      /** Returns the most recent operations on the named account.
       *
       * This returns a list of operation history objects, which describe activity on the account.
       *
       * @param name the name or id of the account
       * @param limit the number of entries to return (starting from the most recent)
       * @returns a list of \c operation_history_objects
       */
      vector<operation_detail>  get_account_history(string name, int limit)const;

      /** Returns the relative operations on the named account from start number.
       *
       * @param name the name or id of the account
       * @param stop Sequence number of earliest operation.
       * @param limit the number of entries to return
       * @param start  the sequence number where to start looping back throw the history
       * @returns a list of \c operation_history_objects
       */
      vector<operation_detail>  get_relative_account_history(string name, uint32_t stop, int limit, uint32_t start)const;

      vector<account_balance_object> list_core_accounts()const;

      vector<bucket_object>             get_market_history(string symbol, string symbol2, uint32_t bucket, fc::time_point_sec start, fc::time_point_sec end)const;
      vector<limit_order_object>        get_limit_orders(string a, string b, uint32_t limit)const;
      vector<call_order_object>         get_call_orders(string a, uint32_t limit)const;
      vector<force_settlement_object>   get_settle_orders(string a, uint32_t limit)const;

      /** Returns the block chain's slowly-changing settings.
       * This object contains all of the properties of the blockchain that are fixed
       * or that change only once per maintenance interval (daily) such as the
       * current list of witnesses, committee_members, block interval, etc.
       * @see \c get_dynamic_global_properties() for frequently changing properties
       * @returns the global properties
       */
      global_property_object            get_global_properties() const;

      /** Returns the block chain's rapidly-changing properties.
       * The returned object contains information that changes every block interval
       * such as the head block number, the next witness, etc.
       * @see \c get_global_properties() for less-frequently changing properties
       * @returns the dynamic global properties
       */
      dynamic_global_property_object    get_dynamic_global_properties() const;

      /** Returns information about the given account.
       *
       * @param account_name_or_id the name or id of the account to provide information about
       * @returns the public account data stored in the blockchain
       */
      account_object                    get_account(string account_name_or_id) const;

      /** Returns information about the given asset.
       * @param asset_name_or_id the symbol or id of the asset in question
       * @returns the information about the asset stored in the block chain
       */
      asset_object                      get_asset(string asset_name_or_id) const;

      /** Returns the BitAsset-specific data for a given asset.
       * Market-issued assets's behavior are determined both by their "BitAsset Data" and
       * their basic asset data, as returned by \c get_asset().
       * @param asset_name_or_id the symbol or id of the BitAsset in question
       * @returns the BitAsset-specific data for this asset
       */
      asset_bitasset_data_object        get_bitasset_data(string asset_name_or_id)const;

      /** Lookup the id of a named account.
       * @param account_name_or_id the name of the account to look up
       * @returns the id of the named account
       */
      account_id_type                   get_account_id(string account_name_or_id) const;

      /**
       * Lookup the id of a named asset.
       * @param asset_name_or_id the symbol of an asset to look up
       * @returns the id of the given asset
       */
      asset_id_type                     get_asset_id(string asset_name_or_id) const;

      /**
       * Returns the blockchain object corresponding to the given id.
       *
       * This generic function can be used to retrieve any object from the blockchain
       * that is assigned an ID.  Certain types of objects have specialized convenience
       * functions to return their objects -- e.g., assets have \c get_asset(), accounts
       * have \c get_account(), but this function will work for any object.
       *
       * @param id the id of the object to return
       * @returns the requested object
       */
      variant                           get_object(object_id_type id) const;

      /** Returns the current wallet filename.
       *
       * This is the filename that will be used when automatically saving the wallet.
       *
       * @see set_wallet_filename()
       * @return the wallet filename
       */
      string                            get_wallet_filename() const;

      /**
       * Get the WIF private key corresponding to a public key.  The
       * private key must already be in the wallet.
       */
      string                            get_private_key( public_key_type pubkey )const;

      /**
       * @ingroup Transaction Builder API
       */
      transaction_handle_type begin_builder_transaction();
      /**
       * @ingroup Transaction Builder API
       */
      void add_operation_to_builder_transaction(transaction_handle_type transaction_handle, const operation& op);
      /**
       * @ingroup Transaction Builder API
       */
      void replace_operation_in_builder_transaction(transaction_handle_type handle,
                                                    unsigned operation_index,
                                                    const operation& new_op);
      /**
       * @ingroup Transaction Builder API
       */
      asset set_fees_on_builder_transaction(transaction_handle_type handle, string fee_asset = GRAPHENE_SYMBOL);
      /**
       * @ingroup Transaction Builder API
       */
      transaction preview_builder_transaction(transaction_handle_type handle);
      /**
       * @ingroup Transaction Builder API
       */
      signed_transaction sign_builder_transaction(transaction_handle_type transaction_handle, bool broadcast = true);
      /** Broadcast signed transaction
       * @param tx signed transaction
       * @returns the transaction ID along with the signed transaction.
       */
      pair<transaction_id_type,signed_transaction> broadcast_transaction(signed_transaction tx);
      /**
       * @ingroup Transaction Builder API
       */
      signed_transaction propose_builder_transaction(
          transaction_handle_type handle,
          time_point_sec expiration = time_point::now() + fc::minutes(1),
          uint32_t review_period_seconds = 0,
          bool broadcast = true
         );

      signed_transaction propose_builder_transaction2(
         transaction_handle_type handle,
         string account_name_or_id,
         time_point_sec expiration = time_point::now() + fc::minutes(1),
         uint32_t review_period_seconds = 0,
         bool broadcast = true
        );

      /**
       * @ingroup Transaction Builder API
       */
      void remove_builder_transaction(transaction_handle_type handle);

      /** Checks whether the wallet has just been created and has not yet had a password set.
       *
       * Calling \c set_password will transition the wallet to the locked state.
       * @return true if the wallet is new
       * @ingroup Wallet Management
       */
      bool    is_new()const;

      /** Checks whether the wallet is locked (is unable to use its private keys).
       *
       * This state can be changed by calling \c lock() or \c unlock().
       * @return true if the wallet is locked
       * @ingroup Wallet Management
       */
      bool    is_locked()const;

      /** Locks the wallet immediately.
       * @ingroup Wallet Management
       */
      void    lock();

      /** Unlocks the wallet.
       *
       * The wallet remain unlocked until the \c lock is called
       * or the program exits.
       * @param password the password previously set with \c set_password()
       * @ingroup Wallet Management
       */
      void    unlock(string password);

      /** Sets a new password on the wallet.
       *
       * The wallet must be either 'new' or 'unlocked' to
       * execute this command.
       * @ingroup Wallet Management
       */
      void    set_password(string password);

      /** Dumps all private keys owned by the wallet.
       *
       * The keys are printed in WIF format.  You can import these keys into another wallet
       * using \c import_key()
       * @returns a map containing the private keys, indexed by their public key
       */
      map<public_key_type, string> dump_private_keys();

      /** Returns a list of all commands supported by the wallet API.
       *
       * This lists each command, along with its arguments and return types.
       * For more detailed help on a single command, use \c get_help()
       *
       * @returns a multi-line string suitable for displaying on a terminal
       */
      string  help()const;

      /** Returns detailed help on a single API command.
       * @param method the name of the API command you want help with
       * @returns a multi-line string suitable for displaying on a terminal
       */
      string  gethelp(const string& method)const;

      /** Loads a specified Graphene wallet.
       *
       * The current wallet is closed before the new wallet is loaded.
       *
       * @warning This does not change the filename that will be used for future
       * wallet writes, so this may cause you to overwrite your original
       * wallet unless you also call \c set_wallet_filename()
       *
       * @param wallet_filename the filename of the wallet JSON file to load.
       *                        If \c wallet_filename is empty, it reloads the
       *                        existing wallet file
       * @returns true if the specified wallet is loaded
       */
      bool    load_wallet_file(string wallet_filename = "");

      /** Quitting from Peerplays wallet.
       *
       * The current wallet will be closed.
       */
      void    quit();

      /** Saves the current wallet to the given filename.
       *
       * @warning This does not change the wallet filename that will be used for future
       * writes, so think of this function as 'Save a Copy As...' instead of
       * 'Save As...'.  Use \c set_wallet_filename() to make the filename
       * persist.
       * @param wallet_filename the filename of the new wallet JSON file to create
       *                        or overwrite.  If \c wallet_filename is empty,
       *                        save to the current filename.
       */
      void    save_wallet_file(string wallet_filename = "");

      /** Sets the wallet filename used for future writes.
       *
       * This does not trigger a save, it only changes the default filename
       * that will be used the next time a save is triggered.
       *
       * @param wallet_filename the new filename to use for future saves
       */
      void    set_wallet_filename(string wallet_filename);

      /** Suggests a safe brain key to use for creating your account.
       * \c create_account_with_brain_key() requires you to specify a 'brain key',
       * a long passphrase that provides enough entropy to generate cyrptographic
       * keys.  This function will suggest a suitably random string that should
       * be easy to write down (and, with effort, memorize).
       * @returns a suggested brain_key
       */
      brain_key_info suggest_brain_key()const;

     /**
      * Derive any number of *possible* owner keys from a given brain key.
      *
      * NOTE: These keys may or may not match with the owner keys of any account.
      * This function is merely intended to assist with account or key recovery.
      *
      * @see suggest_brain_key()
      *
      * @param brain_key    Brain key
      * @param number_of_desired_keys  Number of desired keys
      * @return A list of keys that are deterministically derived from the brainkey
      */
     vector<brain_key_info> derive_owner_keys_from_brain_key(string brain_key, int number_of_desired_keys = 1) const;

     /**
      * Determine whether a textual representation of a public key
      * (in Base-58 format) is *currently* linked
      * to any *registered* (i.e. non-stealth) account on the blockchain
      * @param public_key Public key
      * @return Whether a public key is known
      */
     bool is_public_key_registered(string public_key) const;

      /**
       * Gets private key from password
       *
       * @param account Account name
       * @param role Account role - active | owner | memo
       * @param password Account password
       * @return public/private key pair
       */
      pair<public_key_type,string>  get_private_key_from_password( string account, string role, string password )const;

      /** Converts a signed_transaction in JSON form to its binary representation.
       *
       * TODO: I don't see a broadcast_transaction() function, do we need one?
       *
       * @param tx the transaction to serialize
       * @returns the be hex encoded form of the serialized transaction
       */
      string serialize_transaction(signed_transaction tx) const;

      /** Imports the private key for an existing account.
       *
       * The private key must match either an owner key or an active key for the
       * named account.
       *
       * @see dump_private_keys()
       *
       * @param account_name_or_id the account owning the key
       * @param wif_key the private key in WIF format
       * @returns true if the key was imported
       */
      bool import_key(string account_name_or_id, string wif_key);

      map<string, bool> import_accounts( string filename, string password );

      bool import_account_keys( string filename, string password, string src_account_name, string dest_account_name );

      /**
       * This call will construct transaction(s) that will claim all balances controled
       * by wif_keys and deposit them into the given account.
       */
      vector< signed_transaction > import_balance( string account_name_or_id, const vector<string>& wif_keys, bool broadcast );

      /** Transforms a brain key to reduce the chance of errors when re-entering the key from memory.
       *
       * This takes a user-supplied brain key and normalizes it into the form used
       * for generating private keys.  In particular, this upper-cases all ASCII characters
       * and collapses multiple spaces into one.
       * @param s the brain key as supplied by the user
       * @returns the brain key in its normalized form
       */
      string normalize_brain_key(string s) const;

      /** Registers a third party's account on the blockckain.
       *
       * This function is used to register an account for which you do not own the private keys.
       * When acting as a registrar, an end user will generate their own private keys and send
       * you the public keys.  The registrar will use this function to register the account
       * on behalf of the end user.
       *
       * @see create_account_with_brain_key()
       *
       * @param name the name of the account, must be unique on the blockchain.  Shorter names
       *             are more expensive to register; the rules are still in flux, but in general
       *             names of more than 8 characters with at least one digit will be cheap.
       * @param owner the owner key for the new account
       * @param active the active key for the new account
       * @param registrar_account the account which will pay the fee to register the user
       * @param referrer_account the account who is acting as a referrer, and may receive a
       *                         portion of the user's transaction fees.  This can be the
       *                         same as the registrar_account if there is no referrer.
       * @param referrer_percent the percentage (0 - 100) of the new user's transaction fees
       *                         not claimed by the blockchain that will be distributed to the
       *                         referrer; the rest will be sent to the registrar.  Will be
       *                         multiplied by GRAPHENE_1_PERCENT when constructing the transaction.
       * @param broadcast true to broadcast the transaction on the network
       * @returns the signed transaction registering the account
       */
      signed_transaction register_account(string name,
                                          public_key_type owner,
                                          public_key_type active,
                                          string  registrar_account,
                                          string  referrer_account,
                                          uint32_t referrer_percent,
                                          bool broadcast = false);

      /** Updates account public keys
       *
       * @param name the name of the existing account
       * @param old_owner the owner key for the named account to be replaced
       * @param new_owner the owner key for the named account to be set as new
       * @param old_active the active key for the named account to be replaced
       * @param new_active the active key for the named account to be set as new
       * @param broadcast true to broadcast the transaction on the network
       * @returns the signed transaction updating account public keys
       */
      signed_transaction update_account_keys(string name,
                                             public_key_type old_owner,
                                             public_key_type new_owner,
                                             public_key_type old_active,
                                             public_key_type new_active,
                                             bool broadcast = false);

      /**
       * This method updates the key of an authority for an exisiting account.
       * Warning: You can create impossible authorities using this method. The method
       * will fail if you create an impossible owner authority, but will allow impossible
       * active and posting authorities.
       *
       * @param account_name The name of the account whose authority you wish to update
       * @param type The authority type. e.g. owner or active
       * @param key The public key to add to the authority
       * @param weight The weight the key should have in the authority. A weight of 0 indicates the removal of the key.
       * @param broadcast true if you wish to broadcast the transaction.
       */
      signed_transaction update_account_auth_key(string account_name,
                                                 authority_type type,
                                                 public_key_type key,
                                                 weight_type weight,
                                                 bool broadcast);

      /**
       *  Upgrades an account to prime status.
       *  This makes the account holder a 'lifetime member'.
       *
       *  @todo there is no option for annual membership
       *  @param name the name or id of the account to upgrade
       * @param broadcast true to broadcast the transaction on the network
       * @returns the signed transaction upgrading the account
       */
      signed_transaction upgrade_account(string name, bool broadcast);

      /** Creates a new account and registers it on the blockchain.
       *
       * @todo why no referrer_percent here?
       *
       * @see suggest_brain_key()
       * @see register_account()
       *
       * @param brain_key the brain key used for generating the account's private keys
       * @param account_name the name of the account, must be unique on the blockchain.  Shorter names
       *                     are more expensive to register; the rules are still in flux, but in general
       *                     names of more than 8 characters with at least one digit will be cheap.
       * @param registrar_account the account which will pay the fee to register the user
       * @param referrer_account the account who is acting as a referrer, and may receive a
       *                         portion of the user's transaction fees.  This can be the
       *                         same as the registrar_account if there is no referrer.
       * @param broadcast true to broadcast the transaction on the network
       * @returns the signed transaction registering the account
       */
      signed_transaction create_account_with_brain_key(string brain_key,
                                                       string account_name,
                                                       string registrar_account,
                                                       string referrer_account,
                                                       bool broadcast = false);

      /** Transfer an amount from one account to another.
       * @param from the name or id of the account sending the funds
       * @param to the name or id of the account receiving the funds
       * @param amount the amount to send (in nominal units -- to send half of a BTS, specify 0.5)
       * @param asset_symbol the symbol or id of the asset to send
       * @param memo a memo to attach to the transaction.  The memo will be encrypted in the
       *             transaction and readable for the receiver.  There is no length limit
       *             other than the limit imposed by maximum transaction size, but transaction
       *             increase with transaction size
       * @param broadcast true to broadcast the transaction on the network
       * @returns the signed transaction transferring funds
       */
      signed_transaction transfer(string from,
                                  string to,
                                  string amount,
                                  string asset_symbol,
                                  string memo,
                                  bool broadcast = false);

      /**
       *  This method works just like transfer, except it always broadcasts and
       *  returns the transaction ID along with the signed transaction.
       */
      pair<transaction_id_type,signed_transaction> transfer2(string from,
                                                             string to,
                                                             string amount,
                                                             string asset_symbol,
                                                             string memo ) {
         auto trx = transfer( from, to, amount, asset_symbol, memo, true );
         return std::make_pair(trx.id(),trx);
      }


      /**
       *  This method is used to convert a JSON transaction to its transactin ID.
       */
      transaction_id_type get_transaction_id( const signed_transaction& trx )const { return trx.id(); }


      /** These methods are used for stealth transfers */
      ///@{
      /**
       *  This method can be used to set the label for a public key
       *
       *  @note No two keys can have the same label.
       *
       *  @return true if the label was set, otherwise false
       */
      bool                        set_key_label( public_key_type, string label );
      string                      get_key_label( public_key_type )const;

      /**
       *  Generates a new blind account for the given brain key and assigns it the given label.
       */
      public_key_type             create_blind_account( string label, string brain_key  );

      /**
       * @return the total balance of all blinded commitments that can be claimed by the
       * given account key or label
       */
      vector<asset>                get_blind_balances( string key_or_label );
      /** @return all blind accounts */
      map<string,public_key_type> get_blind_accounts()const;
      /** @return all blind accounts for which this wallet has the private key */
      map<string,public_key_type> get_my_blind_accounts()const;
      /** @return the public key associated with the given label */
      public_key_type             get_public_key( string label )const;
      ///@}

      /**
       * @return all blind receipts to/form a particular account
       */
      vector<blind_receipt> blind_history( string key_or_account );

      /**
       *  Given a confirmation receipt, this method will parse it for a blinded balance and confirm
       *  that it exists in the blockchain.  If it exists then it will report the amount received and
       *  who sent it.
       *
       *  @param confirmation_receipt - a base58 encoded stealth confirmation
       *  @param opt_from - if not  empty and the sender is a unknown public key, then the unknown public key will be given the label opt_from
       *  @param opt_memo - optional memo
       */
      blind_receipt receive_blind_transfer( string confirmation_receipt, string opt_from, string opt_memo );

      /**
       *  Transfers a public balance from from_account_id_or_name to one or more blinded balances using a
       *  stealth transfer.
       *
       *  @param from_account_id_or_name account id or name
       *  @param asset_symbol asset symbol
       *  @param to_amounts map from key or label to amount
       *  @param broadcast true to broadcast the transaction on the network
       *  @returns blind confirmation structure
       */
      blind_confirmation transfer_to_blind( string from_account_id_or_name,
                                            string asset_symbol,
                                            vector<pair<string, string>> to_amounts,
                                            bool broadcast = false );

      /**
       * Transfers funds from a set of blinded balances to a public account balance.
       */
      blind_confirmation transfer_from_blind(
                                            string from_blind_account_key_or_label,
                                            string to_account_id_or_name,
                                            string amount,
                                            string asset_symbol,
                                            bool broadcast = false );

      /**
       *  Used to transfer from one set of blinded balances to another
       */
      blind_confirmation blind_transfer( string from_key_or_label,
                                         string to_key_or_label,
                                         string amount,
                                         string symbol,
                                         bool broadcast = false );

      /** Place a limit order attempting to sell one asset for another.
       *
       * Buying and selling are the same operation on Graphene; if you want to buy BTS
       * with USD, you should sell USD for BTS.
       *
       * The blockchain will attempt to sell the \c symbol_to_sell for as
       * much \c symbol_to_receive as possible, as long as the price is at
       * least \c min_to_receive / \c amount_to_sell.
       *
       * In addition to the transaction fees, market fees will apply as specified
       * by the issuer of both the selling asset and the receiving asset as
       * a percentage of the amount exchanged.
       *
       * If either the selling asset or the receiving asset is whitelist
       * restricted, the order will only be created if the seller is on
       * the whitelist of the restricted asset type.
       *
       * Market orders are matched in the order they are included
       * in the block chain.
       *
       * @todo Allow order expiration to be set here.  Document default/max expiration time
       *
       * @param seller_account the account providing the asset being sold, and which will
       *                       receive the proceeds of the sale.
       * @param amount_to_sell the amount of the asset being sold to sell (in nominal units)
       * @param symbol_to_sell the name or id of the asset to sell
       * @param min_to_receive the minimum amount you are willing to receive in return for
       *                       selling the entire amount_to_sell
       * @param symbol_to_receive the name or id of the asset you wish to receive
       * @param timeout_sec if the order does not fill immediately, this is the length of
       *                    time the order will remain on the order books before it is
       *                    cancelled and the un-spent funds are returned to the seller's
       *                    account
       * @param fill_or_kill if true, the order will only be included in the blockchain
       *                     if it is filled immediately; if false, an open order will be
       *                     left on the books to fill any amount that cannot be filled
       *                     immediately.
       * @param broadcast true to broadcast the transaction on the network
       * @returns the signed transaction selling the funds
       */
      signed_transaction sell_asset(string seller_account,
                                    string amount_to_sell,
                                    string   symbol_to_sell,
                                    string min_to_receive,
                                    string   symbol_to_receive,
                                    uint32_t timeout_sec = 0,
                                    bool     fill_or_kill = false,
                                    bool     broadcast = false);

      /** Place a limit order attempting to sell one asset for another.
       *
       * This API call abstracts away some of the details of the sell_asset call to be more
       * user friendly. All orders placed with sell never timeout and will not be killed if they
       * cannot be filled immediately. If you wish for one of these parameters to be different,
       * then sell_asset should be used instead.
       *
       * @param seller_account the account providing the asset being sold, and which will
       *                       receive the processed of the sale.
       * @param base The name or id of the asset to sell.
       * @param quote The name or id of the asset to recieve.
       * @param rate The rate in base:quote at which you want to sell.
       * @param amount The amount of base you want to sell.
       * @param broadcast true to broadcast the transaction on the network.
       * @returns The signed transaction selling the funds.
       */
      signed_transaction sell( string seller_account,
                               string base,
                               string quote,
                               double rate,
                               double amount,
                               bool broadcast );

      /** Place a limit order attempting to buy one asset with another.
       *
       * This API call abstracts away some of the details of the sell_asset call to be more
       * user friendly. All orders placed with buy never timeout and will not be killed if they
       * cannot be filled immediately. If you wish for one of these parameters to be different,
       * then sell_asset should be used instead.
       *
       * @param buyer_account The account buying the asset for another asset.
       * @param base The name or id of the asset to buy.
       * @param quote The name or id of the asset being offered as payment.
       * @param rate The rate in base:quote at which you want to buy.
       * @param amount the amount of base you want to buy.
       * @param broadcast true to broadcast the transaction on the network.
       * @returns The signed transaction buying the funds.
       */
      signed_transaction buy( string buyer_account,
                              string base,
                              string quote,
                              double rate,
                              double amount,
                              bool broadcast );

      /** Borrow an asset or update the debt/collateral ratio for the loan.
       *
       * This is the first step in shorting an asset.  Call \c sell_asset() to complete the short.
       *
       * @param borrower_name the name or id of the account associated with the transaction.
       * @param amount_to_borrow the amount of the asset being borrowed.  Make this value
       *                         negative to pay back debt.
       * @param asset_symbol the symbol or id of the asset being borrowed.
       * @param amount_of_collateral the amount of the backing asset to add to your collateral
       *        position.  Make this negative to claim back some of your collateral.
       *        The backing asset is defined in the \c bitasset_options for the asset being borrowed.
       * @param broadcast true to broadcast the transaction on the network
       * @returns the signed transaction borrowing the asset
       */
      signed_transaction borrow_asset(string borrower_name, string amount_to_borrow, string asset_symbol,
                                      string amount_of_collateral, bool broadcast = false);

      /** Cancel an existing order
       *
       * @param order_id the id of order to be cancelled
       * @param broadcast true to broadcast the transaction on the network
       * @returns the signed transaction canceling the order
       */
      signed_transaction cancel_order(object_id_type order_id, bool broadcast = false);

      /** Creates a new user-issued or market-issued asset.
       *
       * Many options can be changed later using \c update_asset()
       *
       * Right now this function is difficult to use because you must provide raw JSON data
       * structures for the options objects, and those include prices and asset ids.
       *
       * @param issuer the name or id of the account who will pay the fee and become the
       *               issuer of the new asset.  This can be updated later
       * @param symbol the ticker symbol of the new asset
       * @param precision the number of digits of precision to the right of the decimal point,
       *                  must be less than or equal to 12
       * @param common asset options required for all new assets.
       *               Note that core_exchange_rate technically needs to store the asset ID of
       *               this new asset. Since this ID is not known at the time this operation is
       *               created, create this price as though the new asset has instance ID 1, and
       *               the chain will overwrite it with the new asset's ID.
       * @param bitasset_opts options specific to BitAssets.  This may be null unless the
       *               \c market_issued flag is set in common.flags
       * @param broadcast true to broadcast the transaction on the network
       * @returns the signed transaction creating a new asset
       */
      signed_transaction create_asset(string issuer,
                                      string symbol,
                                      uint8_t precision,
                                      asset_options common,
                                      fc::optional<bitasset_options> bitasset_opts,
                                      bool broadcast = false);

      signed_transaction create_lottery(  string issuer,
                                          string symbol,
                                          asset_options common,
                                          lottery_asset_options lottery_opts,
                                          bool broadcast = false);

      signed_transaction buy_ticket( asset_id_type lottery, account_id_type buyer, uint64_t tickets_to_buy );

      /** Issue new shares of an asset.
       *
       * @param to_account the name or id of the account to receive the new shares
       * @param amount the amount to issue, in nominal units
       * @param symbol the ticker symbol of the asset to issue
       * @param memo a memo to include in the transaction, readable by the recipient
       * @param broadcast true to broadcast the transaction on the network
       * @returns the signed transaction issuing the new shares
       */
      signed_transaction issue_asset(string to_account, string amount,
                                     string symbol,
                                     string memo,
                                     bool broadcast = false);

      /** Update the core options on an asset.
       * There are a number of options which all assets in the network use. These options are
       * enumerated in the asset_object::asset_options struct. This command is used to update
       * these options for an existing asset.
       *
       * @note This operation cannot be used to update BitAsset-specific options. For these options,
       * \c update_bitasset() instead.
       *
       * @param symbol the name or id of the asset to update
       * @param new_issuer if changing the asset's issuer, the name or id of the new issuer.
       *                   null if you wish to remain the issuer of the asset
       * @param new_options the new asset_options object, which will entirely replace the existing
       *                    options.
       * @param broadcast true to broadcast the transaction on the network
       * @returns the signed transaction updating the asset
       */
      signed_transaction update_asset(string symbol,
                                      optional<string> new_issuer,
                                      asset_options new_options,
                                      bool broadcast = false);

      /** Update the options specific to a BitAsset.
       *
       * BitAssets have some options which are not relevant to other asset types. This operation is used to update those
       * options an an existing BitAsset.
       *
       * @see update_asset()
       *
       * @param symbol the name or id of the asset to update, which must be a market-issued asset
       * @param new_options the new bitasset_options object, which will entirely replace the existing
       *                    options.
       * @param broadcast true to broadcast the transaction on the network
       * @returns the signed transaction updating the bitasset
       */
      signed_transaction update_bitasset(string symbol,
                                         bitasset_options new_options,
                                         bool broadcast = false);


      /** Update the given asset's dividend asset options.
       *
       * If the asset is not already a dividend-paying asset, it will be converted into one.
       *
       * @param symbol the name or id of the asset to update, which must be a market-issued asset
       * @param new_options the new dividend_asset_options object, which will entirely replace the existing
       *                    options.
       * @param broadcast true to broadcast the transaction on the network
       * @returns the signed transaction updating the asset
       */
      signed_transaction update_dividend_asset(string symbol,
                                               dividend_asset_options new_options,
                                               bool broadcast = false);

      /** Update the set of feed-producing accounts for a BitAsset.
       *
       * BitAssets have price feeds selected by taking the median values of recommendations from a set of feed producers.
       * This command is used to specify which accounts may produce feeds for a given BitAsset.
       * @param symbol the name or id of the asset to update
       * @param new_feed_producers a list of account names or ids which are authorized to produce feeds for the asset.
       *                           this list will completely replace the existing list
       * @param broadcast true to broadcast the transaction on the network
       * @returns the signed transaction updating the bitasset's feed producers
       */
      signed_transaction update_asset_feed_producers(string symbol,
                                                     flat_set<string> new_feed_producers,
                                                     bool broadcast = false);

      /** Publishes a price feed for the named asset.
       *
       * Price feed providers use this command to publish their price feeds for market-issued assets. A price feed is
       * used to tune the market for a particular market-issued asset. For each value in the feed, the median across all
       * committee_member feeds for that asset is calculated and the market for the asset is configured with the median of that
       * value.
       *
       * The feed object in this command contains three prices: a call price limit, a short price limit, and a settlement price.
       * The call limit price is structured as (collateral asset) / (debt asset) and the short limit price is structured
       * as (asset for sale) / (collateral asset). Note that the asset IDs are opposite to eachother, so if we're
       * publishing a feed for USD, the call limit price will be CORE/USD and the short limit price will be USD/CORE. The
       * settlement price may be flipped either direction, as long as it is a ratio between the market-issued asset and
       * its collateral.
       *
       * @param publishing_account the account publishing the price feed
       * @param symbol the name or id of the asset whose feed we're publishing
       * @param feed the price_feed object containing the three prices making up the feed
       * @param broadcast true to broadcast the transaction on the network
       * @returns the signed transaction updating the price feed for the given asset
       */
      signed_transaction publish_asset_feed(string publishing_account,
                                            string symbol,
                                            price_feed feed,
                                            bool broadcast = false);

      /** Pay into the fee pool for the given asset.
       *
       * User-issued assets can optionally have a pool of the core asset which is
       * automatically used to pay transaction fees for any transaction using that
       * asset (using the asset's core exchange rate).
       *
       * This command allows anyone to deposit the core asset into this fee pool.
       *
       * @param from the name or id of the account sending the core asset
       * @param symbol the name or id of the asset whose fee pool you wish to fund
       * @param amount the amount of the core asset to deposit
       * @param broadcast true to broadcast the transaction on the network
       * @returns the signed transaction funding the fee pool
       */
      signed_transaction fund_asset_fee_pool(string from,
                                             string symbol,
                                             string amount,
                                             bool broadcast = false);

      /** Burns the given user-issued asset.
       *
       * This command burns the user-issued asset to reduce the amount in circulation.
       * @note you cannot burn market-issued assets.
       * @param from the account containing the asset you wish to burn
       * @param amount the amount to burn, in nominal units
       * @param symbol the name or id of the asset to burn
       * @param broadcast true to broadcast the transaction on the network
       * @returns the signed transaction burning the asset
       */
      signed_transaction reserve_asset(string from,
                                    string amount,
                                    string symbol,
                                    bool broadcast = false);

      /** Forces a global settling of the given asset (black swan or prediction markets).
       *
       * In order to use this operation, asset_to_settle must have the global_settle flag set
       *
       * When this operation is executed all balances are converted into the backing asset at the
       * settle_price and all open margin positions are called at the settle price.  If this asset is
       * used as backing for other bitassets, those bitassets will be force settled at their current
       * feed price.
       *
       * @note this operation is used only by the asset issuer, \c settle_asset() may be used by
       *       any user owning the asset
       *
       * @param symbol the name or id of the asset to force settlement on
       * @param settle_price the price at which to settle
       * @param broadcast true to broadcast the transaction on the network
       * @returns the signed transaction settling the named asset
       */
      signed_transaction global_settle_asset(string symbol,
                                             price settle_price,
                                             bool broadcast = false);

      /** Schedules a market-issued asset for automatic settlement.
       *
       * Holders of market-issued assests may request a forced settlement for some amount of their asset. This means that
       * the specified sum will be locked by the chain and held for the settlement period, after which time the chain will
       * choose a margin posision holder and buy the settled asset using the margin's collateral. The price of this sale
       * will be based on the feed price for the market-issued asset being settled. The exact settlement price will be the
       * feed price at the time of settlement with an offset in favor of the margin position, where the offset is a
       * blockchain parameter set in the global_property_object.
       *
       * @param account_to_settle the name or id of the account owning the asset
       * @param amount_to_settle the amount of the named asset to schedule for settlement
       * @param symbol the name or id of the asset to settlement on
       * @param broadcast true to broadcast the transaction on the network
       * @returns the signed transaction settling the named asset
       */
      signed_transaction settle_asset(string account_to_settle,
                                      string amount_to_settle,
                                      string symbol,
                                      bool broadcast = false);

      /** Whitelist and blacklist accounts, primarily for transacting in whitelisted assets.
       *
       * Accounts can freely specify opinions about other accounts, in the form of either whitelisting or blacklisting
       * them. This information is used in chain validation only to determine whether an account is authorized to transact
       * in an asset type which enforces a whitelist, but third parties can use this information for other uses as well,
       * as long as it does not conflict with the use of whitelisted assets.
       *
       * An asset which enforces a whitelist specifies a list of accounts to maintain its whitelist, and a list of
       * accounts to maintain its blacklist. In order for a given account A to hold and transact in a whitelisted asset S,
       * A must be whitelisted by at least one of S's whitelist_authorities and blacklisted by none of S's
       * blacklist_authorities. If A receives a balance of S, and is later removed from the whitelist(s) which allowed it
       * to hold S, or added to any blacklist S specifies as authoritative, A's balance of S will be frozen until A's
       * authorization is reinstated.
       *
       * @param authorizing_account the account who is doing the whitelisting
       * @param account_to_list the account being whitelisted
       * @param new_listing_status the new whitelisting status
       * @param broadcast true to broadcast the transaction on the network
       * @returns the signed transaction changing the whitelisting status
       */
      signed_transaction whitelist_account(string authorizing_account,
                                           string account_to_list,
                                           account_whitelist_operation::account_listing new_listing_status,
                                           bool broadcast = false);

      /** Creates a committee_member object owned by the given account.
       *
       * An account can have at most one committee_member object.
       *
       * @param owner_account the name or id of the account which is creating the committee_member
       * @param url a URL to include in the committee_member record in the blockchain.  Clients may
       *            display this when showing a list of committee_members.  May be blank.
       * @param broadcast true to broadcast the transaction on the network
       * @returns the signed transaction registering a committee_member
       */
      signed_transaction create_committee_member(string owner_account,
                                         string url,
                                         bool broadcast = false);

      /** Lists all witnesses registered in the blockchain.
       * This returns a list of all account names that own witnesses, and the associated witness id,
       * sorted by name.  This lists witnesses whether they are currently voted in or not.
       *
       * Use the \c lowerbound and limit parameters to page through the list.  To retrieve all witnesss,
       * start by setting \c lowerbound to the empty string \c "", and then each iteration, pass
       * the last witness name returned as the \c lowerbound for the next \c list_witnesss() call.
       *
       * @param lowerbound the name of the first witness to return.  If the named witness does not exist,
       *                   the list will start at the witness that comes after \c lowerbound
       * @param limit the maximum number of witnesss to return (max: 1000)
       * @returns a list of witnesss mapping witness names to witness ids
       */
      map<string,witness_id_type>       list_witnesses(const string& lowerbound, uint32_t limit);

      /** Lists all committee_members registered in the blockchain.
       * This returns a list of all account names that own committee_members, and the associated committee_member id,
       * sorted by name.  This lists committee_members whether they are currently voted in or not.
       *
       * Use the \c lowerbound and limit parameters to page through the list.  To retrieve all committee_members,
       * start by setting \c lowerbound to the empty string \c "", and then each iteration, pass
       * the last committee_member name returned as the \c lowerbound for the next \c list_committee_members() call.
       *
       * @param lowerbound the name of the first committee_member to return.  If the named committee_member does not exist,
       *                   the list will start at the committee_member that comes after \c lowerbound
       * @param limit the maximum number of committee_members to return (max: 1000)
       * @returns a list of committee_members mapping committee_member names to committee_member ids
       */
      map<string, committee_member_id_type>       list_committee_members(const string& lowerbound, uint32_t limit);

      /** Lists all workers in the blockchain.
       * This returns a list of all account names that own worker, and the associated worker id,
       * sorted by name.  This lists workers whether they are currently voted in or not.
       *
       * Use the \c lowerbound and limit parameters to page through the list.  To retrieve all workers,
       * start by setting \c lowerbound to the empty string \c "", and then each iteration, pass
       * the last worker name returned as the \c lowerbound for the next \c list_workers() call.
       *
       * @param lowerbound the name of the first worker to return.  If the named worker does not exist,
       *                   the list will start at the worker that comes after \c lowerbound
       * @param limit the maximum number of worker to return (max: 1000)
       * @returns a list of worker mapping worker names to worker ids
       */
      map<string, worker_id_type> list_workers(const string& lowerbound, uint32_t limit);

      /** Returns information about the given SON.
       * @param owner_account the name or id of the SON account owner, or the id of the SON
       * @returns the information about the SON stored in the block chain
       */
      son_object get_son(string owner_account);

      /** Returns information about the given witness.
       * @param owner_account the name or id of the witness account owner, or the id of the witness
       * @returns the information about the witness stored in the block chain
       */
      witness_object get_witness(string owner_account);

      /** Returns true if the account is witness, false otherwise
       * @param owner_account the name or id of the witness account owner, or the id of the witness
       * @returns true if account is witness, false otherwise
       */
      bool is_witness(string owner_account);

      /** Returns information about the given committee_member.
       * @param owner_account the name or id of the committee_member account owner, or the id of the committee_member
       * @returns the information about the committee_member stored in the block chain
       */
      committee_member_object get_committee_member(string owner_account);

      /** Returns information about the given worker.
       * @param owner_account the name or id of the worker account owner, or the id of the worker
       * @returns the information about the workers stored in the block chain
       */
      vector<worker_object> get_workers(string owner_account);


      /** Creates a SON object owned by the given account.
       *
       * An account can have at most one SON object.
       *
       * @param owner_account the name or id of the account which is creating the SON
       * @param url a URL to include in the SON record in the blockchain.  Clients may
       *            display this when showing a list of SONs.  May be blank.
       * @param deposit_id vesting balance id for SON deposit
       * @param pay_vb_id vesting balance id for SON pay_vb
       * @param sidechain_public_keys The new set of sidechain public keys.
       * @param broadcast true to broadcast the transaction on the network
       * @returns the signed transaction registering a SON
       */
      signed_transaction create_son(string owner_account,
                                    string url,
                                    vesting_balance_id_type deposit_id,
                                    vesting_balance_id_type pay_vb_id,
                                    flat_map<sidechain_type, string> sidechain_public_keys,
                                    bool broadcast = false);

      /** Creates a SON object owned by the given account.
       *
       * Tries to create a SON object owned by the given account using
       * existing vesting balances, fails if can't quess matching
       * vesting balance objects. If several vesting balance objects matches
       * this function uses the recent one.
       *
       * @param owner_account the name or id of the account which is creating the SON
       * @param url a URL to include in the SON record in the blockchain.  Clients may
       *            display this when showing a list of SONs.  May be blank.
       * @param sidechain_public_keys The new set of sidechain public keys.
       * @param broadcast true to broadcast the transaction on the network
       * @returns the signed transaction registering a SON
       */
      signed_transaction try_create_son(string owner_account,
                                    string url,
                                    flat_map<sidechain_type, string> sidechain_public_keys,
                                    bool broadcast = false);

      /**
       * Update a SON object owned by the given account.
       *
       * @param owner_account The name of the SON's owner account.  Also accepts the ID of the owner account or the ID of the SON.
       * @param url Same as for create_son.  The empty string makes it remain the same.
       * @param block_signing_key The new block signing public key.  The empty string makes it remain the same.
       * @param sidechain_public_keys The new set of sidechain public keys.  The empty string makes it remain the same.
       * @param broadcast true if you wish to broadcast the transaction.
       */
      signed_transaction update_son(string owner_account,
                                    string url,
                                    string block_signing_key,
                                    flat_map<sidechain_type, string> sidechain_public_keys,
                                    bool broadcast = false);

      /**
       * Activate deregistered SON object owned by the given account.
       *
       * @param owner_account The name of the SON's owner account.  Also accepts the ID of the owner account or the ID of the SON.
       * @param broadcast true if you wish to broadcast the transaction.
       */
      signed_transaction activate_deregistered_son(const string & owner_account,
                                                   bool broadcast /* = false */);


      /**
       * Updates vesting balances of the SON object owned by the given account.
       *
       * @param owner_account The name of the SON's owner account.  Also accepts the ID of the owner account or the ID of the SON.
       * @param new_deposit New deposit vesting balance id.  The empty string makes it remain the same.
       * @param new_pay_vb New payment vesting balance id.  The empty string makes it remain the same.
       * @param broadcast true if you wish to broadcast the transaction.
       */
      signed_transaction update_son_vesting_balances(string owner_account,
                                                     optional<vesting_balance_id_type> new_deposit,
                                                     optional<vesting_balance_id_type> new_pay_vb,
                                                     bool broadcast = false);

      /** Modify status of the SON owned by the given account to maintenance.
       *
       * @param owner_account the name or id of the account which is owning the SON
       * @param broadcast true to broadcast the transaction on the network
       * @returns the signed transaction
       */
      signed_transaction request_son_maintenance(string owner_account,
                                              bool broadcast = false);

      /** Modify status of the SON owned by the given account back to active.
       *
       * @param owner_account the name or id of the account which is owning the SON
       * @param broadcast true to broadcast the transaction on the network
       * @returns the signed transaction
       */
      signed_transaction cancel_request_son_maintenance(string owner_account,
                                              bool broadcast = false);

      /** Lists all SONs in the blockchain.
       * This returns a list of all account names that own SON, and the associated SON id,
       * sorted by name.  This lists SONs whether they are currently voted in or not.
       *
       * Use the \c lowerbound and limit parameters to page through the list.  To retrieve all SONs,
       * start by setting \c lowerbound to the empty string \c "", and then each iteration, pass
       * the last SON name returned as the \c lowerbound for the next \c list_sons() call.
       *
       * @param lowerbound the name of the first SON to return.  If the named SON does not exist,
       *                   the list will start at the SON that comes after \c lowerbound
       * @param limit the maximum number of SON to return (max: 1000)
       * @returns a list of SON mapping SON names to SON ids
       */
      map<string, son_id_type> list_sons(const string& lowerbound, uint32_t limit);

      /** Lists active at the moment SONs.
       * This returns a list of all account names that own active SON, and the associated SON id,
       * sorted by name.
       * @returns a list of active SONs mapping SON names to SON ids
       */
      map<string, son_id_type> list_active_sons();

      /**
       * @brief Get SON network status
       * @return SON network status description
       */
      map<son_id_type, string>  get_son_network_status();

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

      /** Adds sidechain address owned by the given account for a given sidechain.
       *
       * An account can have at most one sidechain address for one sidechain.
       *
       * @param account the name or id of the account who owns the address
       * @param sidechain a sidechain to whom address belongs
       * @param deposit_public_key sidechain public key used for deposit address
       * @param deposit_address sidechain address for deposits
       * @param withdraw_public_key sidechain public key used for withdraw address
       * @param withdraw_address sidechain address for withdrawals
       * @param broadcast true to broadcast the transaction on the network
       * @returns the signed transaction adding sidechain address
       */
      signed_transaction add_sidechain_address(string account,
                                          sidechain_type sidechain,
                                          string deposit_public_key,
                                          string deposit_address,
                                          string withdraw_public_key,
                                          string withdraw_address,
                                          bool broadcast = false);

      /** Deletes existing sidechain address owned by the given account for a given sidechain.
       *
       * @param account the name or id of the account who owns the address
       * @param sidechain a sidechain to whom address belongs
       * @param broadcast true to broadcast the transaction on the network
       * @returns the signed transaction updating sidechain address
       */
      signed_transaction delete_sidechain_address(string account,
                                          sidechain_type sidechain,
                                          bool broadcast = false);

      /** Retrieves all sidechain addresses owned by given account.
       *
       * @param account the name or id of the account who owns the address
       * @returns the list of all sidechain addresses owned by given account.
       */
      vector<optional<sidechain_address_object>> get_sidechain_addresses_by_account(string account);

      /** Retrieves all sidechain addresses registered for a given sidechain.
       *
       * @param sidechain the name of the sidechain
       * @returns the list of all sidechain addresses registered for a given sidechain.
       */
      vector<optional<sidechain_address_object>> get_sidechain_addresses_by_sidechain(sidechain_type sidechain);

      /** Retrieves sidechain address owned by given account for a given sidechain.
       *
       * @param account the name or id of the account who owns the address
       * @param sidechain the name of the sidechain
       * @returns the sidechain address owned by given account for a given sidechain.
       */
      fc::optional<sidechain_address_object> get_sidechain_address_by_account_and_sidechain(string account, sidechain_type sidechain);

      /** Retrieves the total number of sidechain addresses registered in the system.
       *
       * @returns the total number of sidechain addresses registered in the system.
       */
      uint64_t get_sidechain_addresses_count();

      /** Creates a witness object owned by the given account.
       *
       * An account can have at most one witness object.
       *
       * @param owner_account the name or id of the account which is creating the witness
       * @param url a URL to include in the witness record in the blockchain.  Clients may
       *            display this when showing a list of witnesses.  May be blank.
       * @param broadcast true to broadcast the transaction on the network
       * @returns the signed transaction registering a witness
       */
      signed_transaction create_witness(string owner_account,
                                        string url,
                                        bool broadcast = false);

      /**
       * Update a witness object owned by the given account.
       *
       * @param witness_name The name of the witness's owner account.  Also accepts the ID of the owner account or the ID of the witness.
       * @param url Same as for create_witness.  The empty string makes it remain the same.
       * @param block_signing_key The new block signing public key.  The empty string makes it remain the same.
       * @param broadcast true if you wish to broadcast the transaction.
       */
      signed_transaction update_witness(string witness_name,
                                        string url,
                                        string block_signing_key,
                                        bool broadcast = false);


      /**
       * Create a worker object.
       *
       * @param owner_account The account which owns the worker and will be paid
       * @param work_begin_date When the work begins
       * @param work_end_date When the work ends
       * @param daily_pay Amount of pay per day (NOT per maint interval)
       * @param name Any text
       * @param url Any text
       * @param worker_settings {"type" : "burn"|"refund"|"vesting", "pay_vesting_period_days" : x}
       * @param broadcast true if you wish to broadcast the transaction.
       */
      signed_transaction create_worker(
         string owner_account,
         time_point_sec work_begin_date,
         time_point_sec work_end_date,
         share_type daily_pay,
         string name,
         string url,
         variant worker_settings,
         bool broadcast = false
         );

      /**
       * Update your votes for a worker
       *
       * @param account The account which will pay the fee and update votes.
       * @param delta {"vote_for" : [...], "vote_against" : [...], "vote_abstain" : [...]}
       * @param broadcast true if you wish to broadcast the transaction.
       */
      signed_transaction update_worker_votes(
         string account,
         worker_vote_delta delta,
         bool broadcast = false
         );

      /** Creates a vesting deposit owned by the given account.
       *
       * @param owner_account vesting balance owner and creator (the name or id)
       * @param amount amount to vest
       * @param asset_symbol the symbol of the asset to vest
       * @param vesting_type "normal", "gpos" or "son"
       * @param broadcast true to broadcast the transaction on the network
       * @returns the signed transaction registering a vesting object
       */
      signed_transaction create_vesting_balance(string owner_account,
                                        string amount,
                                        string asset_symbol,
                                        vesting_balance_type vesting_type,
                                        bool broadcast = false);

      /**
       * Get information about a vesting balance object.
       *
       * @param account_name An account name, account ID, or vesting balance object ID.
       */
      vector< vesting_balance_object_with_info > get_vesting_balances( string account_name );

      /**
       * Withdraw a normal(old) vesting balance.
       *
       * @param witness_name The account name of the witness, also accepts account ID or vesting balance ID type.
       * @param amount The amount to withdraw.
       * @param asset_symbol The symbol of the asset to withdraw.
       * @param broadcast true if you wish to broadcast the transaction
       */
      signed_transaction withdraw_vesting(
         string witness_name,
         string amount,
         string asset_symbol,
         bool broadcast = false);

      /**
       * Withdraw a GPOS vesting balance.
       *
       * @param account_name The account name of the witness/user, also accepts account ID or vesting balance ID type.
       * @param amount The amount to withdraw.
       * @param asset_symbol The symbol of the asset to withdraw.
       * @param broadcast true if you wish to broadcast the transaction
       */
      signed_transaction withdraw_GPOS_vesting_balance(
         string account_name,
         string amount,
         string asset_symbol,
         bool broadcast = false);

      /** Vote for a given committee_member.
       *
       * An account can publish a list of all committee_memberes they approve of.  This
       * command allows you to add or remove committee_memberes from this list.
       * Each account's vote is weighted according to the number of shares of the
       * core asset owned by that account at the time the votes are tallied.
       *
       * @note you cannot vote against a committee_member, you can only vote for the committee_member
       *       or not vote for the committee_member.
       *
       * @param voting_account the name or id of the account who is voting with their shares
       * @param committee_member the name or id of the committee_member' owner account
       * @param approve true if you wish to vote in favor of that committee_member, false to
       *                remove your vote in favor of that committee_member
       * @param broadcast true if you wish to broadcast the transaction
       * @return the signed transaction changing your vote for the given committee_member
       */
      signed_transaction vote_for_committee_member(string voting_account,
                                           string committee_member,
                                           bool approve,
                                           bool broadcast = false);

      /** Vote for a given SON.
       *
       * An account can publish a list of all SONs they approve of.  This
       * command allows you to add or remove SONs from this list.
       * Each account's vote is weighted according to the number of shares of the
       * core asset owned by that account at the time the votes are tallied.
       *
       * @note you cannot vote against a SON, you can only vote for the SON
       *       or not vote for the SON.
       *
       * @param voting_account the name or id of the account who is voting with their shares
       * @param son the name or id of the SONs' owner account
       * @param approve true if you wish to vote in favor of that SON, false to
       *                remove your vote in favor of that SON
       * @param broadcast true if you wish to broadcast the transaction
       * @return the signed transaction changing your vote for the given SON
       */
      signed_transaction vote_for_son(string voting_account,
                                             string son,
                                             bool approve,
                                             bool broadcast = false);

      /** Change your SON votes.
       *
       * An account can publish a list of all SONs they approve of.
       * Each account's vote is weighted according to the number of shares of the
       * core asset owned by that account at the time the votes are tallied.
       * This command allows you to add or remove one or more SON from this list
       * in one call.  When you are changing your vote on several SONs, this
       * may be easier than multiple `vote_for_sons` and
       * `set_desired_witness_and_committee_member_count` calls.
       *
       * @note you cannot vote against a SON, you can only vote for the SON
       *       or not vote for the SON.
       *
       * @param voting_account the name or id of the account who is voting with their shares
       * @param sons_to_approve the names or ids of the sons owner accounts you wish
       *                             to approve (these will be added to the list of sons
       *                             you currently approve).  This list can be empty.
       * @param sons_to_reject the names or ids of the SONs owner accounts you wish
       *                            to reject (these will be removed from the list of SONs
       *                            you currently approve).  This list can be empty.
       * @param desired_number_of_sons the number of SONs you believe the network
       *                                    should have.  You must vote for at least this many
       *                                    SONs.  You can set this to 0 to abstain from
       *                                    voting on the number of SONNs.
       * @param broadcast true if you wish to broadcast the transaction
       * @return the signed transaction changing your vote for the given witnesses
       */
      signed_transaction update_son_votes(string voting_account,
                                              std::vector<std::string> sons_to_approve,
                                              std::vector<std::string> sons_to_reject,
                                              uint16_t desired_number_of_sons,
                                              bool broadcast = false);


      /** Broadcast signed transaction for manually sidechain deposit
       * @param son_name_or_id ID or name of the son account
       * @param sidechain Sidechain type (bitcoin, HIVE, etc)
       * @param transaction_id ID of transaction
       * @param operation_index Index of operation
       * @param sidechain_from Sidechain address transaction from
       * @param sidechain_to Sidechain address transaction to
       * @param sidechain_currency Sidechain currency
       * @param sidechain_amount Sidechain amount to deposit
       * @param peerplays_from_name_or_id ID or name of the account transaction from
       * @param peerplays_to_name_or_id ID or name of the account transaction to
       * @returns the signed transaction.
       */
      signed_transaction sidechain_deposit_transaction(  const string &son_name_or_id,
                                                         const sidechain_type& sidechain,
                                                         const string &transaction_id,
                                                         uint32_t operation_index,
                                                         const string &sidechain_from,
                                                         const string &sidechain_to,
                                                         const string &sidechain_currency,
                                                         int64_t sidechain_amount,
                                                         const string &peerplays_from_name_or_id,
                                                         const string &peerplays_to_name_or_id);

      /** Broadcast signed transaction for manually sidechain withdrawal
       * @param son_name_or_id ID or name of the son account
       * @param block_num Block number where original withdrawal transaction is executed
       * @param sidechain Sidechain type (bitcoin, HIVE, etc)
       * @param peerplays_uid peerplays_uid
       * @param peerplays_transaction_id ID of transaction
       * @param peerplays_from Sidechain address transaction from
       * @param withdraw_sidechain Withdraw sidechain
       * @param withdraw_address Withdraw address
       * @param withdraw_currency Withdraw currency
       * @param withdraw_amount Withdraw amount
       * @returns the signed transaction.
       */
      signed_transaction sidechain_withdrawal_transaction(const string &son_name_or_id,
		                                      uint32_t block_num,
                                                      const sidechain_type& sidechain,
                                                      const std::string &peerplays_uid,
                                                      const std::string &peerplays_transaction_id,
                                                      const chain::account_id_type &peerplays_from,
                                                      const sidechain_type& withdraw_sidechain,
                                                      const std::string &withdraw_address,
                                                      const std::string &withdraw_currency,
                                                      const string &withdraw_amount);

      /** Vote for a given witness.
       *
       * An account can publish a list of all witnesses they approve of.  This
       * command allows you to add or remove witnesses from this list.
       * Each account's vote is weighted according to the number of shares of the
       * core asset owned by that account at the time the votes are tallied.
       *
       * @note you cannot vote against a witness, you can only vote for the witness
       *       or not vote for the witness.
       *
       * @param voting_account the name or id of the account who is voting with their shares
       * @param witness the name or id of the witness' owner account
       * @param approve true if you wish to vote in favor of that witness, false to
       *                remove your vote in favor of that witness
       * @param broadcast true if you wish to broadcast the transaction
       * @return the signed transaction changing your vote for the given witness
       */
      signed_transaction vote_for_witness(string voting_account,
                                          string witness,
                                          bool approve,
                                          bool broadcast = false);

      /** Change your witness votes.
       *
       * An account can publish a list of all witnesses they approve of.
       * Each account's vote is weighted according to the number of shares of the
       * core asset owned by that account at the time the votes are tallied.
       * This command allows you to add or remove one or more witnesses from this list
       * in one call.  When you are changing your vote on several witnesses, this
       * may be easier than multiple `vote_for_witness` and
       * `set_desired_witness_and_committee_member_count` calls.
       *
       * @note you cannot vote against a witness, you can only vote for the witness
       *       or not vote for the witness.
       *
       * @param voting_account the name or id of the account who is voting with their shares
       * @param witnesses_to_approve the names or ids of the witnesses owner accounts you wish
       *                             to approve (these will be added to the list of witnesses
       *                             you currently approve).  This list can be empty.
       * @param witnesses_to_reject the names or ids of the witnesses owner accounts you wish
       *                            to reject (these will be removed fromthe list of witnesses
       *                            you currently approve).  This list can be empty.
       * @param desired_number_of_witnesses the number of witnesses you believe the network
       *                                    should have.  You must vote for at least this many
       *                                    witnesses.  You can set this to 0 to abstain from
       *                                    voting on the number of witnesses.
       * @param broadcast true if you wish to broadcast the transaction
       * @return the signed transaction changing your vote for the given witnesses
       */
      signed_transaction update_witness_votes(string voting_account,
                                              std::vector<std::string> witnesses_to_approve,
                                              std::vector<std::string> witnesses_to_reject,
                                              uint16_t desired_number_of_witnesses,
                                              bool broadcast = false);
      /** Set the voting proxy for an account.
       *
       * If a user does not wish to take an active part in voting, they can choose
       * to allow another account to vote their stake.
       *
       * Setting a vote proxy does not remove your previous votes from the blockchain,
       * they remain there but are ignored.  If you later null out your vote proxy,
       * your previous votes will take effect again.
       *
       * This setting can be changed at any time.
       *
       * @param account_to_modify the name or id of the account to update
       * @param voting_account the name or id of an account authorized to vote account_to_modify's shares,
       *                       or null to vote your own shares
       *
       * @param broadcast true if you wish to broadcast the transaction
       * @return the signed transaction changing your vote proxy settings
       */
      signed_transaction set_voting_proxy(string account_to_modify,
                                          optional<string> voting_account,
                                          bool broadcast = false);

      /** Set your vote for the number of witnesses and committee_members in the system.
       *
       * Each account can voice their opinion on how many committee_members and how many
       * witnesses there should be in the active committee_member/active witness list.  These
       * are independent of each other.  You must vote your approval of at least as many
       * committee_members or witnesses as you claim there should be (you can't say that there should
       * be 20 committee_members but only vote for 10).
       *
       * There are maximum values for each set in the blockchain parameters (currently
       * defaulting to 1001).
       *
       * This setting can be changed at any time.  If your account has a voting proxy
       * set, your preferences will be ignored.
       *
       * @param account_to_modify the name or id of the account to update
       * @param desired_number_of_witnesses desired number of witnesses
       * @param desired_number_of_committee_members desired number of committee members
       * @param broadcast true if you wish to broadcast the transaction
       * @return the signed transaction changing your vote proxy settings
       */
      signed_transaction set_desired_witness_and_committee_member_count(string account_to_modify,
                                                                uint16_t desired_number_of_witnesses,
                                                                uint16_t desired_number_of_committee_members,
                                                                bool broadcast = false);

      /** Signs a transaction.
       *
       * Given a fully-formed transaction that is only lacking signatures, this signs
       * the transaction with the necessary keys and optionally broadcasts the transaction
       * @param tx the unsigned transaction
       * @param broadcast true if you wish to broadcast the transaction
       * @return the signed version of the transaction
       */
      signed_transaction sign_transaction(signed_transaction tx, bool broadcast = false);

      /** Get transaction signers.
       *
       * Returns information about who signed the transaction, specifically,
       * the corresponding public keys of the private keys used to sign the transaction.
       * @param tx the signed transaction
       * @return the set of public_keys
       */
      flat_set<public_key_type> get_transaction_signers(const signed_transaction &tx) const;

      /** Get key references.
       *
       * Returns accounts related to given public keys.
       * @param keys public keys to search for related accounts
       * @return the set of related accounts
       */
      vector<vector<account_id_type>> get_key_references(const vector<public_key_type> &keys) const;

      /** Signs a transaction.
       *
       * Given a fully-formed transaction with or without signatures, signs
       * the transaction with the owned keys and optionally broadcasts the
       * transaction.
       *
       * @param tx the unsigned transaction
       * @param broadcast true if you wish to broadcast the transaction
       *
       * @return the signed transaction
       */
      signed_transaction add_transaction_signature( signed_transaction tx,
                                                    bool broadcast = false );

      /** Returns an uninitialized object representing a given blockchain operation.
       *
       * This returns a default-initialized object of the given type; it can be used
       * during early development of the wallet when we don't yet have custom commands for
       * creating all of the operations the blockchain supports.
       *
       * Any operation the blockchain supports can be created using the transaction builder's
       * \c add_operation_to_builder_transaction() , but to do that from the CLI you need to
       * know what the JSON form of the operation looks like.  This will give you a template
       * you can fill in.  It's better than nothing.
       *
       * @param operation_type the type of operation to return, must be one of the
       *                       operations defined in `graphene/chain/operations.hpp`
       *                       (e.g., "global_parameters_update_operation")
       * @return a default-constructed operation of the given type
       */
      operation get_prototype_operation(string operation_type);

      /** Creates a transaction to propose a parameter change.
       *
       * Multiple parameters can be specified if an atomic change is
       * desired.
       *
       * @param proposing_account The account paying the fee to propose the tx
       * @param expiration_time Timestamp specifying when the proposal will either take effect or expire.
       * @param changed_values The values to change; all other chain parameters are filled in with default values
       * @param broadcast true if you wish to broadcast the transaction
       * @return the signed version of the transaction
       */
      signed_transaction propose_parameter_change(
         const string& proposing_account,
         fc::time_point_sec expiration_time,
         const variant_object& changed_values,
         bool broadcast = false);

      /** Propose a fee change.
       *
       * @param proposing_account The account paying the fee to propose the tx
       * @param expiration_time Timestamp specifying when the proposal will either take effect or expire.
       * @param changed_values Map of operation type to new fee.  Operations may be specified by name or ID.
       *    The "scale" key changes the scale.  All other operations will maintain current values.
       * @param broadcast true if you wish to broadcast the transaction
       * @return the signed version of the transaction
       */
      signed_transaction propose_fee_change(
         const string& proposing_account,
         fc::time_point_sec expiration_time,
         const variant_object& changed_values,
         bool broadcast = false);

      /** Propose a dividend asset update.
       *
       * @param proposing_account The account paying the fee to propose the tx
       * @param expiration_time Timestamp specifying when the proposal will either take effect or expire.
       * @param changed_values dividend asset parameters to update
       * @param broadcast true if you wish to broadcast the transaction
       * @return the signed version of the transaction
       */
      signed_transaction propose_dividend_asset_update(
         const string& proposing_account,
         fc::time_point_sec expiration_time,
         const variant_object& changed_values,
         bool broadcast = false);

      /** Approve or disapprove a proposal.
       *
       * @param fee_paying_account The account paying the fee for the op.
       * @param proposal_id The proposal to modify.
       * @param delta Members contain approvals to create or remove.  In JSON you can leave empty members undefined.
       * @param broadcast true if you wish to broadcast the transaction
       * @return the signed version of the transaction
       */
      signed_transaction approve_proposal(
         const string& fee_paying_account,
         const string& proposal_id,
         const approval_delta& delta,
         bool broadcast /* = false */
         );

      order_book get_order_book( const string& base, const string& quote, unsigned limit = 50);

      asset get_total_matched_bet_amount_for_betting_market_group(betting_market_group_id_type group_id);
      std::vector<event_object> get_events_containing_sub_string(const std::string& sub_string, const std::string& language);

      /** Get an order book for a betting market, with orders aggregated into bins with similar
       * odds
       *
       * @param betting_market_id the betting market
       * @param precision the number of digits of precision for binning
       */
      binned_order_book get_binned_order_book(graphene::chain::betting_market_id_type betting_market_id, int32_t precision);

      std::vector<matched_bet_object> get_matched_bets_for_bettor(account_id_type bettor_id) const;

      std::vector<matched_bet_object> get_all_matched_bets_for_bettor(account_id_type bettor_id, bet_id_type start = bet_id_type(), unsigned limit = 1000) const;

      vector<sport_object> list_sports() const;
      vector<event_group_object> list_event_groups(sport_id_type sport_id) const;
      vector<betting_market_group_object> list_betting_market_groups(event_id_type event_id) const;
      vector<betting_market_object> list_betting_markets(betting_market_group_id_type betting_market_group_id) const;
      global_betting_statistics_object get_global_betting_statistics() const;
      vector<event_object> list_events_in_group(event_group_id_type event_group_id) const;
      vector<bet_object> get_unmatched_bets_for_bettor(betting_market_id_type betting_market_id, account_id_type account_id) const;
      vector<bet_object> get_all_unmatched_bets_for_bettor(account_id_type account_id) const;

      signed_transaction propose_create_sport(
              const string& proposing_account,
              fc::time_point_sec expiration_time,
              internationalized_string_type name,
              bool broadcast = false);

      signed_transaction propose_update_sport(
              const string& proposing_account,
              fc::time_point_sec expiration_time,
              sport_id_type sport_id,
              fc::optional<internationalized_string_type> name,
              bool broadcast = false);

      signed_transaction propose_delete_sport(
              const string& proposing_account,
              fc::time_point_sec expiration_time,
              sport_id_type sport_id,
              bool broadcast = false);

      signed_transaction propose_create_event_group(
              const string& proposing_account,
              fc::time_point_sec expiration_time,
              internationalized_string_type name,
              sport_id_type sport_id,
              bool broadcast = false);

      signed_transaction propose_update_event_group(
              const string& proposing_account,
              fc::time_point_sec expiration_time,
              event_group_id_type event_group,
              fc::optional<object_id_type> sport_id,
              fc::optional<internationalized_string_type> name,
              bool broadcast = false);

      signed_transaction propose_delete_event_group(
              const string& proposing_account,
              fc::time_point_sec expiration_time,
              event_group_id_type event_group,
              bool broadcast = false);

      signed_transaction propose_create_event(
              const string& proposing_account,
              fc::time_point_sec expiration_time,
              internationalized_string_type name,
              internationalized_string_type season,
              fc::optional<time_point_sec> start_time,
              event_group_id_type event_group_id,
              bool broadcast = false);

      signed_transaction propose_update_event(
              const string& proposing_account,
              fc::time_point_sec expiration_time,
              event_id_type event_id,
              fc::optional<object_id_type> event_group_id,
              fc::optional<internationalized_string_type> name,
              fc::optional<internationalized_string_type> season,
              fc::optional<event_status> status,
              fc::optional<time_point_sec> start_time,
              bool broadcast = false);

      signed_transaction propose_create_betting_market_rules(
              const string& proposing_account,
              fc::time_point_sec expiration_time,
              internationalized_string_type name,
              internationalized_string_type description,
              bool broadcast = false);

      signed_transaction propose_update_betting_market_rules(
              const string& proposing_account,
              fc::time_point_sec expiration_time,
              betting_market_rules_id_type rules_id,
              fc::optional<internationalized_string_type> name,
              fc::optional<internationalized_string_type> description,
              bool broadcast = false);

      signed_transaction propose_create_betting_market_group(
              const string& proposing_account,
              fc::time_point_sec expiration_time,
              internationalized_string_type description,
              event_id_type event_id,
              betting_market_rules_id_type rules_id,
              asset_id_type asset_id,
              bool broadcast = false);

      signed_transaction propose_update_betting_market_group(
              const string& proposing_account,
              fc::time_point_sec expiration_time,
              betting_market_group_id_type betting_market_group_id,
              fc::optional<internationalized_string_type> description,
              fc::optional<object_id_type> rules_id,
              fc::optional<betting_market_group_status> status,
              bool broadcast = false);

      signed_transaction propose_create_betting_market(
              const string& proposing_account,
              fc::time_point_sec expiration_time,
              betting_market_group_id_type group_id,
              internationalized_string_type description,
              internationalized_string_type payout_condition,
              bool broadcast = false);

      signed_transaction propose_update_betting_market(
              const string& proposing_account,
              fc::time_point_sec expiration_time,
              betting_market_id_type market_id,
              fc::optional<object_id_type> group_id,
              fc::optional<internationalized_string_type> description,
              fc::optional<internationalized_string_type> payout_condition,
              bool broadcast = false);

      /** Place a bet
       * @param bettor the account placing the bet
       * @param betting_market_id the market on which to bet
       * @param back_or_lay back or lay
       * @param amount the amount to bet
       * @param asset_symbol the asset to bet with (must be the same as required by the betting market group)
       * @param backer_multiplier the odds (use 2.0 for a 1:1 bet)
       * @param broadcast true to broadcast the transaction
       */
      signed_transaction place_bet(string bettor,
                                   betting_market_id_type betting_market_id,
                                   bet_type back_or_lay,
                                   string amount,
                                   string asset_symbol,
                                   double backer_multiplier,
                                   bool broadcast = false);

      signed_transaction cancel_bet(string betting_account,
                                                bet_id_type bet_id,
                                                bool broadcast = false);

      signed_transaction propose_resolve_betting_market_group(
              const string& proposing_account,
              fc::time_point_sec expiration_time,
              betting_market_group_id_type betting_market_group_id,
              const std::map<betting_market_id_type, betting_market_resolution_type>& resolutions,
              bool broadcast = false);

      signed_transaction propose_cancel_betting_market_group(
              const string& proposing_account,
              fc::time_point_sec expiration_time,
              betting_market_group_id_type betting_market_group_id,
              bool broadcast = false);

      /** Creates a new tournament
       * @param creator the accout that is paying the fee to create the tournament
       * @param options the options detailing the specifics of the tournament
       * @param broadcast true if you wish to broadcast the transaction
       * @return the signed version of the transaction
       */
      signed_transaction tournament_create( string creator, tournament_options options, bool broadcast = false );

      /** Join an existing tournament
       * @param payer_account the account that is paying the buy-in and the fee to join the tournament
       * @param player_account the account that will be playing in the tournament
       * @param buy_in_amount buy_in to pay
       * @param buy_in_asset_symbol buy_in asset
       * @param tournament_id the tournament the user wishes to join
       * @param broadcast true if you wish to broadcast the transaction
       * @return the signed version of the transaction
       */
      signed_transaction tournament_join( string payer_account, string player_account, tournament_id_type tournament_id, string buy_in_amount, string buy_in_asset_symbol, bool broadcast = false );

      /** Leave an existing tournament
       * @param payer_account the account that is paying the fee
       * @param player_account the account that would be playing in the tournament
       * @param tournament_id the tournament the user wishes to leave
       * @param broadcast true if you wish to broadcast the transaction
       * @return the signed version of the transaction
       */
      signed_transaction tournament_leave(string payer_account, string player_account, tournament_id_type tournament_id, bool broadcast = false);

      /** Get a list of upcoming tournaments
       * @param limit the number of tournaments to return
       */
      vector<tournament_object> get_upcoming_tournaments(uint32_t limit);

      vector<tournament_object> get_tournaments(tournament_id_type stop,
                                                unsigned limit,
                                                tournament_id_type start);

      vector<tournament_object> get_tournaments_by_state(tournament_id_type stop,
                                                         unsigned limit,
                                                         tournament_id_type start,
                                                         tournament_state state);

      /** Get specific information about a tournament
       * @param id the ID of the tournament
       */
      tournament_object get_tournament(tournament_id_type id);

      /** Play a move in the rock-paper-scissors game
       * @param game_id the id of the game
       * @param player_account the name of the player
       * @param gesture rock, paper, or scissors
       * @param broadcast true if you wish to broadcast the transaction
       * @return the signed version of the transaction
       */
      signed_transaction rps_throw(game_id_type game_id,
                                   string player_account,
                                   rock_paper_scissors_gesture gesture,
                                   bool broadcast);

      signed_transaction create_custom_permission(string owner,
                                                  string permission_name,
                                                  authority auth,
                                                  bool broadcast = true);
      signed_transaction update_custom_permission(string owner,
                                                  custom_permission_id_type permission_id,
                                                  fc::optional<authority> new_auth,
                                                  bool broadcast = true);
      signed_transaction delete_custom_permission(string owner,
                                                  custom_permission_id_type permission_id,
                                                  bool broadcast = true);
      signed_transaction create_custom_account_authority(string owner,
                                                         custom_permission_id_type permission_id,
                                                         int operation_type,
                                                         fc::time_point_sec valid_from,
                                                         fc::time_point_sec valid_to,
                                                         bool broadcast = true);
      signed_transaction update_custom_account_authority(string owner,
                                                         custom_account_authority_id_type auth_id,
                                                         fc::optional<fc::time_point_sec> new_valid_from,
                                                         fc::optional<fc::time_point_sec> new_valid_to,
                                                         bool broadcast = true);
      signed_transaction delete_custom_account_authority(string owner,
                                                         custom_account_authority_id_type auth_id,
                                                         bool broadcast = true);
      vector<custom_permission_object> get_custom_permissions(string owner) const;
      fc::optional<custom_permission_object> get_custom_permission_by_name(string owner, string permission_name) const;
      vector<custom_account_authority_object> get_custom_account_authorities(string owner) const;
      vector<custom_account_authority_object> get_custom_account_authorities_by_permission_id(custom_permission_id_type permission_id) const;
      vector<custom_account_authority_object> get_custom_account_authorities_by_permission_name(string owner, string permission_name) const;
      vector<authority> get_active_custom_account_authorities_by_operation(string owner, int operation_type) const;

      /////////
      // NFT //
      /////////
      /**
       * @brief Creates NFT metadata
       * @param owner_account_id_or_name Owner account ID or name
       * @param name Name of the token group
       * @param symbol Symbol of the token group
       * @param base_uri Base URI for token URI
       * @param revenue_partner revenue partner for this type of Token
       * @param revenue_split revenue split for the sale
       * @param is_transferable can transfer the NFT or not
       * @param is_sellable can sell NFT or not
       * @param role_id account role id
       * @param max_supply max supply of NTFs
       * @param lottery_options lottery options
       * @param broadcast  true to broadcast transaction to the network
       * @return Signed transaction transfering the funds
       */
      signed_transaction nft_metadata_create(string owner_account_id_or_name,
                                    string name,
                                    string symbol,
                                    string base_uri,
                                    optional<string> revenue_partner,
                                    optional<uint16_t> revenue_split,
                                    bool is_transferable,
                                    bool is_sellable,
                                    optional<account_role_id_type> role_id,
                                    optional<share_type> max_supply,
                                    optional<nft_lottery_options> lottery_options,
                                    bool broadcast);

      /**
       * @brief Updates NFT metadata
       * @param owner_account_id_or_name Owner account ID or name
       * @param nft_metadata_id Metadata ID to modify
       * @param name Name of the token group
       * @param symbol Symbol of the token group
       * @param base_uri Base URI for token URI
       * @param revenue_partner revenue partner for this type of Token
       * @param revenue_split revenue split for the sale
       * @param is_transferable can transfer the NFT or not
       * @param is_sellable can sell NFT or not
       * @param role_id account role id
       * @param broadcast  true to broadcast transaction to the network
       * @return Signed transaction transfering the funds
       */
      signed_transaction nft_metadata_update(string owner_account_id_or_name,
                                    nft_metadata_id_type nft_metadata_id,
                                    optional<string> name,
                                    optional<string> symbol,
                                    optional<string> base_uri,
                                    optional<string> revenue_partner,
                                    optional<uint16_t> revenue_split,
                                    optional<bool> is_transferable,
                                    optional<bool> is_sellable,
                                    optional<account_role_id_type> role_id,
                                    bool broadcast);

      /**
       * @brief Creates NFT
       * @param metadata_owner_account_id_or_name NFT metadata owner account ID or name
       * @param metadata_id NFT metadata ID to which token will belong
       * @param owner_account_id_or_name Owner account ID or name
       * @param approved_account_id_or_name Approved account ID or name
       * @param token_uri Token URI (Will be combined with metadata base_uri if its not empty)
       * @param broadcast  true to broadcast transaction to the network
       * @return Signed transaction transfering the funds
       */
      signed_transaction nft_create(string metadata_owner_account_id_or_name,
                                    nft_metadata_id_type metadata_id,
                                    string owner_account_id_or_name,
                                    string approved_account_id_or_name,
                                    string token_uri,
                                    bool broadcast);

      /**
       * @brief Returns the number of NFT owned by account
       * @param owner_account_id_or_name Owner account ID or name
       * @return Number of NFTs owned by account
       */
      uint64_t nft_get_balance(string owner_account_id_or_name) const;

      /**
       * @brief Returns the NFT owner
       * @param token_id NFT ID
       * @return NFT owner account ID
       */
      optional<account_id_type> nft_owner_of(const nft_id_type token_id) const;

      /**
       * @brief Transfers NFT safely
       * @param operator_account_id_or_name Operators account ID or name
       * @param from_account_id_or_name Senders account ID or name
       * @param to_account_id_or_name Receivers account ID or name
       * @param token_id NFT ID
       * @param data Non mandatory data
       * @param broadcast  true to broadcast transaction to the network
       * @return Signed transaction transfering NFT
       */
      signed_transaction nft_safe_transfer_from(string operator_account_id_or_name,
                                                string from_account_id_or_name,
                                                string to_account_id_or_name,
                                                nft_id_type token_id,
                                                string data,
                                                bool broadcast);

      /**
       * @brief Transfers NFT
       * @param operator_account_id_or_name Operators account ID or name
       * @param from_account_id_or_name Senders account ID or name
       * @param to_account_id_or_name Receivers account ID or name
       * @param token_id NFT ID
       * @param broadcast  true to broadcast transaction to the network
       * @return Signed transaction transfering NFT
       */
      signed_transaction nft_transfer_from(string operator_account_id_or_name,
                                           string from_account_id_or_name,
                                           string to_account_id_or_name,
                                           nft_id_type token_id,
                                           bool broadcast);

      /**
       * @brief Sets approved account for NFT
       * @param operator_account_id_or_name Operators account ID or name
       * @param approved_account_id_or_name Senders account ID or name
       * @param token_id NFT ID
       * @param broadcast  true to broadcast transaction to the network
       * @return Signed transaction setting approving account for NFT
       */
      signed_transaction nft_approve(string operator_account_id_or_name,
                                     string approved_account_id_or_name,
                                     nft_id_type token_id,
                                     bool broadcast);

      /**
       * @brief Sets approval for all NFT owned by owner
       * @param owner_account_id_or_name Owner account ID or name
       * @param operator_account_id_or_name Operator account ID or name
       * @param approved true if approved
       * @param broadcast  true to broadcast transaction to the network
       * @return Signed transaction setting approvals for all NFT owned by owner
       */
      signed_transaction nft_set_approval_for_all(string owner_account_id_or_name,
                                                  string operator_account_id_or_name,
                                                  bool approved,
                                                  bool broadcast);

      /**
       * @brief Returns the NFT approved account ID
       * @param token_id NFT ID
       * @return NFT approved account ID
       */
      optional<account_id_type> nft_get_approved(const nft_id_type token_id) const;

      /**
       * @brief Returns operator approved state for all NFT owned by owner
       * @param owner_account_id_or_name NFT owner account ID or name
       * @param operator_account_id_or_name NFT operator account ID or name
       * @return True if operator is approved for all NFT owned by owner, else False
       */
      bool nft_is_approved_for_all(string owner_account_id_or_name, string operator_account_id_or_name) const;

      /**
       * @brief Returns all tokens
       * @return Returns vector of NFT objects, empty vector if none
       */
      vector<nft_object> nft_get_all_tokens() const;
      signed_transaction nft_lottery_buy_ticket( nft_metadata_id_type lottery, account_id_type buyer, uint64_t tickets_to_buy, bool broadcast );

      signed_transaction create_offer(set<nft_id_type> item_ids,
                                      string issuer_accound_id_or_name,
                                      asset minimum_price,
                                      asset maximum_price,
                                      bool buying_item,
                                      time_point_sec offer_expiration_date,
                                      optional<memo_data> memo,
                                      bool broadcast);
      signed_transaction create_bid(string bidder_account_id_or_name,
                                    asset bid_price,
                                    offer_id_type offer_id,
                                    bool broadcast);
      signed_transaction cancel_offer(string issuer_account_id_or_name,
                                      offer_id_type offer_id,
                                      bool broadcast);
      vector<offer_object> list_offers(uint32_t limit, optional<offer_id_type> lower_id) const;
      vector<offer_object> list_sell_offers(uint32_t limit, optional<offer_id_type> lower_id) const;
      vector<offer_object> list_buy_offers(uint32_t limit, optional<offer_id_type> lower_id) const;
      vector<offer_history_object> list_offer_history(uint32_t limit, optional<offer_history_id_type> lower_id) const;
      vector<offer_object> get_offers_by_issuer(string issuer_account_id_or_name,
                                                uint32_t limit, optional<offer_id_type> lower_id) const;
      vector<offer_object> get_offers_by_item(const nft_id_type item, uint32_t limit, optional<offer_id_type> lower_id) const;
      vector<offer_history_object> get_offer_history_by_issuer(string issuer_account_id_or_name, uint32_t limit, optional<offer_history_id_type> lower_id) const;
      vector<offer_history_object> get_offer_history_by_item(const nft_id_type item, uint32_t limit, optional<offer_history_id_type> lower_id) const;
      vector<offer_history_object> get_offer_history_by_bidder(string bidder_account_id_or_name, uint32_t limit, optional<offer_history_id_type> lower_id) const;

      signed_transaction create_account_role(string owner_account_id_or_name,
                                             string name,
                                             string metadata,
                                             flat_set<int> allowed_operations,
                                             flat_set<account_id_type> whitelisted_accounts,
                                             time_point_sec valid_to,
                                             bool broadcast);
      signed_transaction update_account_role(string owner_account_id_or_name,
                                             account_role_id_type role_id,
                                             optional<string> name,
                                             optional<string> metadata,
                                             flat_set<int> operations_to_add,
                                             flat_set<int> operations_to_remove,
                                             flat_set<account_id_type> accounts_to_add,
                                             flat_set<account_id_type> accounts_to_remove,
                                             optional<time_point_sec> valid_to,
                                             bool broadcast);
      signed_transaction delete_account_role(string owner_account_id_or_name,
                                             account_role_id_type role_id,
                                             bool broadcast);
      vector<account_role_object> get_account_roles_by_owner(string owner_account_id_or_name) const;

      void dbg_make_uia(string creator, string symbol);
      void dbg_make_mia(string creator, string symbol);
      void dbg_push_blocks( std::string src_filename, uint32_t count );
      void dbg_generate_blocks( std::string debug_wif_key, uint32_t count );
      void dbg_stream_json_objects( const std::string& filename );
      void dbg_update_object( fc::variant_object update );

      void flood_network(string prefix, uint32_t number_of_transactions);

      void network_add_nodes( const vector<string>& nodes );
      vector< variant > network_get_connected_peers();

      /**
       *  Used to transfer from one set of blinded balances to another
       */
      blind_confirmation blind_transfer_help( string from_key_or_label,
                                         string to_key_or_label,
                                         string amount,
                                         string symbol,
                                         bool broadcast = false,
                                         bool to_temp = false );

      std::map<string,std::function<string(fc::variant,const fc::variants&)>> get_result_formatters() const;

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

      /**
       * @brief Demo plugin api
       * @return The hello world string
       */
      std::map<sidechain_type, std::vector<std::string>> get_son_listener_log() const;

      fc::signal<void(bool)> lock_changed;
      std::shared_ptr<detail::wallet_api_impl> my;
      void encrypt_keys();
};

} }

extern template class fc::api<graphene::wallet::wallet_api>;

FC_REFLECT( graphene::wallet::key_label, (label)(key) )
FC_REFLECT( graphene::wallet::blind_balance, (amount)(from)(to)(one_time_key)(blinding_factor)(commitment)(used) )
FC_REFLECT( graphene::wallet::blind_confirmation::output, (label)(pub_key)(decrypted_memo)(confirmation)(auth)(confirmation_receipt) )
FC_REFLECT( graphene::wallet::blind_confirmation, (trx)(outputs) )

FC_REFLECT( graphene::wallet::plain_keys, (keys)(checksum) )

FC_REFLECT( graphene::wallet::wallet_data,
            (chain_id)
            (my_accounts)
            (cipher_keys)
            (extra_keys)
            (pending_account_registrations)(pending_witness_registrations)
            (labeled_keys)
            (blind_receipts)
            (committed_game_moves)
            (ws_server)
            (ws_user)
            (ws_password)
          )

FC_REFLECT( graphene::wallet::brain_key_info,
            (brain_priv_key)
            (wif_priv_key)
            (pub_key)
          )

FC_REFLECT_ENUM( graphene::wallet::authority_type, (owner)(active) )

FC_REFLECT( graphene::wallet::exported_account_keys, (account_name)(encrypted_private_keys)(public_keys) )

FC_REFLECT( graphene::wallet::exported_keys, (password_checksum)(account_keys) )

FC_REFLECT( graphene::wallet::blind_receipt,
            (date)(from_key)(from_label)(to_key)(to_label)(amount)(memo)(control_authority)(data)(used)(conf) )

FC_REFLECT( graphene::wallet::approval_delta,
   (active_approvals_to_add)
   (active_approvals_to_remove)
   (owner_approvals_to_add)
   (owner_approvals_to_remove)
   (key_approvals_to_add)
   (key_approvals_to_remove)
)

FC_REFLECT( graphene::wallet::worker_vote_delta,
   (vote_for)
   (vote_against)
   (vote_abstain)
)

FC_REFLECT_DERIVED( graphene::wallet::signed_block_with_info, (graphene::chain::signed_block),
   (block_id)(signing_key)(transaction_ids) )

FC_REFLECT_DERIVED( graphene::wallet::vesting_balance_object_with_info, (graphene::chain::vesting_balance_object),
   (allowed_withdraw)(allowed_withdraw_time) )

FC_REFLECT( graphene::wallet::operation_detail,
            (memo)(description)(op) )

FC_API( graphene::wallet::wallet_api,
        (help)
        (gethelp)
        (info)
        (about)
        (begin_builder_transaction)
        (add_operation_to_builder_transaction)
        (replace_operation_in_builder_transaction)
        (set_fees_on_builder_transaction)
        (preview_builder_transaction)
        (sign_builder_transaction)
        (broadcast_transaction)
        (propose_builder_transaction)
        (propose_builder_transaction2)
        (remove_builder_transaction)
        (is_new)
        (is_locked)
        (lock)(unlock)(set_password)
        (dump_private_keys)
        (list_my_accounts)
        (list_accounts)
        (list_account_balances)
        (list_assets)
        (get_asset_count)
        (import_key)
        (import_accounts)
        (import_account_keys)
        (import_balance)
        (suggest_brain_key)
        (derive_owner_keys_from_brain_key)
        (get_private_key_from_password)
        (register_account)
        (update_account_keys)
        (update_account_auth_key)
        (upgrade_account)
        (create_account_with_brain_key)
        (sell_asset)
        (sell)
        (buy)
        (borrow_asset)
        (cancel_order)
        (transfer)
        (transfer2)
        (get_transaction_id)
        (create_asset)
        (create_lottery)
        (update_asset)
        (update_bitasset)
        (update_dividend_asset)
        (update_asset_feed_producers)
        (publish_asset_feed)
        (issue_asset)
        (get_asset)
        (get_bitasset_data)
        (get_lotteries)
        (get_account_lotteries)
        (get_lottery_balance)
        (fund_asset_fee_pool)
        (reserve_asset)
        (global_settle_asset)
        (settle_asset)
        (whitelist_account)
        (create_committee_member)
        (get_son)
        (get_witness)
        (is_witness)
        (get_committee_member)
        (get_workers)
        (list_witnesses)
        (list_committee_members)
        (list_workers)
        (create_son)
        (try_create_son)
        (update_son)
        (update_son_vesting_balances)
        (activate_deregistered_son)
        (list_sons)
        (list_active_sons)
        (get_son_network_status)
        (request_son_maintenance)
        (cancel_request_son_maintenance)
        (get_active_son_wallet)
        (get_son_wallet_by_time_point)
        (get_son_wallets)
        (add_sidechain_address)
        (delete_sidechain_address)
	(sidechain_withdrawal_transaction)
        (get_sidechain_addresses_by_account)
        (get_sidechain_addresses_by_sidechain)
        (get_sidechain_address_by_account_and_sidechain)
        (get_sidechain_addresses_count)
        (create_witness)
        (update_witness)
        (create_worker)
        (update_worker_votes)
        (get_vesting_balances)
        (withdraw_vesting)
        (withdraw_GPOS_vesting_balance)
        (vote_for_committee_member)
        (vote_for_son)
        (update_son_votes)
        (sidechain_deposit_transaction)
        (vote_for_witness)
        (update_witness_votes)
        (set_voting_proxy)
        (set_desired_witness_and_committee_member_count)
        (get_account)
        (get_account_id)
        (get_block)
        (get_blocks)
        (get_account_count)
        (get_account_history)
        (get_relative_account_history)
        (is_public_key_registered)
        (list_core_accounts)
        (get_market_history)
        (get_global_properties)
        (get_dynamic_global_properties)
        (get_object)
        (get_private_key)
        (load_wallet_file)
        (normalize_brain_key)
        (get_limit_orders)
        (get_call_orders)
        (get_settle_orders)
        (save_wallet_file)
        (serialize_transaction)
        (sign_transaction)
        (get_transaction_signers)
        (get_key_references)
        (add_transaction_signature)
        (get_prototype_operation)
        (propose_parameter_change)
        (propose_fee_change)
        (propose_dividend_asset_update)
        (approve_proposal)
        (dbg_make_uia)
        (dbg_make_mia)
        (dbg_push_blocks)
        (dbg_generate_blocks)
        (dbg_stream_json_objects)
        (dbg_update_object)
        (flood_network)
        (network_add_nodes)
        (network_get_connected_peers)
        (set_key_label)
        (get_key_label)
        (get_public_key)
        (get_blind_accounts)
        (get_my_blind_accounts)
        (get_blind_balances)
        (create_blind_account)
        (transfer_to_blind)
        (transfer_from_blind)
        (blind_transfer)
        (blind_history)
        (receive_blind_transfer)
        (list_sports)
        (list_event_groups)
        (list_betting_market_groups)
        (list_betting_markets)
        (list_events_in_group)
        (get_unmatched_bets_for_bettor)
        (get_all_unmatched_bets_for_bettor)
        (get_global_betting_statistics)
        (propose_create_sport)
        (propose_create_event_group)
        (propose_create_event)
        (propose_create_betting_market_group)
        (propose_create_betting_market)
        (propose_create_betting_market_rules)
        (propose_update_betting_market_rules)
        (propose_update_sport)
        (propose_update_event_group)
        (propose_update_event)
        (propose_update_betting_market_group)
        (propose_update_betting_market)
        (propose_delete_sport)
        (propose_delete_event_group)
        (place_bet)
        (cancel_bet)
        (propose_resolve_betting_market_group)
        (tournament_create)
        (tournament_join)
        (tournament_leave)
        (rps_throw)
        (create_vesting_balance)
        (nft_metadata_create)
        (nft_metadata_update)
        (nft_create)
        (nft_get_balance)
        (nft_owner_of)
        (nft_safe_transfer_from)
        (nft_transfer_from)
        (nft_approve)
        (nft_set_approval_for_all)
        (nft_get_approved)
        (nft_is_approved_for_all)
        (nft_get_all_tokens)
        (nft_lottery_buy_ticket)
        (create_offer)
        (create_bid)
        (cancel_offer)
        (list_offers)
        (list_sell_offers)
        (list_buy_offers)
        (list_offer_history)
        (get_offers_by_issuer)
        (get_offers_by_item)
        (get_offer_history_by_issuer)
        (get_offer_history_by_item)
        (get_offer_history_by_bidder)
        (create_account_role)
        (update_account_role)
        (delete_account_role)
        (get_account_roles_by_owner)
        (get_upcoming_tournaments)
        (get_tournaments)
        (get_tournaments_by_state)
        (get_tournament)
        (get_order_book)
        (get_total_matched_bet_amount_for_betting_market_group)
        (get_events_containing_sub_string)
        (get_binned_order_book)
        (get_matched_bets_for_bettor)
        (get_all_matched_bets_for_bettor)
        (buy_ticket)
        (quit)
        (create_custom_permission)
        (update_custom_permission)
        (delete_custom_permission)
        (create_custom_account_authority)
        (update_custom_account_authority)
        (delete_custom_account_authority)
        (get_custom_permissions)
        (get_custom_permission_by_name)
        (get_custom_account_authorities)
        (get_custom_account_authorities_by_permission_id)
        (get_custom_account_authorities_by_permission_name)
        (get_active_custom_account_authorities_by_operation)
        (get_votes_ids)
        (get_votes)
        (get_voters_by_id)
        (get_voters)
        (get_son_listener_log)
      )
