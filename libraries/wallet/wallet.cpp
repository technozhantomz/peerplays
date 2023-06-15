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
#include <algorithm>
#include <cctype>
#include <iomanip>
#include <iostream>
#include <iterator>
#include <sstream>
#include <string>
#include <list>
#include <random>

#include <boost/version.hpp>
#include <boost/lexical_cast.hpp>
#include <boost/algorithm/string/replace.hpp>
#include <boost/multiprecision/integer.hpp>
#include <boost/algorithm/string/join.hpp>

#include <boost/range/adaptor/map.hpp>
#include <boost/range/algorithm_ext/erase.hpp>
#include <boost/range/algorithm/unique.hpp>
#include <boost/range/algorithm/sort.hpp>

#include <boost/multi_index_container.hpp>
#include <boost/multi_index/ordered_index.hpp>
#include <boost/multi_index/mem_fun.hpp>
#include <boost/multi_index/member.hpp>
#include <boost/multi_index/random_access_index.hpp>
#include <boost/multi_index/tag.hpp>
#include <boost/multi_index/sequenced_index.hpp>
#include <boost/multi_index/hashed_index.hpp>

#include <fc/git_revision.hpp>
#include <fc/io/fstream.hpp>
#include <fc/io/json.hpp>
#include <fc/io/stdio.hpp>
#include <fc/network/http/websocket.hpp>
#include <fc/rpc/api_connection.hpp>
#include <fc/rpc/cli.hpp>
#include <fc/rpc/websocket_api.hpp>
#include <fc/crypto/aes.hpp>
#include <fc/crypto/hex.hpp>
#include <fc/thread/mutex.hpp>
#include <fc/thread/scoped_lock.hpp>

#include <graphene/app/api.hpp>
#include <graphene/chain/asset_object.hpp>

#include <graphene/chain/tournament_object.hpp>
#include <graphene/chain/match_object.hpp>
#include <graphene/chain/game_object.hpp>
#include <graphene/chain/protocol/rock_paper_scissors.hpp>
#include <graphene/chain/rock_paper_scissors.hpp>

#include <graphene/bookie/bookie_api.hpp>
#include <graphene/chain/sidechain_defs.hpp>

#include <graphene/chain/protocol/fee_schedule.hpp>
#include <graphene/utilities/git_revision.hpp>
#include <graphene/utilities/key_conversion.hpp>
#include <graphene/utilities/words.hpp>
#include <graphene/wallet/wallet.hpp>
#include <graphene/wallet/api_documentation.hpp>
#include <graphene/wallet/reflect_util.hpp>
#include <graphene/debug_witness/debug_api.hpp>
#include <graphene/peerplays_sidechain/sidechain_api.hpp>

#ifndef WIN32
# include <sys/types.h>
# include <sys/stat.h>
#endif

template class fc::api<graphene::wallet::wallet_api>;

#define BRAIN_KEY_WORD_COUNT 16

namespace graphene { namespace wallet {

namespace detail {

struct operation_result_printer
{
public:
   explicit operation_result_printer( const wallet_api_impl& w )
      : _wallet(w) {}
   const wallet_api_impl& _wallet;
   typedef std::string result_type;

   std::string operator()(const void_result& x) const;
   std::string operator()(const object_id_type& oid);
   std::string operator()(const asset& a);
};

// BLOCK  TRX  OP  VOP
struct operation_printer
{
private:
   ostream& out;
   const wallet_api_impl& wallet;
   operation_result result;

   std::string fee(const asset& a) const;

public:
   operation_printer( ostream& out, const wallet_api_impl& wallet, const operation_result& r = operation_result() )
      : out(out),
        wallet(wallet),
        result(r)
   {}
   typedef std::string result_type;

   template<typename T>
   std::string operator()(const T& op)const;

   std::string operator()(const transfer_operation& op)const;
   std::string operator()(const transfer_from_blind_operation& op)const;
   std::string operator()(const transfer_to_blind_operation& op)const;
   std::string operator()(const account_create_operation& op)const;
   std::string operator()(const account_update_operation& op)const;
   std::string operator()(const asset_create_operation& op)const;
   std::string operator()(const lottery_asset_create_operation& op)const;
   std::string operator()(const asset_dividend_distribution_operation& op)const;
   std::string operator()(const tournament_payout_operation& op)const;
   std::string operator()(const bet_place_operation& op)const;
   std::string operator()(const bet_matched_operation& op)const;
   std::string operator()(const bet_canceled_operation& op)const;
   std::string operator()(const bet_adjusted_operation& op)const;
};

template<class T>
optional<T> maybe_id( const string& name_or_id )
{
   if( std::isdigit( name_or_id.front() ) )
   {
      try
      {
         return fc::variant(name_or_id, 1).as<T>(1);
      }
      catch (const fc::exception&)
      { // not an ID
      }
   }
   return optional<T>();
}

string address_to_shorthash( const address& addr )
{
   uint32_t x = addr.addr._hash[0];
   static const char hd[] = "0123456789abcdef";
   string result;

   result += hd[(x >> 0x1c) & 0x0f];
   result += hd[(x >> 0x18) & 0x0f];
   result += hd[(x >> 0x14) & 0x0f];
   result += hd[(x >> 0x10) & 0x0f];
   result += hd[(x >> 0x0c) & 0x0f];
   result += hd[(x >> 0x08) & 0x0f];
   result += hd[(x >> 0x04) & 0x0f];
   result += hd[(x        ) & 0x0f];

   return result;
}

fc::ecc::private_key derive_private_key( const std::string& prefix_string,
                                         int sequence_number )
{
   std::string sequence_string = std::to_string(sequence_number);
   fc::sha512 h = fc::sha512::hash(prefix_string + " " + sequence_string);
   fc::ecc::private_key derived_key = fc::ecc::private_key::regenerate(fc::sha256::hash(h));
   return derived_key;
}

string normalize_brain_key( string s )
{
   size_t i = 0, n = s.length();
   std::string result;
   char c;
   result.reserve( n );

   bool preceded_by_whitespace = false;
   bool non_empty = false;
   while( i < n )
   {
      c = s[i++];
      switch( c )
      {
      case ' ':  case '\t': case '\r': case '\n': case '\v': case '\f':
         preceded_by_whitespace = true;
         continue;

      case 'a': c = 'A'; break;
      case 'b': c = 'B'; break;
      case 'c': c = 'C'; break;
      case 'd': c = 'D'; break;
      case 'e': c = 'E'; break;
      case 'f': c = 'F'; break;
      case 'g': c = 'G'; break;
      case 'h': c = 'H'; break;
      case 'i': c = 'I'; break;
      case 'j': c = 'J'; break;
      case 'k': c = 'K'; break;
      case 'l': c = 'L'; break;
      case 'm': c = 'M'; break;
      case 'n': c = 'N'; break;
      case 'o': c = 'O'; break;
      case 'p': c = 'P'; break;
      case 'q': c = 'Q'; break;
      case 'r': c = 'R'; break;
      case 's': c = 'S'; break;
      case 't': c = 'T'; break;
      case 'u': c = 'U'; break;
      case 'v': c = 'V'; break;
      case 'w': c = 'W'; break;
      case 'x': c = 'X'; break;
      case 'y': c = 'Y'; break;
      case 'z': c = 'Z'; break;

      default:
         break;
      }
      if( preceded_by_whitespace && non_empty )
         result.push_back(' ');
      result.push_back(c);
      preceded_by_whitespace = false;
      non_empty = true;
   }
   return result;
}

struct op_prototype_visitor
{
   typedef void result_type;

   int t = 0;
   flat_map< std::string, operation >& name2op;

   op_prototype_visitor(
      int _t,
      flat_map< std::string, operation >& _prototype_ops
      ):t(_t), name2op(_prototype_ops) {}

   template<typename Type>
   result_type operator()( const Type& op )const
   {
      string name = fc::get_typename<Type>::name();
      size_t p = name.rfind(':');
      if( p != string::npos )
         name = name.substr( p+1 );
      name2op[ name ] = Type();
   }
};

class wallet_api_impl
{
public:
   api_documentation method_documentation;
private:
   void claim_registered_account(const account_object& account)
   {
      bool import_keys = false;
      auto it = _wallet.pending_account_registrations.find( account.name );
      FC_ASSERT( it != _wallet.pending_account_registrations.end() );
      for (const std::string& wif_key : it->second)
         if( !import_key( account.name, wif_key ) )
         {
            // somebody else beat our pending registration, there is
            //    nothing we can do except log it and move on
            elog( "account ${name} registered by someone else first!",
                  ("name", account.name) );
            // might as well remove it from pending regs,
            //    because there is now no way this registration
            //    can become valid (even in the extremely rare
            //    possibility of migrating to a fork where the
            //    name is available, the user can always
            //    manually re-register)
         } else {
            import_keys = true;
         }
      _wallet.pending_account_registrations.erase( it );

      if (import_keys)
         save_wallet_file();
   }

   // after a witness registration succeeds, this saves the private key in the wallet permanently
   //
   void claim_registered_witness(const std::string& witness_name)
   {
      auto iter = _wallet.pending_witness_registrations.find(witness_name);
      FC_ASSERT(iter != _wallet.pending_witness_registrations.end());
      std::string wif_key = iter->second;

      // get the list key id this key is registered with in the chain
      fc::optional<fc::ecc::private_key> witness_private_key = wif_to_key(wif_key);
      FC_ASSERT(witness_private_key);

      auto pub_key = witness_private_key->get_public_key();
      _keys[pub_key] = wif_key;
      _wallet.pending_witness_registrations.erase(iter);
   }

   fc::mutex _resync_mutex;
   void resync()
   {
      fc::scoped_lock<fc::mutex> lock(_resync_mutex);
      // this method is used to update wallet_data annotations
      //   e.g. wallet has been restarted and was not notified
      //   of events while it was down
      //
      // everything that is done "incremental style" when a push
      //   notification is received, should also be done here
      //   "batch style" by querying the blockchain

      if( !_wallet.pending_account_registrations.empty() )
      {
         // make a vector of the account names pending registration
         std::vector<string> pending_account_names = boost::copy_range<std::vector<string> >(boost::adaptors::keys(_wallet.pending_account_registrations));

         // look those up on the blockchain
         std::vector<fc::optional<graphene::chain::account_object >>
               pending_account_objects = _remote_db->lookup_account_names( pending_account_names );

         // if any of them exist, claim them
         for( const fc::optional<graphene::chain::account_object>& optional_account : pending_account_objects )
            if( optional_account )
               claim_registered_account(*optional_account);
      }

      if (!_wallet.pending_witness_registrations.empty())
      {
         // make a vector of the owner accounts for witnesses pending registration
         std::vector<string> pending_witness_names = boost::copy_range<std::vector<string> >(boost::adaptors::keys(_wallet.pending_witness_registrations));

         // look up the owners on the blockchain
         std::vector<fc::optional<graphene::chain::account_object>> owner_account_objects = _remote_db->lookup_account_names(pending_witness_names);

         // if any of them have registered witnesses, claim them
         for( const fc::optional<graphene::chain::account_object>& optional_account : owner_account_objects )
            if (optional_account)
            {
               std::string account_id = account_id_to_string(optional_account->id);
               fc::optional<witness_object> witness_obj = _remote_db->get_witness_by_account(account_id);
               if (witness_obj)
                  claim_registered_witness(optional_account->name);
            }
      }
   }

   // return true if any of my_accounts are players in this tournament
   bool tournament_is_relevant_to_my_accounts(const tournament_object& tournament_obj)
   {
      tournament_details_object tournament_details = get_object<tournament_details_object>(tournament_obj.tournament_details_id);
      for (const account_object& account_obj : _wallet.my_accounts)
         if (tournament_details.registered_players.find(account_obj.id) != tournament_details.registered_players.end())
            return true;
      return false;
   }

   fc::mutex _subscribed_object_changed_mutex;
   void subscribed_object_changed(const variant& changed_objects_variant)
   {
      fc::scoped_lock<fc::mutex> lock(_resync_mutex);
      fc::variants changed_objects = changed_objects_variant.get_array();
      for (const variant& changed_object_variant : changed_objects)
      {
         // changed_object_variant is either the object, or just the id if the object was removed
         if (changed_object_variant.is_object())
         {
            try
            {
               object_id_type id = changed_object_variant["id"].as<tournament_id_type>( GRAPHENE_MAX_NESTED_OBJECTS );
               tournament_object current_tournament_obj = changed_object_variant.as<tournament_object>( GRAPHENE_MAX_NESTED_OBJECTS );
               auto tournament_cache_iter = tournament_cache.find(id);
               if (tournament_cache_iter != tournament_cache.end())
               {
                  const tournament_object& cached_tournament_obj = *tournament_cache_iter;
                  if (cached_tournament_obj.get_state() != current_tournament_obj.get_state())
                  {
                     ilog("Tournament ${id} changed state from ${old} to ${new}",
                          ("id", id)
                          ("old", cached_tournament_obj.get_state())
                          ("new", current_tournament_obj.get_state()));
                     if (current_tournament_obj.get_state() == tournament_state::in_progress)
                        monitor_matches_in_tournament(current_tournament_obj);
                  }
                  tournament_cache.modify(tournament_cache_iter, [&](tournament_object& obj) { obj = current_tournament_obj; });
               }
               else if (tournament_is_relevant_to_my_accounts(current_tournament_obj))
               {
                  ilog ("We were just notified about an in-progress tournament ${id} relevant to our accounts",
                        ("id", current_tournament_obj.id));
                  tournament_cache.insert(current_tournament_obj);
                  if (current_tournament_obj.get_state() == tournament_state::in_progress)
                     monitor_matches_in_tournament(current_tournament_obj);
               }
               continue;
            }
            catch (const fc::exception& e)
            {
            //   idump((e));
            }
            try
            {
               object_id_type id = changed_object_variant["id"].as<match_id_type>( GRAPHENE_MAX_NESTED_OBJECTS );
               match_object current_match_obj = changed_object_variant.as<match_object>( GRAPHENE_MAX_NESTED_OBJECTS );
               auto match_cache_iter = match_cache.find(id);
               if (match_cache_iter != match_cache.end())
               {
                  const match_object& cached_match_obj = *match_cache_iter;
                  if (cached_match_obj.get_state() != current_match_obj.get_state() ||
                      cached_match_obj.games.size() != current_match_obj.games.size())
                  {
                     ilog("match ${id} changed state from ${old} to ${new}",
                          ("id", id)
                          ("old", cached_match_obj.get_state())
                          ("new", current_match_obj.get_state()));
                     match_in_new_state(current_match_obj);
                  }
                  match_cache.modify(match_cache_iter, [&](match_object& obj) { obj = current_match_obj; });
               }
               continue;
            }
            catch (const fc::exception& e)
            {
            //   idump((e));
            }
            try
            {
               object_id_type id = changed_object_variant["id"].as<game_id_type>( GRAPHENE_MAX_NESTED_OBJECTS );
               game_object current_game_obj = changed_object_variant.as<game_object>( GRAPHENE_MAX_NESTED_OBJECTS );
               auto game_cache_iter = game_cache.find(id);
               if (game_cache_iter != game_cache.end())
               {
                  const game_object& cached_game_obj = *game_cache_iter;
                  if (cached_game_obj.get_state() != current_game_obj.get_state())
                  {
                     ilog("game ${id} changed state from ${old} to ${new}",
                          ("id", id)
                          ("old", cached_game_obj.get_state())
                          ("new", current_game_obj.get_state()));
                     game_in_new_state(current_game_obj);
                  }
                  game_cache.modify(game_cache_iter, [&](game_object& obj) { obj = current_game_obj; });
               }
               continue;
            }
            catch (const fc::exception& e)
            {
            //   idump((e));
            }
            try
            {
               object_id_type id = changed_object_variant["id"].as<account_id_type>( GRAPHENE_MAX_NESTED_OBJECTS );
               if (_wallet.my_accounts.find(id) != _wallet.my_accounts.end())
               {
                  account_object account = changed_object_variant.as<account_object>( GRAPHENE_MAX_NESTED_OBJECTS );
                  _wallet.update_account(account);
               }
               continue;
            }
            catch (const fc::exception& e)
            {
            //   idump((e));
            }
         }
      }
   }

   void enable_umask_protection()
   {
#ifdef __unix__
      _old_umask = umask( S_IRWXG | S_IRWXO );
#endif
   }

   void disable_umask_protection()
   {
#ifdef __unix__
      umask( _old_umask );
#endif
   }

   void init_prototype_ops()
   {
      operation op;
      for( int t=0; t<op.count(); t++ )
      {
         op.set_which( t );
         op.visit( op_prototype_visitor(t, _prototype_ops) );
      }
      return;
   }

   map<transaction_handle_type, signed_transaction> _builder_transactions;

   // if the user executes the same command twice in quick succession,
   // we might generate the same transaction id, and cause the second
   // transaction to be rejected.  This can be avoided by altering the
   // second transaction slightly (bumping up the expiration time by
   // a second).  Keep track of recent transaction ids we've generated
   // so we can know if we need to do this
   struct recently_generated_transaction_record
   {
      fc::time_point_sec generation_time;
      graphene::chain::transaction_id_type transaction_id;
   };
   struct timestamp_index{};
   typedef boost::multi_index_container<recently_generated_transaction_record,
                                        boost::multi_index::indexed_by<boost::multi_index::hashed_unique<boost::multi_index::member<recently_generated_transaction_record,
                                                                                                                                    graphene::chain::transaction_id_type,
                                                                                                                                    &recently_generated_transaction_record::transaction_id>,
                                                                                                         std::hash<graphene::chain::transaction_id_type> >,
                                                                       boost::multi_index::ordered_non_unique<boost::multi_index::tag<timestamp_index>,
                                                                                                              boost::multi_index::member<recently_generated_transaction_record, fc::time_point_sec, &recently_generated_transaction_record::generation_time> > > > recently_generated_transaction_set_type;
   recently_generated_transaction_set_type _recently_generated_transactions;

public:
   wallet_api& self;
   wallet_api_impl( wallet_api& s, const wallet_data& initial_data, fc::api<login_api> rapi )
      : self(s),
        _chain_id(initial_data.chain_id),
        _remote_api(rapi),
        _remote_db(rapi->database()),
        _remote_net_broadcast(rapi->network_broadcast()),
        _remote_hist(rapi->history()),
        _remote_bookie(rapi->bookie())
   {
      chain_id_type remote_chain_id = _remote_db->get_chain_id();
      if( remote_chain_id != _chain_id )
      {
         FC_THROW( "Remote server gave us an unexpected chain_id",
            ("remote_chain_id", remote_chain_id)
            ("chain_id", _chain_id) );
      }
      init_prototype_ops();

      _remote_db->set_block_applied_callback( [this](const variant& block_id )
      {
         on_block_applied( block_id );
      } );

      _remote_db->set_subscribe_callback( [this](const variant& object )
      {
         on_subscribe_callback( object );
      }, false );

      _wallet.chain_id = _chain_id;
      _wallet.ws_server = initial_data.ws_server;
      _wallet.ws_user = initial_data.ws_user;
      _wallet.ws_password = initial_data.ws_password;

   }
   virtual ~wallet_api_impl()
   {
      try
      {
         _remote_db->cancel_all_subscriptions();
      }
      catch (const fc::exception& e)
      {
         // Right now the wallet_api has no way of knowing if the connection to the
         // witness has already disconnected (via the witness node exiting first).
         // If it has exited, cancel_all_subscriptsions() will throw and there's
         // nothing we can do about it.
         // dlog("Caught exception ${e} while canceling database subscriptions", ("e", e));
      }
   }

   void encrypt_keys()
   {
      if( !is_locked() )
      {
         plain_keys data;
         data.keys = _keys;
         data.checksum = _checksum;
         auto plain_txt = fc::raw::pack(data);
         _wallet.cipher_keys = fc::aes_encrypt( data.checksum, plain_txt );
      }
   }

   void on_block_applied( const variant& block_id )
   {
      fc::async([this]{resync();}, "Resync after block");
   }

   void on_subscribe_callback( const variant& object )
   {
      //idump((object));
      fc::async([this, object]{subscribed_object_changed(object);}, "Object changed");
   }

   void quit()
   {
      ilog( "Quitting Cli Wallet ..." );

      throw fc::canceled_exception();
   }

   bool copy_wallet_file( string destination_filename )
   {
      fc::path src_path = get_wallet_filename();
      if( !fc::exists( src_path ) )
         return false;
      fc::path dest_path = destination_filename + _wallet_filename_extension;
      int suffix = 0;
      while( fc::exists(dest_path) )
      {
         ++suffix;
         dest_path = destination_filename + "-" + to_string( suffix ) + _wallet_filename_extension;
      }
      wlog( "backing up wallet ${src} to ${dest}",
            ("src", src_path)
            ("dest", dest_path) );

      fc::path dest_parent = fc::absolute(dest_path).parent_path();
      try
      {
         enable_umask_protection();
         if( !fc::exists( dest_parent ) )
            fc::create_directories( dest_parent );
         fc::copy( src_path, dest_path );
         disable_umask_protection();
      }
      catch(...)
      {
         disable_umask_protection();
         throw;
      }
      return true;
   }

   bool is_locked()const
   {
      return _checksum == fc::sha512();
   }

   template<typename T>
   T get_object(object_id<T::space_id, T::type_id, T> id)const
   {
      auto ob = _remote_db->get_objects({id}).front();
      return ob.template as<T>( GRAPHENE_MAX_NESTED_OBJECTS );
   }

   void set_operation_fees( signed_transaction& tx, std::shared_ptr<fee_schedule> s  )
   {
      for( auto& op : tx.operations )
         s->set_fee(op);
   }

   variant info() const
   {
      auto chain_props = get_chain_properties();
      auto global_props = get_global_properties();
      auto dynamic_props = get_dynamic_global_properties();
      fc::mutable_variant_object result;
      result["head_block_num"] = dynamic_props.head_block_number;
      result["head_block_id"] = fc::variant(dynamic_props.head_block_id, 1);
      result["head_block_age"] = fc::get_approximate_relative_time_string(dynamic_props.time,
                                                                          time_point_sec(time_point::now()),
                                                                          " old");
      result["next_maintenance_time"] = fc::get_approximate_relative_time_string(dynamic_props.next_maintenance_time);
      result["chain_id"] = chain_props.chain_id;
      result["participation"] = (100*dynamic_props.recent_slots_filled.popcount()) / 128.0;
      result["active_witnesses"] = fc::variant(global_props.active_witnesses, GRAPHENE_MAX_NESTED_OBJECTS);
      result["active_committee_members"] = fc::variant(global_props.active_committee_members, GRAPHENE_MAX_NESTED_OBJECTS);
      result["active_sons"] = fc::variant(global_props.active_sons, GRAPHENE_MAX_NESTED_OBJECTS);
      result["entropy"] = fc::variant(dynamic_props.random, GRAPHENE_MAX_NESTED_OBJECTS);
      return result;
   }

   variant_object about() const
   {
      string client_version( graphene::utilities::git_revision_description );
      const size_t pos = client_version.find( '/' );
      if( pos != string::npos && client_version.size() > pos )
         client_version = client_version.substr( pos + 1 );

      fc::mutable_variant_object result;
      //result["blockchain_name"]        = BLOCKCHAIN_NAME;
      //result["blockchain_description"] = BTS_BLOCKCHAIN_DESCRIPTION;
      result["client_version"]           = client_version;
      result["graphene_revision"]        = graphene::utilities::git_revision_sha;
      result["graphene_revision_age"]    = fc::get_approximate_relative_time_string( fc::time_point_sec( graphene::utilities::git_revision_unix_timestamp ) );
      result["fc_revision"]              = fc::git_revision_sha;
      result["fc_revision_age"]          = fc::get_approximate_relative_time_string( fc::time_point_sec( fc::git_revision_unix_timestamp ) );
      result["compile_date"]             = "compiled on " __DATE__ " at " __TIME__;
      result["boost_version"]            = boost::replace_all_copy(std::string(BOOST_LIB_VERSION), "_", ".");
      result["openssl_version"]          = OPENSSL_VERSION_TEXT;

      std::string bitness = boost::lexical_cast<std::string>(8 * sizeof(int*)) + "-bit";
#if defined(__APPLE__)
      std::string os = "osx";
#elif defined(__linux__)
      std::string os = "linux";
#elif defined(_MSC_VER)
      std::string os = "win32";
#else
      std::string os = "other";
#endif
      result["build"] = os + " " + bitness;

      return result;
   }

   chain_property_object get_chain_properties() const
   {
      return _remote_db->get_chain_properties();
   }
   global_property_object get_global_properties() const
   {
      return _remote_db->get_global_properties();
   }
   dynamic_global_property_object get_dynamic_global_properties() const
   {
      return _remote_db->get_dynamic_global_properties();
   }
   std::string object_id_to_string(object_id_type id) const
   {
      std::string object_id = fc::to_string(id.space())
                              + "." + fc::to_string(id.type())
                              + "." + fc::to_string(id.instance());
      return object_id;
   }
   std::string account_id_to_string(account_id_type id) const
   {
      return object_id_to_string(id);
   }
   account_object get_account(account_id_type id) const
   {
      std::string account_id = account_id_to_string(id);
      auto rec = _remote_db->get_accounts({account_id}).front();
      FC_ASSERT(rec, "Accout id: ${account_id} doesn't exist", ("account_id", account_id));
      return *rec;
   }
   account_object get_account(string account_name_or_id) const
   {
      FC_ASSERT( account_name_or_id.size() > 0 );

      if( auto id = maybe_id<account_id_type>(account_name_or_id) )
      {
         // It's an ID
         return get_account(*id);
      } else {
         auto rec = _remote_db->lookup_account_names({account_name_or_id}).front();
         FC_ASSERT( rec && rec->name == account_name_or_id, "Account name or id: ${account_name_or_id} doesn't exist", ("account_name_or_id",account_name_or_id ) );
         return *rec;
      }
   }
   account_id_type get_account_id(string account_name_or_id) const
   {
      return get_account(account_name_or_id).get_id();
   }
   std::string asset_id_to_string(asset_id_type id) const
   {
      std::string asset_id = fc::to_string(id.space_id) +
                             "." + fc::to_string(id.type_id) +
                             "." + fc::to_string(id.instance.value);
      return asset_id;
   }
   optional<asset_object> find_asset(asset_id_type id)const
   {
      auto rec = _remote_db->get_assets({asset_id_to_string(id)}).front();
      if( rec )
         _asset_cache[id] = *rec;
      return rec;
   }
   optional<asset_object> find_asset(string asset_symbol_or_id)const
   {
      FC_ASSERT( asset_symbol_or_id.size() > 0 );

      if( auto id = maybe_id<asset_id_type>(asset_symbol_or_id) )
      {
         // It's an ID
         return find_asset(*id);
      } else {
         // It's a symbol
         auto rec = _remote_db->lookup_asset_symbols({asset_symbol_or_id}).front();
         if( rec )
         {
            if( rec->symbol != asset_symbol_or_id )
               return optional<asset_object>();

            _asset_cache[rec->get_id()] = *rec;
         }
         return rec;
      }
   }
   asset_object get_asset(asset_id_type id)const
   {
      auto opt = find_asset(id);
      FC_ASSERT(opt);
      return *opt;
   }
   asset_object get_asset(string asset_symbol_or_id)const
   {
      auto opt = find_asset(asset_symbol_or_id);
      FC_ASSERT(opt);
      return *opt;
   }

   asset_id_type get_asset_id(string asset_symbol_or_id) const
   {
      FC_ASSERT( asset_symbol_or_id.size() > 0 );
      vector<optional<asset_object>> opt_asset;
      if( std::isdigit( asset_symbol_or_id.front() ) )
         return fc::variant(asset_symbol_or_id, 1).as<asset_id_type>( 1 );
      opt_asset = _remote_db->lookup_asset_symbols( {asset_symbol_or_id} );
      FC_ASSERT( (opt_asset.size() > 0) && (opt_asset[0].valid()) );
      return opt_asset[0]->id;
   }

   string                            get_wallet_filename() const
   {
      return _wallet_filename;
   }

   fc::ecc::private_key              get_private_key(const public_key_type& id)const
   {
      auto it = _keys.find(id);
      FC_ASSERT( it != _keys.end() );

      fc::optional< fc::ecc::private_key > privkey = wif_to_key( it->second );
      FC_ASSERT( privkey );
      return *privkey;
   }

   fc::ecc::private_key get_private_key_for_account(const account_object& account)const
   {
      vector<public_key_type> active_keys = account.active.get_keys();
      if (active_keys.size() != 1)
         FC_THROW("Expecting a simple authority with one active key");
      return get_private_key(active_keys.front());
   }

   // imports the private key into the wallet, and associate it in some way (?) with the
   // given account name.
   // @returns true if the key matches a current active/owner/memo key for the named
   //          account, false otherwise (but it is stored either way)
   bool import_key(string account_name_or_id, string wif_key)
   {
      fc::scoped_lock<fc::mutex> lock(_resync_mutex);
      fc::optional<fc::ecc::private_key> optional_private_key = wif_to_key(wif_key);
      if (!optional_private_key)
         FC_THROW("Invalid private key");
      graphene::chain::public_key_type wif_pub_key = optional_private_key->get_public_key();

      account_object account = get_account( account_name_or_id );

      // make a list of all current public keys for the named account
      flat_set<public_key_type> all_keys_for_account;
      std::vector<public_key_type> active_keys = account.active.get_keys();
      std::vector<public_key_type> owner_keys = account.owner.get_keys();
      std::copy(active_keys.begin(), active_keys.end(), std::inserter(all_keys_for_account, all_keys_for_account.end()));
      std::copy(owner_keys.begin(), owner_keys.end(), std::inserter(all_keys_for_account, all_keys_for_account.end()));
      all_keys_for_account.insert(account.options.memo_key);

      _keys[wif_pub_key] = wif_key;

      _wallet.update_account(account);

      _wallet.extra_keys[account.id].insert(wif_pub_key);

      return all_keys_for_account.find(wif_pub_key) != all_keys_for_account.end();
   }

   vector< signed_transaction > import_balance( string name_or_id, const vector<string>& wif_keys, bool broadcast );

   void game_in_new_state(const game_object& game_obj)
   { try {
      if (game_obj.get_state() == game_state::expecting_commit_moves)
      {
         if (game_obj.players.size() != 2) // we only support RPS, a 2 player game
            return;
         const rock_paper_scissors_game_details& rps_details = game_obj.game_details.get<rock_paper_scissors_game_details>();
         for (unsigned i = 0; i < 2; ++i)
         {
            if (!rps_details.commit_moves.at(i)) // if this player hasn't committed their move
            {
               const account_id_type& account_id = game_obj.players[i];
               if (_wallet.my_accounts.find(account_id) != _wallet.my_accounts.end()) // and they're us
               {
                  ilog("Game ${game_id}: it is ${account_name}'s turn to commit their move",
                       ("game_id", game_obj.id)
                       ("account_name", get_account(account_id).name));
               }
            }
         }
      }
      else if (game_obj.get_state() == game_state::expecting_reveal_moves)
      {
         if (game_obj.players.size() != 2) // we only support RPS, a 2 player game
            return;
         const rock_paper_scissors_game_details& rps_details = game_obj.game_details.get<rock_paper_scissors_game_details>();
         for (unsigned i = 0; i < 2; ++i)
         {
            if (rps_details.commit_moves.at(i) &&
                !rps_details.reveal_moves.at(i)) // if this player has committed but not revealed
            {
               const account_id_type& account_id = game_obj.players[i];
               if (_wallet.my_accounts.find(account_id) != _wallet.my_accounts.end()) // and they're us
               {
                  if (self.is_locked())
                     ilog("Game ${game_id}: unable to broadcast ${account_name}'s reveal because the wallet is locked",
                          ("game_id", game_obj.id)
                          ("account_name", get_account(account_id).name));
                  else
                  {
                     ilog("Game ${game_id}: it is ${account_name}'s turn to reveal their move",
                          ("game_id", game_obj.id)
                          ("account_name", get_account(account_id).name));

                     auto iter = _wallet.committed_game_moves.find(*rps_details.commit_moves.at(i));
                     if (iter != _wallet.committed_game_moves.end())
                     {
                        const rock_paper_scissors_throw_reveal& reveal = iter->second;

                        game_move_operation move_operation;
                        move_operation.game_id = game_obj.id;
                        move_operation.player_account_id = account_id;
                        move_operation.move = reveal;

                        signed_transaction trx;
                        trx.operations = {move_operation};
                        set_operation_fees( trx, _remote_db->get_global_properties().parameters.current_fees);
                        trx.validate();
                        ilog("Broadcasting reveal...");
                        trx = sign_transaction(trx, true);
                        ilog("Reveal broadcast, transaction id is ${id}", ("id", trx.id()));
                     }
                  }
               }
            }
         }
      }
   } FC_RETHROW_EXCEPTIONS(warn, "") }

   void match_in_new_state(const match_object& match_obj)
   { try {
      if (match_obj.get_state() == match_state::match_in_progress)
      {
         for (const account_id_type& account_id : match_obj.players)
         {
            if (_wallet.my_accounts.find(account_id) != _wallet.my_accounts.end())
            {
               ilog("Match ${match} is now in progress for player ${account}",
                    ("match", match_obj.id)("account", get_account(account_id).name));
               for (const game_id_type& game_id : match_obj.games)
               {
                  game_object game_obj = get_object<game_object>(game_id);
                  auto insert_result = game_cache.insert(game_obj);
                  if (insert_result.second)
                     game_in_new_state(game_obj);
               }
            }
         }
      }
   } FC_RETHROW_EXCEPTIONS(warn, "") }

   // Cache all matches in the tournament, which will also register us for
   // updates on those matches
   void monitor_matches_in_tournament(const tournament_object& tournament_obj)
   { try {
      tournament_details_object tournament_details = get_object<tournament_details_object>(tournament_obj.tournament_details_id);
      for (const match_id_type& match_id : tournament_details.matches)
      {
         match_object match_obj = get_object<match_object>(match_id);
         auto insert_result = match_cache.insert(match_obj);
         if (insert_result.second)
            match_in_new_state(match_obj);
      }
   } FC_RETHROW_EXCEPTIONS(warn, "") }

   void resync_active_tournaments()
   {
      // check to see if any of our accounts are registered for tournaments
      // the real purpose of this is to ensure that we are subscribed for callbacks on these tournaments
      ilog("Checking my accounts for active tournaments",);
      tournament_cache.clear();
      match_cache.clear();
      game_cache.clear();
      for (const account_object& my_account : _wallet.my_accounts)
      {
         std::vector<tournament_id_type> tournament_ids = _remote_db->get_registered_tournaments(my_account.id, 100);
         for (const tournament_id_type& tournament_id : tournament_ids)
         {
            try
            {
               tournament_object tournament = get_object<tournament_object>(tournament_id);
               auto insert_result = tournament_cache.insert(tournament);
               if (insert_result.second)
               {
                  // then this is the first time we've seen this tournament
                  monitor_matches_in_tournament(tournament);
               }
               tournament_ids.push_back(tournament.id);
            }
            catch (const fc::exception& e)
            {
               edump((e)(tournament_id));
            }
         }
         if (!tournament_ids.empty())
            ilog("Account ${my_account} is registered for tournaments: ${tournaments}", ("my_account", my_account.name)("tournaments", tournament_ids));
         else
            ilog("Account ${my_account} is not registered for any tournaments", ("my_account", my_account.name));
      }
   }

   bool load_wallet_file(string wallet_filename = "")
   {
      // TODO:  Merge imported wallet with existing wallet,
      //        instead of replacing it
      if( wallet_filename == "" )
         wallet_filename = _wallet_filename;

      if( ! fc::exists( wallet_filename ) )
         return false;

      _wallet = fc::json::from_file( wallet_filename ).as< wallet_data >( 2 * GRAPHENE_MAX_NESTED_OBJECTS );
      if( _wallet.chain_id != _chain_id )
         FC_THROW( "Wallet chain ID does not match",
            ("wallet.chain_id", _wallet.chain_id)
            ("chain_id", _chain_id) );

      size_t account_pagination = 100;
      vector< std::string > account_ids_to_send;
      size_t n = _wallet.my_accounts.size();
      account_ids_to_send.reserve( std::min( account_pagination, n ) );
      auto it = _wallet.my_accounts.begin();

      for( size_t start=0; start<n; start+=account_pagination )
      {
         size_t end = std::min( start+account_pagination, n );
         assert( end > start );
         account_ids_to_send.clear();
         std::vector< account_object > old_accounts;
         for( size_t i=start; i<end; i++ )
         {
            assert( it != _wallet.my_accounts.end() );
            old_accounts.push_back( *it );
            std::string account_id = account_id_to_string(old_accounts.back().id);
            account_ids_to_send.push_back( account_id );
            ++it;
         }
         std::vector< optional< account_object > > accounts = _remote_db->get_accounts(account_ids_to_send);
         // server response should be same length as request
         FC_ASSERT( accounts.size() == account_ids_to_send.size() );
         size_t i = 0;
         for( const optional< account_object >& acct : accounts )
         {
            account_object& old_acct = old_accounts[i];
            if( !acct.valid() )
            {
               elog( "Could not find account ${id} : \"${name}\" does not exist on the chain!", ("id", old_acct.id)("name", old_acct.name) );
               i++;
               continue;
            }
            // this check makes sure the server didn't send results
            // in a different order, or accounts we didn't request
            FC_ASSERT( acct->id == old_acct.id );
            if( fc::json::to_string(*acct) != fc::json::to_string(old_acct) )
            {
               wlog( "Account ${id} : \"${name}\" updated on chain", ("id", acct->id)("name", acct->name) );
            }
            _wallet.update_account( *acct );
            i++;
         }
      }

      resync_active_tournaments();

      return true;
   }

   void save_wallet_file(string wallet_filename = "")
   {
      //
      // Serialize in memory, then save to disk
      //
      // This approach lessens the risk of a partially written wallet
      // if exceptions are thrown in serialization
      //

      encrypt_keys();

      if( wallet_filename == "" )
         wallet_filename = _wallet_filename;

      ilog( "saving wallet to file ${fn}", ("fn", wallet_filename) );

      string data = fc::json::to_pretty_string( _wallet );
      try
      {
         enable_umask_protection();
         //
         // Parentheses on the following declaration fails to compile,
         // due to the Most Vexing Parse.  Thanks, C++
         //
         // http://en.wikipedia.org/wiki/Most_vexing_parse
         //
         std::string tmp_wallet_filename = wallet_filename + ".tmp";
         fc::ofstream outfile{ fc::path( tmp_wallet_filename ) };
         outfile.write( data.c_str(), data.length() );
         outfile.flush();
         outfile.close();

         ilog( "saved successfully wallet to tmp file ${fn}", ("fn", tmp_wallet_filename) );

         std::string wallet_file_content;
         fc::read_file_contents(tmp_wallet_filename, wallet_file_content);

         if (wallet_file_content == data) {
            dlog( "validated successfully tmp wallet file ${fn}", ("fn", tmp_wallet_filename) );
            fc::rename( tmp_wallet_filename, wallet_filename );
            dlog( "renamed successfully tmp wallet file ${fn}", ("fn", tmp_wallet_filename) );
         }
         else
         {
            FC_THROW("tmp wallet file cannot be validated ${fn}", ("fn", tmp_wallet_filename) );
         }

         ilog( "successfully saved wallet to file ${fn}", ("fn", wallet_filename) );

         disable_umask_protection();
      }
      catch(...)
      {
         string ws_password = _wallet.ws_password;
         _wallet.ws_password = "";
         elog("wallet file content is: ${data}", ("data", fc::json::to_pretty_string( _wallet ) ) );
         _wallet.ws_password = ws_password;

         disable_umask_protection();
         throw;
      }
   }

   transaction_handle_type begin_builder_transaction()
   {
      int trx_handle = _builder_transactions.empty()? 0
                                                    : (--_builder_transactions.end())->first + 1;
      _builder_transactions[trx_handle];
      return trx_handle;
   }
   void add_operation_to_builder_transaction(transaction_handle_type transaction_handle, const operation& op)
   {
      FC_ASSERT(_builder_transactions.count(transaction_handle));
      _builder_transactions[transaction_handle].operations.emplace_back(op);
   }
   void replace_operation_in_builder_transaction(transaction_handle_type handle,
                                                 uint32_t operation_index,
                                                 const operation& new_op)
   {
      FC_ASSERT(_builder_transactions.count(handle));
      signed_transaction& trx = _builder_transactions[handle];
      FC_ASSERT( operation_index < trx.operations.size());
      trx.operations[operation_index] = new_op;
   }
   asset set_fees_on_builder_transaction(transaction_handle_type handle, string fee_asset = GRAPHENE_SYMBOL)
   {
      FC_ASSERT(_builder_transactions.count(handle));

      auto fee_asset_obj = get_asset(fee_asset);
      asset total_fee = fee_asset_obj.amount(0);

      auto gprops = _remote_db->get_global_properties().parameters;
      if( fee_asset_obj.get_id() != asset_id_type() )
      {
         for( auto& op : _builder_transactions[handle].operations )
            total_fee += gprops.current_fees->set_fee( op, fee_asset_obj.options.core_exchange_rate );

         FC_ASSERT((total_fee * fee_asset_obj.options.core_exchange_rate).amount <=
                   get_object<asset_dynamic_data_object>(fee_asset_obj.dynamic_asset_data_id).fee_pool,
                   "Cannot pay fees in ${asset}, as this asset's fee pool is insufficiently funded.",
                   ("asset", fee_asset_obj.symbol));
      } else {
         for( auto& op : _builder_transactions[handle].operations )
            total_fee += gprops.current_fees->set_fee( op );
      }

      return total_fee;
   }
   transaction preview_builder_transaction(transaction_handle_type handle)
   {
      FC_ASSERT(_builder_transactions.count(handle));
      return _builder_transactions[handle];
   }
   signed_transaction sign_builder_transaction(transaction_handle_type transaction_handle, bool broadcast = true)
   {
      FC_ASSERT(_builder_transactions.count(transaction_handle));

      return _builder_transactions[transaction_handle] = sign_transaction(_builder_transactions[transaction_handle], broadcast);
   }

   pair<transaction_id_type,signed_transaction> broadcast_transaction(signed_transaction tx)
   {
       try {
           _remote_net_broadcast->broadcast_transaction(tx);
       }
       catch (const fc::exception& e) {
           elog("Caught exception while broadcasting tx ${id}:  ${e}",
                ("id", tx.id().str())("e", e.to_detail_string()));
           throw;
       }
       return std::make_pair(tx.id(),tx);
   }

   signed_transaction propose_builder_transaction(
      transaction_handle_type handle,
      time_point_sec expiration = time_point::now() + fc::minutes(1),
      uint32_t review_period_seconds = 0, bool broadcast = true)
   {
      FC_ASSERT(_builder_transactions.count(handle));
      proposal_create_operation op;
      op.expiration_time = expiration;
      signed_transaction& trx = _builder_transactions[handle];
      std::transform(trx.operations.begin(), trx.operations.end(), std::back_inserter(op.proposed_ops),
                     [](const operation& op) -> op_wrapper { return op; });
      if( review_period_seconds )
         op.review_period_seconds = review_period_seconds;
      trx.operations = {op};
      _remote_db->get_global_properties().parameters.current_fees->set_fee( trx.operations.front() );

      return trx = sign_transaction(trx, broadcast);
   }

   signed_transaction propose_builder_transaction2(
      transaction_handle_type handle,
      string account_name_or_id,
      time_point_sec expiration = time_point::now() + fc::minutes(1),
      uint32_t review_period_seconds = 0, bool broadcast = true)
   {
      FC_ASSERT(_builder_transactions.count(handle));
      proposal_create_operation op;
      op.fee_paying_account = get_account(account_name_or_id).get_id();
      op.expiration_time = expiration;
      signed_transaction& trx = _builder_transactions[handle];
      std::transform(trx.operations.begin(), trx.operations.end(), std::back_inserter(op.proposed_ops),
                     [](const operation& op) -> op_wrapper { return op; });
      if( review_period_seconds )
         op.review_period_seconds = review_period_seconds;
      trx.operations = {op};
      _remote_db->get_global_properties().parameters.current_fees->set_fee( trx.operations.front() );

      return trx = sign_transaction(trx, broadcast);
   }

   void remove_builder_transaction(transaction_handle_type handle)
   {
      _builder_transactions.erase(handle);
   }


   signed_transaction register_account(string name,
                                       public_key_type owner,
                                       public_key_type active,
                                       string  registrar_account,
                                       string  referrer_account,
                                       uint32_t referrer_percent,
                                       bool broadcast = false)
   { try {
      FC_ASSERT( !self.is_locked() );
      FC_ASSERT( is_valid_name(name) );
      account_create_operation account_create_op;

      // #449 referrer_percent is on 0-100 scale, if user has larger
      // number it means their script is using GRAPHENE_100_PERCENT scale
      // instead of 0-100 scale.
      FC_ASSERT( referrer_percent <= 100 );
      // TODO:  process when pay_from_account is ID

      account_object registrar_account_object =
            this->get_account( registrar_account );
      FC_ASSERT( registrar_account_object.is_lifetime_member() );

      account_id_type registrar_account_id = registrar_account_object.id;

      account_object referrer_account_object =
            this->get_account( referrer_account );
      account_create_op.referrer = referrer_account_object.id;
      account_create_op.referrer_percent = uint16_t( referrer_percent * GRAPHENE_1_PERCENT );

      account_create_op.registrar = registrar_account_id;
      account_create_op.name = name;
      account_create_op.owner = authority(1, owner, 1);
      account_create_op.active = authority(1, active, 1);
      account_create_op.options.memo_key = active;

      signed_transaction tx;

      tx.operations.push_back( account_create_op );

      set_operation_fees( tx, _remote_db->get_global_properties().parameters.current_fees );

      vector<public_key_type> paying_keys = registrar_account_object.active.get_keys();

      auto dyn_props = get_dynamic_global_properties();
      tx.set_reference_block( dyn_props.head_block_id );
      tx.set_expiration( dyn_props.time + fc::seconds(30) );
      tx.validate();

      for( public_key_type& key : paying_keys )
      {
         auto it = _keys.find(key);
         if( it != _keys.end() )
         {
            fc::optional< fc::ecc::private_key > privkey = wif_to_key( it->second );
            if( !privkey.valid() )
            {
               FC_ASSERT( false, "Malformed private key in _keys" );
            }
            tx.sign( *privkey, _chain_id );
         }
      }

      if( broadcast )
         _remote_net_broadcast->broadcast_transaction( tx );
      return tx;
   } FC_CAPTURE_AND_RETHROW( (name)(owner)(active)(registrar_account)(referrer_account)(referrer_percent)(broadcast) ) }


   signed_transaction update_account_keys(string name,
                                          public_key_type old_owner,
                                          public_key_type new_owner,
                                          public_key_type old_active,
                                          public_key_type new_active,
                                          bool broadcast)
   { try {
      FC_ASSERT( !self.is_locked() );
      account_object account_obj = get_account(name);

      authority owner = account_obj.owner;
      owner.key_auths[new_owner] = owner.key_auths[old_owner];
      owner.key_auths.erase(old_owner);

      authority active = account_obj.active;
      active.key_auths[new_active] = active.key_auths[old_active];
      active.key_auths.erase(old_active);

      signed_transaction tx;
      account_update_operation op;

      op.account = account_obj.get_id();
      op.owner = owner;
      op.active = active;

      ilog("account_update_operation: ${op}", ("op", op));

      tx.operations = {op};
      set_operation_fees( tx, _remote_db->get_global_properties().parameters.current_fees );
      tx.validate();

      return sign_transaction( tx, broadcast );
   } FC_CAPTURE_AND_RETHROW( (name) ) }


   signed_transaction update_account_auth_key(string account_name,
                                              authority_type type,
                                              public_key_type key,
                                              weight_type weight,
                                              bool broadcast)
   {
      FC_ASSERT( !is_locked() );
      account_object account_obj = get_account(account_name);

      account_update_operation op;
      op.account = account_obj.get_id();

      authority new_auth;

      switch( type )
      {
         case( owner ):
            new_auth = account_obj.owner;
            break;
         case( active ):
            new_auth = account_obj.active;
            break;
      }

      if( weight == 0 ) // Remove the key
      {
         new_auth.key_auths.erase( key );
      }
      else
      {
         new_auth.add_authority( key, weight );
      }

      if( new_auth.is_impossible() )
      {
         if ( type == owner )
         {
            FC_ASSERT( false, "Owner authority change would render account irrecoverable." );
         }

         wlog( "Authority is now impossible." );
      }

      switch( type )
      {
         case( owner ):
            op.owner = new_auth;
            break;
         case( active ):
            op.active = new_auth;
            break;
      }

      signed_transaction tx;
      tx.operations.push_back(op);
      set_operation_fees( tx, _remote_db->get_global_properties().parameters.current_fees );
      tx.validate();

      return sign_transaction( tx, broadcast );
   }

   signed_transaction upgrade_account(string name, bool broadcast)
   { try {
      FC_ASSERT( !self.is_locked() );
      account_object account_obj = get_account(name);
      FC_ASSERT( !account_obj.is_lifetime_member() );

      signed_transaction tx;
      account_upgrade_operation op;
      op.account_to_upgrade = account_obj.get_id();
      op.upgrade_to_lifetime_member = true;
      tx.operations = {op};
      set_operation_fees( tx, _remote_db->get_global_properties().parameters.current_fees );
      tx.validate();

      return sign_transaction( tx, broadcast );
   } FC_CAPTURE_AND_RETHROW( (name) ) }


   // This function generates derived keys starting with index 0 and keeps incrementing
   // the index until it finds a key that isn't registered in the block chain.  To be
   // safer, it continues checking for a few more keys to make sure there wasn't a short gap
   // caused by a failed registration or the like.
   int find_first_unused_derived_key_index(const fc::ecc::private_key& parent_key)
   {
      int first_unused_index = 0;
      int number_of_consecutive_unused_keys = 0;
      for (int key_index = 0; ; ++key_index)
      {
         fc::ecc::private_key derived_private_key = derive_private_key(key_to_wif(parent_key), key_index);
         graphene::chain::public_key_type derived_public_key = derived_private_key.get_public_key();
         if( _keys.find(derived_public_key) == _keys.end() )
         {
            if (number_of_consecutive_unused_keys)
            {
               ++number_of_consecutive_unused_keys;
               if (number_of_consecutive_unused_keys > 5)
                  return first_unused_index;
            }
            else
            {
               first_unused_index = key_index;
               number_of_consecutive_unused_keys = 1;
            }
         }
         else
         {
            // key_index is used
            first_unused_index = 0;
            number_of_consecutive_unused_keys = 0;
         }
      }
   }

   signed_transaction create_account_with_private_key(fc::ecc::private_key owner_privkey,
                                                      string account_name,
                                                      string registrar_account,
                                                      string referrer_account,
                                                      bool broadcast = false,
                                                      bool save_wallet = true)
   { try {
         fc::scoped_lock<fc::mutex> lock(_resync_mutex);
         int active_key_index = find_first_unused_derived_key_index(owner_privkey);
         fc::ecc::private_key active_privkey = derive_private_key( key_to_wif(owner_privkey), active_key_index);

         int memo_key_index = find_first_unused_derived_key_index(active_privkey);
         fc::ecc::private_key memo_privkey = derive_private_key( key_to_wif(active_privkey), memo_key_index);

         graphene::chain::public_key_type owner_pubkey = owner_privkey.get_public_key();
         graphene::chain::public_key_type active_pubkey = active_privkey.get_public_key();
         graphene::chain::public_key_type memo_pubkey = memo_privkey.get_public_key();

         account_create_operation account_create_op;

         // TODO:  process when pay_from_account is ID

         account_object registrar_account_object = get_account( registrar_account );

         account_id_type registrar_account_id = registrar_account_object.id;

         account_object referrer_account_object = get_account( referrer_account );
         account_create_op.referrer = referrer_account_object.id;
         account_create_op.referrer_percent = referrer_account_object.referrer_rewards_percentage;

         account_create_op.registrar = registrar_account_id;
         account_create_op.name = account_name;
         account_create_op.owner = authority(1, owner_pubkey, 1);
         account_create_op.active = authority(1, active_pubkey, 1);
         account_create_op.options.memo_key = memo_pubkey;

         // current_fee_schedule()
         // find_account(pay_from_account)

         // account_create_op.fee = account_create_op.calculate_fee(db.current_fee_schedule());

         signed_transaction tx;

         tx.operations.push_back( account_create_op );

         set_operation_fees( tx, _remote_db->get_global_properties().parameters.current_fees);

         vector<public_key_type> paying_keys = registrar_account_object.active.get_keys();

         auto dyn_props = get_dynamic_global_properties();
         tx.set_reference_block( dyn_props.head_block_id );
         tx.set_expiration( dyn_props.time + fc::seconds(30) );
         tx.validate();

         for( public_key_type& key : paying_keys )
         {
            auto it = _keys.find(key);
            if( it != _keys.end() )
            {
               fc::optional< fc::ecc::private_key > privkey = wif_to_key( it->second );
               FC_ASSERT( privkey.valid(), "Malformed private key in _keys" );
               tx.sign( *privkey, _chain_id );
            }
         }

         // we do not insert owner_privkey here because
         //    it is intended to only be used for key recovery
         _wallet.pending_account_registrations[account_name].push_back(key_to_wif( active_privkey ));
         _wallet.pending_account_registrations[account_name].push_back(key_to_wif( memo_privkey ));
         if( save_wallet )
            save_wallet_file();
         if( broadcast )
            _remote_net_broadcast->broadcast_transaction( tx );
         return tx;
   } FC_CAPTURE_AND_RETHROW( (account_name)(registrar_account)(referrer_account)(broadcast) ) }

   signed_transaction create_account_with_brain_key(string brain_key,
                                                    string account_name,
                                                    string registrar_account,
                                                    string referrer_account,
                                                    bool broadcast = false,
                                                    bool save_wallet = true)
   { try {
      FC_ASSERT( !self.is_locked() );
      string normalized_brain_key = normalize_brain_key( brain_key );
      // TODO:  scan blockchain for accounts that exist with same brain key
      fc::ecc::private_key owner_privkey = derive_private_key( normalized_brain_key, 0 );
      return create_account_with_private_key(owner_privkey, account_name, registrar_account, referrer_account, broadcast, save_wallet);
   } FC_CAPTURE_AND_RETHROW( (account_name)(registrar_account)(referrer_account) ) }


   signed_transaction create_asset(string issuer,
                                   string symbol,
                                   uint8_t precision,
                                   asset_options common,
                                   fc::optional<bitasset_options> bitasset_opts,
                                   bool broadcast = false)
   { try {
      account_object issuer_account = get_account( issuer );
      FC_ASSERT(!find_asset(symbol).valid(), "Asset with that symbol already exists!");

      asset_create_operation create_op;
      create_op.issuer = issuer_account.id;
      create_op.symbol = symbol;
      create_op.precision = precision;
      create_op.common_options = common;
      create_op.bitasset_opts = bitasset_opts;

      signed_transaction tx;
      tx.operations.push_back( create_op );
      set_operation_fees( tx, _remote_db->get_global_properties().parameters.current_fees);
      tx.validate();

      return sign_transaction( tx, broadcast );
   } FC_CAPTURE_AND_RETHROW( (issuer)(symbol)(precision)(common)(bitasset_opts)(broadcast) ) }


   signed_transaction create_lottery(string issuer,
      string symbol,
      asset_options common,
      lottery_asset_options lottery_opts,
      bool broadcast = false)
   { try {
      account_object issuer_account = get_account( issuer );
      FC_ASSERT(!find_asset(symbol).valid(), "Asset with that symbol already exists!");

      lottery_asset_create_operation create_op;
      create_op.issuer = issuer_account.id;
      create_op.symbol = symbol;
      create_op.precision = 0;
      create_op.common_options = common;

      create_op.extensions = lottery_opts;

      signed_transaction tx;
      tx.operations.push_back( create_op );
      set_operation_fees( tx, _remote_db->get_global_properties().parameters.current_fees);
      tx.validate();

      return sign_transaction( tx, broadcast );
   } FC_CAPTURE_AND_RETHROW( (issuer)(symbol)(common)(broadcast) ) }

   signed_transaction buy_ticket( asset_id_type lottery, account_id_type buyer, uint64_t tickets_to_buy )
   { try {
      auto asset_obj = get_asset( lottery );
      FC_ASSERT( asset_obj.is_lottery() );

      ticket_purchase_operation top;
      top.lottery = lottery;
      top.buyer = buyer;
      top.tickets_to_buy = tickets_to_buy;
      top.amount = asset( asset_obj.lottery_options->ticket_price.amount * tickets_to_buy, asset_obj.lottery_options->ticket_price.asset_id );

      signed_transaction tx;
      tx.operations.push_back( top );
      set_operation_fees( tx, _remote_db->get_global_properties().parameters.current_fees);
      tx.validate();

      return sign_transaction( tx, true );
   } FC_CAPTURE_AND_RETHROW( (lottery)(tickets_to_buy) ) }


   signed_transaction update_asset(string symbol,
                                   optional<string> new_issuer,
                                   asset_options new_options,
                                   bool broadcast /* = false */)
   { try {
      optional<asset_object> asset_to_update = find_asset(symbol);
      if (!asset_to_update)
        FC_THROW("No asset with that symbol exists!");
      optional<account_id_type> new_issuer_account_id;
      if (new_issuer)
      {
        account_object new_issuer_account = get_account(*new_issuer);
        new_issuer_account_id = new_issuer_account.id;
      }

      asset_update_operation update_op;
      update_op.issuer = asset_to_update->issuer;
      update_op.asset_to_update = asset_to_update->id;
      update_op.new_issuer = new_issuer_account_id;
      update_op.new_options = new_options;

      signed_transaction tx;
      tx.operations.push_back( update_op );
      set_operation_fees( tx, _remote_db->get_global_properties().parameters.current_fees);
      tx.validate();

      return sign_transaction( tx, broadcast );
   } FC_CAPTURE_AND_RETHROW( (symbol)(new_issuer)(new_options)(broadcast) ) }

   signed_transaction update_bitasset(string symbol,
                                      bitasset_options new_options,
                                      bool broadcast /* = false */)
   { try {
      optional<asset_object> asset_to_update = find_asset(symbol);
      if (!asset_to_update)
        FC_THROW("No asset with that symbol exists!");

      asset_update_bitasset_operation update_op;
      update_op.issuer = asset_to_update->issuer;
      update_op.asset_to_update = asset_to_update->id;
      update_op.new_options = new_options;

      signed_transaction tx;
      tx.operations.push_back( update_op );
      set_operation_fees( tx, _remote_db->get_global_properties().parameters.current_fees);
      tx.validate();

      return sign_transaction( tx, broadcast );
   } FC_CAPTURE_AND_RETHROW( (symbol)(new_options)(broadcast) ) }

   signed_transaction update_dividend_asset(string symbol,
                                            dividend_asset_options new_options,
                                            bool broadcast /* = false */)
   { try {
      optional<asset_object> asset_to_update = find_asset(symbol);
      if (!asset_to_update)
        FC_THROW("No asset with that symbol exists!");

      asset_update_dividend_operation update_op;
      update_op.issuer = asset_to_update->issuer;
      update_op.asset_to_update = asset_to_update->id;
      update_op.new_options = new_options;

      signed_transaction tx;
      tx.operations.push_back( update_op );
      set_operation_fees( tx, _remote_db->get_global_properties().parameters.current_fees);
      tx.validate();

      return sign_transaction( tx, broadcast );
   } FC_CAPTURE_AND_RETHROW( (symbol)(new_options)(broadcast) ) }

   signed_transaction update_asset_feed_producers(string symbol,
                                                  flat_set<string> new_feed_producers,
                                                  bool broadcast /* = false */)
   { try {
      optional<asset_object> asset_to_update = find_asset(symbol);
      if (!asset_to_update)
        FC_THROW("No asset with that symbol exists!");

      asset_update_feed_producers_operation update_op;
      update_op.issuer = asset_to_update->issuer;
      update_op.asset_to_update = asset_to_update->id;
      update_op.new_feed_producers.reserve(new_feed_producers.size());
      std::transform(new_feed_producers.begin(), new_feed_producers.end(),
                     std::inserter(update_op.new_feed_producers, update_op.new_feed_producers.end()),
                     [this](const std::string& account_name_or_id){ return get_account_id(account_name_or_id); });

      signed_transaction tx;
      tx.operations.push_back( update_op );
      set_operation_fees( tx, _remote_db->get_global_properties().parameters.current_fees);
      tx.validate();

      return sign_transaction( tx, broadcast );
   } FC_CAPTURE_AND_RETHROW( (symbol)(new_feed_producers)(broadcast) ) }

   signed_transaction publish_asset_feed(string publishing_account,
                                         string symbol,
                                         price_feed feed,
                                         bool broadcast /* = false */)
   { try {
      optional<asset_object> asset_to_update = find_asset(symbol);
      if (!asset_to_update)
        FC_THROW("No asset with that symbol exists!");

      asset_publish_feed_operation publish_op;
      publish_op.publisher = get_account_id(publishing_account);
      publish_op.asset_id = asset_to_update->id;
      publish_op.feed = feed;

      signed_transaction tx;
      tx.operations.push_back( publish_op );
      set_operation_fees( tx, _remote_db->get_global_properties().parameters.current_fees);
      tx.validate();

      return sign_transaction( tx, broadcast );
   } FC_CAPTURE_AND_RETHROW( (publishing_account)(symbol)(feed)(broadcast) ) }

   signed_transaction fund_asset_fee_pool(string from,
                                          string symbol,
                                          string amount,
                                          bool broadcast /* = false */)
   { try {
      account_object from_account = get_account(from);
      optional<asset_object> asset_to_fund = find_asset(symbol);
      if (!asset_to_fund)
        FC_THROW("No asset with that symbol exists!");
      asset_object core_asset = get_asset(asset_id_type());

      asset_fund_fee_pool_operation fund_op;
      fund_op.from_account = from_account.id;
      fund_op.asset_id = asset_to_fund->id;
      fund_op.amount = core_asset.amount_from_string(amount).amount;

      signed_transaction tx;
      tx.operations.push_back( fund_op );
      set_operation_fees( tx, _remote_db->get_global_properties().parameters.current_fees);
      tx.validate();

      return sign_transaction( tx, broadcast );
   } FC_CAPTURE_AND_RETHROW( (from)(symbol)(amount)(broadcast) ) }

   signed_transaction reserve_asset(string from,
                                 string amount,
                                 string symbol,
                                 bool broadcast /* = false */)
   { try {
      account_object from_account = get_account(from);
      optional<asset_object> asset_to_reserve = find_asset(symbol);
      if (!asset_to_reserve)
        FC_THROW("No asset with that symbol exists!");

      asset_reserve_operation reserve_op;
      reserve_op.payer = from_account.id;
      reserve_op.amount_to_reserve = asset_to_reserve->amount_from_string(amount);

      signed_transaction tx;
      tx.operations.push_back( reserve_op );
      set_operation_fees( tx, _remote_db->get_global_properties().parameters.current_fees);
      tx.validate();

      return sign_transaction( tx, broadcast );
   } FC_CAPTURE_AND_RETHROW( (from)(amount)(symbol)(broadcast) ) }

   signed_transaction global_settle_asset(string symbol,
                                          price settle_price,
                                          bool broadcast /* = false */)
   { try {
      optional<asset_object> asset_to_settle = find_asset(symbol);
      if (!asset_to_settle)
        FC_THROW("No asset with that symbol exists!");

      asset_global_settle_operation settle_op;
      settle_op.issuer = asset_to_settle->issuer;
      settle_op.asset_to_settle = asset_to_settle->id;
      settle_op.settle_price = settle_price;

      signed_transaction tx;
      tx.operations.push_back( settle_op );
      set_operation_fees( tx, _remote_db->get_global_properties().parameters.current_fees);
      tx.validate();

      return sign_transaction( tx, broadcast );
   } FC_CAPTURE_AND_RETHROW( (symbol)(settle_price)(broadcast) ) }

   signed_transaction settle_asset(string account_to_settle,
                                   string amount_to_settle,
                                   string symbol,
                                   bool broadcast /* = false */)
   { try {
      optional<asset_object> asset_to_settle = find_asset(symbol);
      if (!asset_to_settle)
        FC_THROW("No asset with that symbol exists!");

      asset_settle_operation settle_op;
      settle_op.account = get_account_id(account_to_settle);
      settle_op.amount = asset_to_settle->amount_from_string(amount_to_settle);

      signed_transaction tx;
      tx.operations.push_back( settle_op );
      set_operation_fees( tx, _remote_db->get_global_properties().parameters.current_fees);
      tx.validate();

      return sign_transaction( tx, broadcast );
   } FC_CAPTURE_AND_RETHROW( (account_to_settle)(amount_to_settle)(symbol)(broadcast) ) }

   signed_transaction whitelist_account(string authorizing_account,
                                        string account_to_list,
                                        account_whitelist_operation::account_listing new_listing_status,
                                        bool broadcast /* = false */)
   { try {
      account_whitelist_operation whitelist_op;
      whitelist_op.authorizing_account = get_account_id(authorizing_account);
      whitelist_op.account_to_list = get_account_id(account_to_list);
      whitelist_op.new_listing = new_listing_status;

      signed_transaction tx;
      tx.operations.push_back( whitelist_op );
      set_operation_fees( tx, _remote_db->get_global_properties().parameters.current_fees);
      tx.validate();

      return sign_transaction( tx, broadcast );
   } FC_CAPTURE_AND_RETHROW( (authorizing_account)(account_to_list)(new_listing_status)(broadcast) ) }

   signed_transaction create_committee_member(string owner_account, string url,
                                      bool broadcast /* = false */)
   { try {

      committee_member_create_operation committee_member_create_op;
      committee_member_create_op.committee_member_account = get_account_id(owner_account);
      committee_member_create_op.url = url;
      if (_remote_db->get_committee_member_by_account(owner_account))
         FC_THROW("Account ${owner_account} is already a committee_member", ("owner_account", owner_account));

      signed_transaction tx;
      tx.operations.push_back( committee_member_create_op );
      set_operation_fees( tx, _remote_db->get_global_properties().parameters.current_fees);
      tx.validate();

      return sign_transaction( tx, broadcast );
   } FC_CAPTURE_AND_RETHROW( (owner_account)(broadcast) ) }

   witness_object get_witness(string owner_account)
   {
      try
      {
         fc::optional<witness_id_type> witness_id = maybe_id<witness_id_type>(owner_account);
         if (witness_id)
         {
            std::vector<witness_id_type> ids_to_get;
            ids_to_get.push_back(*witness_id);
            std::vector<fc::optional<witness_object>> witness_objects = _remote_db->get_witnesses(ids_to_get);
            if (witness_objects.front())
               return *witness_objects.front();
            FC_THROW("No witness is registered for id ${id}", ("id", owner_account));
         }
         else
         {
            // then maybe it's the owner account
            try
            {
               std::string owner_account_id = account_id_to_string(get_account_id(owner_account));
               fc::optional<witness_object> witness = _remote_db->get_witness_by_account(owner_account_id);
               if (witness)
                  return *witness;
               else
                  FC_THROW("No witness is registered for account ${account}", ("account", owner_account));
            }
            catch (const fc::exception&)
            {
               FC_THROW("No account or witness named ${account}", ("account", owner_account));
            }
         }
      }
      FC_CAPTURE_AND_RETHROW( (owner_account) )
   }

   son_object get_son(string owner_account)
   {
      try
      {
         fc::optional<son_id_type> son_id = maybe_id<son_id_type>(owner_account);
         if (son_id)
         {
            std::vector<son_id_type> ids_to_get;
            ids_to_get.push_back(*son_id);
            std::vector<fc::optional<son_object>> son_objects = _remote_db->get_sons(ids_to_get);
            if (son_objects.front())
               return *son_objects.front();
            FC_THROW("No SON is registered for id ${id}", ("id", owner_account));
         }
         else
         {
            // then maybe it's the owner account
            try
            {
               account_id_type owner_account_id = get_account_id(owner_account);
               fc::optional<son_object> son = _remote_db->get_son_by_account_id(owner_account_id);
               if (son)
                  return *son;
               else
                  FC_THROW("No SON is registered for account ${account}", ("account", owner_account));
            }
            catch (const fc::exception&)
            {
               FC_THROW("No account or SON named ${account}", ("account", owner_account));
            }
         }
      }
      FC_CAPTURE_AND_RETHROW( (owner_account) )
   }

   vector<worker_object> get_workers(string owner_account)
   {
      try
      {
         fc::optional<worker_id_type> worker_id = maybe_id<worker_id_type>(owner_account);
         if (worker_id)
         {
            std::vector<worker_id_type> ids_to_get;
            ids_to_get.push_back(*worker_id);
            std::vector<fc::optional<worker_object>> worker_objects = _remote_db->get_workers(ids_to_get);

            if(!worker_objects.empty()) {
               std::vector<worker_object> result;
               for (const auto &worker_object : worker_objects) {
                  if (worker_object)
                     result.emplace_back(*worker_object);
               }
               return result;
            }
            else
               FC_THROW("No workers is registered for id ${id}", ("id", owner_account));
         }
         else
         {
            // then maybe it's the owner account
            try
            {
               std::string owner_account_id = account_id_to_string(get_account_id(owner_account));
               auto workers = _remote_db->get_workers_by_account(owner_account_id);
               if (!workers.empty())
                  return workers;
               else
                  FC_THROW("No workers is registered for account ${account}", ("account", owner_account));
            }
            catch (const fc::exception&)
            {
               FC_THROW("No account or worker named ${account}", ("account", owner_account));
            }
         }
      }
      FC_CAPTURE_AND_RETHROW( (owner_account) )
   }

   bool is_witness(string owner_account)
   {
      try
      {
         fc::optional<witness_id_type> witness_id = maybe_id<witness_id_type>(owner_account);
         if (witness_id)
         {
            std::vector<witness_id_type> ids_to_get;
            ids_to_get.push_back(*witness_id);
            std::vector<fc::optional<witness_object>> witness_objects = _remote_db->get_witnesses(ids_to_get);
            if (witness_objects.front())
               return true;
            else
               return false;
         }
         else
         {
            // then maybe it's the owner account
            try
            {
               std::string owner_account_id = account_id_to_string(get_account_id(owner_account));
               fc::optional<witness_object> witness = _remote_db->get_witness_by_account(owner_account_id);
               if (witness)
                  return true;
               else
                  return false;
            }
            catch (const fc::exception&)
            {
               return false;
            }
         }
      }
      FC_CAPTURE_AND_RETHROW( (owner_account) )
   }

   committee_member_object get_committee_member(string owner_account)
   {
      try
      {
         fc::optional<committee_member_id_type> committee_member_id = maybe_id<committee_member_id_type>(owner_account);
         if (committee_member_id)
         {
            std::vector<committee_member_id_type> ids_to_get;
            ids_to_get.push_back(*committee_member_id);
            std::vector<fc::optional<committee_member_object>> committee_member_objects = _remote_db->get_committee_members(ids_to_get);
            if (committee_member_objects.front())
               return *committee_member_objects.front();
            FC_THROW("No committee_member is registered for id ${id}", ("id", owner_account));
         }
         else
         {
            // then maybe it's the owner account
            try
            {
               fc::optional<committee_member_object> committee_member = _remote_db->get_committee_member_by_account(owner_account);
               if (committee_member)
                  return *committee_member;
               else
                  FC_THROW("No committee_member is registered for account ${account}", ("account", owner_account));
            }
            catch (const fc::exception&)
            {
               FC_THROW("No account or committee_member named ${account}", ("account", owner_account));
            }
         }
      }
      FC_CAPTURE_AND_RETHROW( (owner_account) )
   }

   signed_transaction create_son(string owner_account,
                                 string url,
                                 vesting_balance_id_type deposit_id,
                                 vesting_balance_id_type pay_vb_id,
                                 flat_map<sidechain_type, string> sidechain_public_keys,
                                 bool broadcast /* = false */)
   { try {
      fc::scoped_lock<fc::mutex> lock(_resync_mutex);
      account_object son_account = get_account(owner_account);
      auto son_public_key = son_account.active.get_keys()[0];

      bool deposit_vb_ok = false;
      bool pay_vb_ok = false;
      vector<vesting_balance_object_with_info> vbs = get_vesting_balances(owner_account);
      for(const auto& vb: vbs)
      {
         if (vb.id  == deposit_id) {
            deposit_vb_ok = (vb.balance_type == vesting_balance_type::son) &&
                            (vb.get_asset_amount() >= _remote_db->get_global_properties().parameters.son_vesting_amount()) &&
                            (vb.policy.which() == vesting_policy::tag<dormant_vesting_policy>::value);
         }
         if (vb.id  == pay_vb_id) {
            pay_vb_ok = (vb.balance_type == vesting_balance_type::normal) &&
                        (vb.policy.which() == vesting_policy::tag<linear_vesting_policy>::value);
         }
      }

      if (deposit_vb_ok == false) {
          FC_THROW("Deposit vesting balance ${deposit_id} must be of SON type, with minimum amount of ${son_vesting_amount}",
                  ("deposit_id", deposit_id) ("son_vesting_amount", _remote_db->get_global_properties().parameters.son_vesting_amount()));
      }
      if (pay_vb_ok == false) {
          FC_THROW("Payment vesting balance ${pay_vb_id} must be of NORMAL type",
                  ("pay_vb_id", pay_vb_id));
      }

      son_create_operation son_create_op;
      son_create_op.owner_account = son_account.id;
      son_create_op.signing_key = son_public_key;
      son_create_op.url = url;
      son_create_op.deposit = deposit_id;
      son_create_op.pay_vb = pay_vb_id;
      son_create_op.sidechain_public_keys = sidechain_public_keys;

      if (_remote_db->get_son_by_account_id(son_create_op.owner_account))
         FC_THROW("Account ${owner_account} is already a SON", ("owner_account", owner_account));

      signed_transaction tx;
      tx.operations.push_back( son_create_op );
      set_operation_fees( tx, _remote_db->get_global_properties().parameters.current_fees);
      tx.validate();

      return sign_transaction( tx, broadcast );
   } FC_CAPTURE_AND_RETHROW( (owner_account)(broadcast) ) }

   signed_transaction update_son(string owner_account,
                                 string url,
                                 string block_signing_key,
                                 flat_map<sidechain_type, string> sidechain_public_keys,
                                 bool broadcast /* = false */)
   { try {
      son_object son = get_son(owner_account);

      son_update_operation son_update_op;
      son_update_op.son_id = son.id;
      son_update_op.owner_account = son.son_account;
      if( url != "" )
         son_update_op.new_url = url;
      if( block_signing_key != "" ) {
         son_update_op.new_signing_key = public_key_type( block_signing_key );
      }
      if( !sidechain_public_keys.empty() ) {
         son_update_op.new_sidechain_public_keys = sidechain_public_keys;
      }

      signed_transaction tx;
      tx.operations.push_back( son_update_op );
      set_operation_fees( tx, _remote_db->get_global_properties().parameters.current_fees );
      tx.validate();

      return sign_transaction( tx, broadcast );
   } FC_CAPTURE_AND_RETHROW( (owner_account)(url)(block_signing_key)(broadcast) ) }

   signed_transaction activate_deregistered_son(const string & owner_account, 
                                                bool broadcast /* = false */) 
   { try {
      son_object son = get_son(owner_account);

      son_update_operation son_update_op;
      son_update_op.son_id = son.id;
      son_update_op.owner_account = son.son_account;
      signed_transaction tx;
      tx.operations.push_back( son_update_op );
      set_operation_fees( tx, _remote_db->get_global_properties().parameters.current_fees );
      tx.validate();

      return sign_transaction( tx, broadcast );
   } FC_CAPTURE_AND_RETHROW( (owner_account)(broadcast) ) }

   signed_transaction update_son_vesting_balances(string owner_account,
                                                  optional<vesting_balance_id_type> new_deposit,
                                                  optional<vesting_balance_id_type> new_pay_vb,
                                                  bool broadcast /* = false */)
   { try {
      son_object son = get_son(owner_account);

      son_update_operation son_update_op;
      son_update_op.son_id = son.id;
      son_update_op.owner_account = son.son_account;
      if (new_deposit.valid())
         son_update_op.new_deposit = new_deposit;
      if (new_pay_vb.valid())
         son_update_op.new_pay_vb = new_pay_vb;
      signed_transaction tx;
      tx.operations.push_back( son_update_op );
      set_operation_fees( tx, _remote_db->get_global_properties().parameters.current_fees );
      tx.validate();

      return sign_transaction( tx, broadcast );
   } FC_CAPTURE_AND_RETHROW( (owner_account)(new_deposit)(new_pay_vb)(broadcast) ) }

   signed_transaction request_son_maintenance(string owner_account,
                                           bool broadcast)
   { try {
         son_object son = get_son(owner_account);

         son_maintenance_operation op;
         op.owner_account = son.son_account;
         op.son_id = son.id;
         op.request_type = son_maintenance_request_type::request_maintenance;

         signed_transaction tx;
         tx.operations.push_back( op );
         set_operation_fees( tx, _remote_db->get_global_properties().parameters.current_fees );
         tx.validate();

         return sign_transaction( tx, broadcast );
   } FC_CAPTURE_AND_RETHROW( (owner_account) ) }

   signed_transaction cancel_request_son_maintenance(string owner_account,
                                           bool broadcast)
   { try {
         son_object son = get_son(owner_account);

         son_maintenance_operation op;
         op.owner_account = son.son_account;
         op.son_id = son.id;
         op.request_type = son_maintenance_request_type::cancel_request_maintenance;

         signed_transaction tx;
         tx.operations.push_back( op );
         set_operation_fees( tx, _remote_db->get_global_properties().parameters.current_fees );
         tx.validate();

         return sign_transaction( tx, broadcast );
   } FC_CAPTURE_AND_RETHROW( (owner_account) ) }

   map<string, son_id_type> list_active_sons()
   { try {
      global_property_object gpo = get_global_properties();
      vector<son_id_type> son_ids;
      son_ids.reserve(gpo.active_sons.size());
      std::transform(gpo.active_sons.begin(), gpo.active_sons.end(),
                     std::inserter(son_ids, son_ids.end()),
                     [](const son_info& swi) {
         return swi.son_id;
      });
      std::vector<fc::optional<son_object>> son_objects = _remote_db->get_sons(son_ids);
      vector<std::string> owners;
      for(auto obj: son_objects)
      {
         std::string acc_id = account_id_to_string(obj->son_account);
         owners.push_back(acc_id);
      }
      vector< optional< account_object> > accs = _remote_db->get_accounts(owners);
      std::remove_if(son_objects.begin(), son_objects.end(),
                     [](const fc::optional<son_object>& obj) -> bool { return obj.valid(); });
      map<string, son_id_type> result;
      std::transform(accs.begin(), accs.end(), son_objects.begin(),
                     std::inserter(result, result.end()),
                     [](fc::optional<account_object>& acct, fc::optional<son_object> son) {
                        FC_ASSERT(acct, "Invalid active SONs list in global properties.");
                        return std::make_pair<string, son_id_type>(string(acct->name), std::move(son->id));
                     });
      return result;
   } FC_CAPTURE_AND_RETHROW() }

   map<son_id_type, string> get_son_network_status()
   { try {
      global_property_object gpo = get_global_properties();
      vector<son_id_type> son_ids;
      son_ids.reserve(gpo.active_sons.size());
      std::transform(gpo.active_sons.begin(), gpo.active_sons.end(),
                     std::inserter(son_ids, son_ids.end()),
                     [](const son_info& swi) {
         return swi.son_id;
      });

      map<son_id_type, string> result;
      std::vector<fc::optional<son_object>> son_objects = _remote_db->get_sons(son_ids);
      for(auto son_obj: son_objects) {
         string status;
         if (son_obj) {
            son_statistics_object sso = get_object(son_obj->statistics);
            if (sso.last_active_timestamp + fc::seconds(gpo.parameters.son_heartbeat_frequency()) > time_point::now()) {
               status = "OK, regular SON heartbeat";
            } else {
               if (sso.last_active_timestamp + fc::seconds(gpo.parameters.son_down_time()) > time_point::now()) {
                  status = "OK, irregular SON heartbeat, but not triggering SON down proposal";
               } else {
                  status = "NOT OK, irregular SON heartbeat, triggering SON down proposal";
               }
            }
         } else {
            status = "NOT OK, invalid SON id";
         }
         result[son_obj->id] = status;
      }
      return result;
   } FC_CAPTURE_AND_RETHROW() }

   optional<son_wallet_object> get_active_son_wallet()
   { try {
       return _remote_db->get_active_son_wallet();
   } FC_CAPTURE_AND_RETHROW() }

   optional<son_wallet_object> get_son_wallet_by_time_point(time_point_sec time_point)
   { try {
       return _remote_db->get_son_wallet_by_time_point(time_point);
   } FC_CAPTURE_AND_RETHROW() }

   vector<optional<son_wallet_object>> get_son_wallets(uint32_t limit)
   { try {
      return _remote_db->get_son_wallets(limit);
   } FC_CAPTURE_AND_RETHROW() }

   signed_transaction add_sidechain_address(string account,
                                            sidechain_type sidechain,
                                            string deposit_public_key,
                                            string deposit_address,
                                            string withdraw_public_key,
                                            string withdraw_address,
                                            bool broadcast /* = false */)
   { try {
      account_id_type sidechain_address_account_id = get_account_id(account);

      sidechain_address_add_operation op;
      op.payer = sidechain_address_account_id;
      op.sidechain_address_account = sidechain_address_account_id;
      op.sidechain = sidechain;
      op.deposit_public_key = deposit_public_key;
      op.deposit_address = deposit_address;
      op.withdraw_public_key = withdraw_public_key;
      op.withdraw_address = withdraw_address;

      signed_transaction tx;
      tx.operations.push_back( op );
      set_operation_fees( tx, _remote_db->get_global_properties().parameters.current_fees);
      tx.validate();

      return sign_transaction( tx, broadcast );
   } FC_CAPTURE_AND_RETHROW() }

   signed_transaction delete_sidechain_address(string account,
                                               sidechain_type sidechain,
                                               bool broadcast /* = false */)
   { try {
      account_id_type sidechain_address_account_id = get_account_id(account);
      fc::optional<sidechain_address_object> sao = _remote_db->get_sidechain_address_by_account_and_sidechain(sidechain_address_account_id, sidechain);
      if (!sao)
         FC_THROW("No sidechain address for account ${account} and sidechain ${sidechain}", ("account", sidechain_address_account_id)("sidechain", sidechain));

      sidechain_address_delete_operation op;
      op.payer = sidechain_address_account_id;
      op.sidechain_address_id = sao->id;
      op.sidechain_address_account = sidechain_address_account_id;
      op.sidechain = sidechain;

      signed_transaction tx;
      tx.operations.push_back( op );
      set_operation_fees( tx, _remote_db->get_global_properties().parameters.current_fees);
      tx.validate();

      return sign_transaction( tx, broadcast );

   } FC_CAPTURE_AND_RETHROW() }

   signed_transaction sidechain_withdrawal_transaction(const string &son_name_or_id,
		                                      uint32_t block_num,
                                                      const sidechain_type& sidechain,
                                                      const std::string &peerplays_uid,
                                                      const std::string &peerplays_transaction_id,
                                                      const chain::account_id_type &peerplays_from,
                                                      const sidechain_type& withdraw_sidechain,
                                                      const std::string &withdraw_address,
                                                      const std::string &withdraw_currency,
                                                      const string &withdraw_amount)
    { try{
      const global_property_object& gpo = get_global_properties();
      const auto dynamic_props = get_dynamic_global_properties();
      const auto son_obj = get_son(son_name_or_id);

      fc::optional<asset_object> peerplays_asset = get_asset(withdraw_currency);
      FC_ASSERT(peerplays_asset, "Could not find asset matching ${asset}", ("asset", peerplays_asset));
      const auto asset_val = peerplays_asset->amount_from_string(withdraw_amount);
      const auto asset_price = peerplays_asset->options.core_exchange_rate;

      price withdraw_currency_price = {};
      if ("BTC" == withdraw_currency) {
	 fc::optional<asset_object> a  = get_asset( gpo.parameters.btc_asset());
         withdraw_currency_price = a->options.core_exchange_rate;
      } else
      if ("AUSD" == withdraw_currency) {
	 fc::optional<asset_object> a  = get_asset( gpo.parameters.hbd_asset());
         withdraw_currency_price = a->options.core_exchange_rate;
      } else
      if ("ACNY" == withdraw_currency) {
         fc::optional<asset_object> a  = get_asset( gpo.parameters.hive_asset());
         withdraw_currency_price = a->options.core_exchange_rate;
      } else {
	      FC_THROW("withdraw_currency ${withdraw_currency}", ("withdraw_currency", withdraw_currency));
      }

      //! Create transaction
      signed_transaction son_wallet_withdraw_create_transaction;
      son_wallet_withdraw_create_operation op;
      op.payer = son_obj.son_account;
      op.son_id = son_obj.id;
      op.timestamp = dynamic_props.time;
      op.block_num = block_num;
      op.sidechain = sidechain;
      op.peerplays_uid = peerplays_uid;
      op.peerplays_transaction_id = peerplays_transaction_id;
      op.peerplays_from = peerplays_from; 
      op.peerplays_asset = asset(asset_val.amount * asset_price.base.amount / asset_price.quote.amount);
      op.withdraw_sidechain = withdraw_sidechain;
      op.withdraw_address = withdraw_address;
      op.withdraw_currency = withdraw_currency;
      op.withdraw_amount = asset_val.amount;

      son_wallet_withdraw_create_transaction.operations.push_back(op);

      return sign_transaction(son_wallet_withdraw_create_transaction, true);
   } FC_CAPTURE_AND_RETHROW( (withdraw_currency) ) }

   signed_transaction create_witness(string owner_account,
                                     string url,
                                     bool broadcast /* = false */)
   { try {
      fc::scoped_lock<fc::mutex> lock(_resync_mutex);
      account_object witness_account = get_account(owner_account);
      fc::ecc::private_key active_private_key = get_private_key_for_account(witness_account);
      int witness_key_index = find_first_unused_derived_key_index(active_private_key);
      fc::ecc::private_key witness_private_key = derive_private_key(key_to_wif(active_private_key), witness_key_index);
      graphene::chain::public_key_type witness_public_key = witness_private_key.get_public_key();

      witness_create_operation witness_create_op;
      witness_create_op.witness_account = witness_account.id;
      witness_create_op.block_signing_key = witness_public_key;
      witness_create_op.url = url;

      secret_hash_type::encoder enc;
      fc::raw::pack(enc, witness_private_key);
      fc::raw::pack(enc, secret_hash_type());
      witness_create_op.initial_secret = enc.result();


      if (_remote_db->get_witness_by_account(account_id_to_string(witness_create_op.witness_account)))
         FC_THROW("Account ${owner_account} is already a witness", ("owner_account", owner_account));

      signed_transaction tx;
      tx.operations.push_back( witness_create_op );
      set_operation_fees( tx, _remote_db->get_global_properties().parameters.current_fees);
      tx.validate();

      _wallet.pending_witness_registrations[owner_account] = key_to_wif(witness_private_key);

      return sign_transaction( tx, broadcast );
   } FC_CAPTURE_AND_RETHROW( (owner_account)(broadcast) ) }

   signed_transaction update_witness(string witness_name,
                                     string url,
                                     string block_signing_key,
                                     bool broadcast /* = false */)
   { try {
      witness_object witness = get_witness(witness_name);
      account_object witness_account = get_account( witness.witness_account );

      witness_update_operation witness_update_op;
      witness_update_op.witness = witness.id;
      witness_update_op.witness_account = witness_account.id;
      if( url != "" )
         witness_update_op.new_url = url;
      if( block_signing_key != "" ) {
         witness_update_op.new_signing_key = public_key_type( block_signing_key );
         witness_update_op.new_initial_secret = secret_hash_type::hash(secret_hash_type());
      }

      signed_transaction tx;
      tx.operations.push_back( witness_update_op );
      set_operation_fees( tx, _remote_db->get_global_properties().parameters.current_fees );
      tx.validate();

      return sign_transaction( tx, broadcast );
   } FC_CAPTURE_AND_RETHROW( (witness_name)(url)(block_signing_key)(broadcast) ) }

   template<typename WorkerInit>
   static WorkerInit _create_worker_initializer( const variant& worker_settings )
   {
      WorkerInit result;
      from_variant( worker_settings, result, GRAPHENE_MAX_NESTED_OBJECTS );
      return result;
   }

   signed_transaction create_worker(
      string owner_account,
      time_point_sec work_begin_date,
      time_point_sec work_end_date,
      share_type daily_pay,
      string name,
      string url,
      variant worker_settings,
      bool broadcast
      )
   {
      worker_initializer init;
      std::string wtype = worker_settings["type"].get_string();

      // TODO:  Use introspection to do this dispatch
      if( wtype == "burn" )
         init = _create_worker_initializer< burn_worker_initializer >( worker_settings );
      else if( wtype == "refund" )
         init = _create_worker_initializer< refund_worker_initializer >( worker_settings );
      else if( wtype == "vesting" )
         init = _create_worker_initializer< vesting_balance_worker_initializer >( worker_settings );
      else
      {
         FC_ASSERT( false, "unknown worker[\"type\"] value" );
      }

      worker_create_operation op;
      op.owner = get_account( owner_account ).id;
      op.work_begin_date = work_begin_date;
      op.work_end_date = work_end_date;
      op.daily_pay = daily_pay;
      op.name = name;
      op.url = url;
      op.initializer = init;

      signed_transaction tx;
      tx.operations.push_back( op );
      set_operation_fees( tx, _remote_db->get_global_properties().parameters.current_fees );
      tx.validate();

      return sign_transaction( tx, broadcast );
   }

   signed_transaction update_worker_votes(
      string account,
      worker_vote_delta delta,
      bool broadcast
      )
   {
      account_object acct = get_account( account );

      // you could probably use a faster algorithm for this, but flat_set is fast enough :)
      flat_set< worker_id_type > merged;
      merged.reserve( delta.vote_for.size() + delta.vote_against.size() + delta.vote_abstain.size() );
      for( const worker_id_type& wid : delta.vote_for )
      {
         bool inserted = merged.insert( wid ).second;
         FC_ASSERT( inserted, "worker ${wid} specified multiple times", ("wid", wid) );
      }
      for( const worker_id_type& wid : delta.vote_against )
      {
         bool inserted = merged.insert( wid ).second;
         FC_ASSERT( inserted, "worker ${wid} specified multiple times", ("wid", wid) );
      }
      for( const worker_id_type& wid : delta.vote_abstain )
      {
         bool inserted = merged.insert( wid ).second;
         FC_ASSERT( inserted, "worker ${wid} specified multiple times", ("wid", wid) );
      }

      // should be enforced by FC_ASSERT's above
      assert( merged.size() == delta.vote_for.size() + delta.vote_against.size() + delta.vote_abstain.size() );

      vector< object_id_type > query_ids;
      for( const worker_id_type& wid : merged )
         query_ids.push_back( wid );

      flat_set<vote_id_type> new_votes( acct.options.votes );

      fc::variants objects = _remote_db->get_objects( query_ids );
      for( const variant& obj : objects )
      {
         worker_object wo;
         from_variant( obj, wo, GRAPHENE_MAX_NESTED_OBJECTS );
         new_votes.erase( wo.vote_for );
         new_votes.erase( wo.vote_against );
         if( delta.vote_for.find( wo.id ) != delta.vote_for.end() )
            new_votes.insert( wo.vote_for );
         else if( delta.vote_against.find( wo.id ) != delta.vote_against.end() )
            new_votes.insert( wo.vote_against );
         else
            assert( delta.vote_abstain.find( wo.id ) != delta.vote_abstain.end() );
      }

      account_update_operation update_op;
      update_op.account = acct.id;
      update_op.new_options = acct.options;
      update_op.new_options->votes = new_votes;

      signed_transaction tx;
      tx.operations.push_back( update_op );
      set_operation_fees( tx, _remote_db->get_global_properties().parameters.current_fees );
      tx.validate();

      return sign_transaction( tx, broadcast );
   }

   signed_transaction create_vesting_balance(string owner_account,
                                     string amount,
                                     string asset_symbol,
                                     vesting_balance_type vesting_type,
                                     bool broadcast /* = false */)
   { try {
      FC_ASSERT( !is_locked() );
      account_object user_account = get_account(owner_account);
      fc::optional<asset_object> asset_obj = get_asset(asset_symbol);
      FC_ASSERT(asset_obj, "Invalid asset symbol {asst}", ("asst", asset_symbol));

      vesting_balance_create_operation op;
      op.creator = user_account.get_id();
      op.owner = user_account.get_id();
      op.amount = asset_obj->amount_from_string(amount);
      op.balance_type = vesting_type;
      if (op.balance_type == vesting_balance_type::son)
          op.policy = dormant_vesting_policy_initializer {};

      signed_transaction tx;
      tx.operations.push_back( op );
      set_operation_fees( tx, _remote_db->get_global_properties().parameters.current_fees);
      tx.validate();

      return sign_transaction( tx, broadcast );
   } FC_CAPTURE_AND_RETHROW( (owner_account)(broadcast) ) }

   vector< vesting_balance_object_with_info > get_vesting_balances( string account_name )
   { try {
      fc::optional<vesting_balance_id_type> vbid = maybe_id<vesting_balance_id_type>( account_name );
      std::vector<vesting_balance_object_with_info> result;
      fc::time_point_sec now = _remote_db->get_dynamic_global_properties().time;

      if( vbid )
      {
         result.emplace_back( get_object<vesting_balance_object>(*vbid), now );
         return result;
      }

      vector< vesting_balance_object > vbos = _remote_db->get_vesting_balances( account_name );
      if( vbos.size() == 0 )
         return result;

      for( const vesting_balance_object& vbo : vbos )
         result.emplace_back( vbo, now );

      return result;
   } FC_CAPTURE_AND_RETHROW( (account_name) )
   }

   signed_transaction withdraw_vesting(
      string witness_name,
      string amount,
      string asset_symbol,
      bool broadcast = false )
   { try {
      asset_object asset_obj = get_asset( asset_symbol );
      fc::optional<vesting_balance_id_type> vbid = maybe_id<vesting_balance_id_type>(witness_name);
      if( !vbid )
      {
         if (is_witness(witness_name))
         {
            witness_object wit = get_witness( witness_name );
            FC_ASSERT( wit.pay_vb, "Account ${account} has no core Token ${TOKEN} vested and thus its not allowed to withdraw.", ("account", witness_name)("TOKEN", GRAPHENE_SYMBOL));
            vbid = wit.pay_vb;
         }
         else
            FC_THROW("Account ${account} has no core Token ${TOKEN} vested and thus its not allowed to withdraw.", ("account", witness_name)("TOKEN", GRAPHENE_SYMBOL));
      }

      vesting_balance_object vbo = get_object< vesting_balance_object >( *vbid );

      if(vbo.balance_type == vesting_balance_type::gpos)
         FC_THROW("Allowed to withdraw only Normal and Son type vest balances with this method");

      vesting_balance_withdraw_operation vesting_balance_withdraw_op;

      vesting_balance_withdraw_op.vesting_balance = *vbid;
      vesting_balance_withdraw_op.owner = vbo.owner;
      vesting_balance_withdraw_op.amount = asset_obj.amount_from_string(amount);

      signed_transaction tx;
      tx.operations.push_back( vesting_balance_withdraw_op );
      set_operation_fees( tx, _remote_db->get_global_properties().parameters.current_fees );
      tx.validate();

      return sign_transaction( tx, broadcast );
   } FC_CAPTURE_AND_RETHROW( (witness_name)(amount) )
   }

   signed_transaction withdraw_GPOS_vesting_balance(
      string account_name,
      string amount,
      string asset_symbol,
      bool broadcast = false)
   { try {

      //Can be deleted after GPOS hardfork time
      time_point_sec now = time_point::now();
      if(now < HARDFORK_GPOS_TIME)
         FC_THROW("GPOS related functionality is not avaiable until next Spring");

      asset_object asset_obj = get_asset( asset_symbol );
      vector< vesting_balance_object > vbos;
      vesting_balance_object vbo;
      fc::optional<vesting_balance_id_type> vbid = maybe_id<vesting_balance_id_type>(account_name);
      if( !vbid )
      {
         vbos = _remote_db->get_vesting_balances( account_name );
         if( vbos.size() == 0 )
            FC_THROW("Account ${account} has no core TOKEN vested and thus its not allowed to withdraw.", ("account", account_name));
      }

      //whether it is a witness or user, keep it in a container and iterate over to process all vesting balances and types
      if(!vbos.size())
         vbos.emplace_back( get_object<vesting_balance_object>(*vbid) );

      for (const vesting_balance_object& vesting_balance_obj: vbos) {
         if(vesting_balance_obj.balance_type == vesting_balance_type::gpos)
         {
            vbo = vesting_balance_obj;
            break;
         }
      }

      vesting_balance_withdraw_operation vesting_balance_withdraw_op;

      vesting_balance_withdraw_op.vesting_balance = vbo.id;
      vesting_balance_withdraw_op.owner = vbo.owner;
      vesting_balance_withdraw_op.amount = asset_obj.amount_from_string(amount);

      signed_transaction tx;
      tx.operations.push_back( vesting_balance_withdraw_op );
      set_operation_fees( tx, _remote_db->get_global_properties().parameters.current_fees );
      tx.validate();

      return sign_transaction( tx, broadcast );
   } FC_CAPTURE_AND_RETHROW( (account_name)(amount) )
   }

   signed_transaction vote_for_committee_member(string voting_account,
                                        string committee_member,
                                        bool approve,
                                        bool broadcast /* = false */)
   { try {
      std::vector<vesting_balance_object_with_info> vbo_info = get_vesting_balances(voting_account);

      time_point_sec now = time_point::now();
      if(now >= HARDFORK_GPOS_TIME)  //can be removed after GPOS HARDFORK time pass
      {
         std::vector<vesting_balance_object_with_info>::iterator vbo_iter;
         vbo_iter = std::find_if(vbo_info.begin(), vbo_info.end(), [](vesting_balance_object_with_info const& obj){return obj.balance_type == vesting_balance_type::gpos;});
         if( vbo_info.size() == 0 ||  vbo_iter == vbo_info.end())
            FC_THROW("Account ${account} has no core Token ${TOKEN} vested and will not be allowed to vote for the committee member", ("account", voting_account)("TOKEN", GRAPHENE_SYMBOL));
      }

      account_object voting_account_object = get_account(voting_account);
      fc::optional<committee_member_object> committee_member_obj = _remote_db->get_committee_member_by_account(committee_member);
      if (!committee_member_obj)
         FC_THROW("Account ${committee_member} is not registered as a committee_member", ("committee_member", committee_member));

      bool update_vote_time = false;

      if (approve)
      {
         auto insert_result = voting_account_object.options.votes.insert(committee_member_obj->vote_id);
         if(now >= HARDFORK_GPOS_TIME)  //can be removed after GPOS HARDFORK time pass
         {
            account_id_type stake_account = get_account_id(voting_account);
            const auto gpos_info = _remote_db->get_gpos_info(stake_account);
            const auto vesting_subperiod = _remote_db->get_global_properties().parameters.gpos_subperiod();
            const auto gpos_start_time = fc::time_point_sec(_remote_db->get_global_properties().parameters.gpos_period_start());
            const auto subperiod_start_time = gpos_start_time.sec_since_epoch() + (gpos_info.current_subperiod - 1) * vesting_subperiod;

            if (!insert_result.second && (gpos_info.last_voted_time.sec_since_epoch() >= subperiod_start_time))
               FC_THROW("Account ${account} was already voting for committee_member ${committee_member} in the current GPOS sub-period", ("account", voting_account)("committee_member", committee_member));
            else
               update_vote_time = true;   //Allow user to vote in each sub-period(Update voting time, which is reference in calculating VF)
         }
         else
         {
            if (!insert_result.second)
               FC_THROW("Account ${account} was already voting for committee_member ${committee_member}", ("account", voting_account)("committee_member", committee_member));
         }
      }
      else
      {
         unsigned votes_removed = voting_account_object.options.votes.erase(committee_member_obj->vote_id);
         if (!votes_removed)
            FC_THROW("Account ${account} is already not voting for committee_member ${committee_member}", ("account", voting_account)("committee_member", committee_member));
      }
      account_update_operation account_update_op;
      account_update_op.account = voting_account_object.id;
      account_update_op.new_options = voting_account_object.options;
      account_update_op.extensions.value.update_last_voting_time = update_vote_time;

      signed_transaction tx;
      tx.operations.push_back( account_update_op );
      set_operation_fees( tx, _remote_db->get_global_properties().parameters.current_fees);
      tx.validate();

      return sign_transaction( tx, broadcast );
   } FC_CAPTURE_AND_RETHROW( (voting_account)(committee_member)(approve)(broadcast) ) }

   signed_transaction vote_for_son(string voting_account,
                                        string son,
                                        bool approve,
                                        bool broadcast /* = false */)
   { try {

      std::vector<vesting_balance_object_with_info> vbo_info = get_vesting_balances(voting_account);
      std::vector<vesting_balance_object_with_info>::iterator vbo_iter;
      vbo_iter = std::find_if(vbo_info.begin(), vbo_info.end(), [](vesting_balance_object_with_info const& obj){return obj.balance_type == vesting_balance_type::gpos;});
      if( vbo_info.size() == 0 ||  vbo_iter == vbo_info.end())
         FC_THROW("Account ${account} has no core Token ${TOKEN} vested and will not be allowed to vote for the SON account", ("account", voting_account)("TOKEN", GRAPHENE_SYMBOL));

      account_object voting_account_object = get_account(voting_account);
      account_id_type son_account_id = get_account_id(son);
      fc::optional<son_object> son_obj = _remote_db->get_son_by_account_id(son_account_id);
      if (!son_obj)
         FC_THROW("Account ${son} is not registered as a son", ("son", son));
      if (approve)
      {
         auto insert_result = voting_account_object.options.votes.insert(son_obj->vote_id);
         if (!insert_result.second)
            FC_THROW("Account ${account} was already voting for son ${son}", ("account", voting_account)("son", son));
      }
      else
      {
         unsigned votes_removed = voting_account_object.options.votes.erase(son_obj->vote_id);
         if (!votes_removed)
            FC_THROW("Account ${account} is already not voting for son ${son}", ("account", voting_account)("son", son));
      }
      account_update_operation account_update_op;
      account_update_op.account = voting_account_object.id;
      account_update_op.new_options = voting_account_object.options;

      signed_transaction tx;
      tx.operations.push_back( account_update_op );
      set_operation_fees( tx, _remote_db->get_global_properties().parameters.current_fees);
      tx.validate();

      return sign_transaction( tx, broadcast );
   } FC_CAPTURE_AND_RETHROW( (voting_account)(son)(approve)(broadcast) ) }

   signed_transaction update_son_votes(string voting_account,
                                           std::vector<std::string> sons_to_approve,
                                           std::vector<std::string> sons_to_reject,
                                           uint16_t desired_number_of_sons,
                                           bool broadcast /* = false */)
   { try {
      FC_ASSERT(sons_to_approve.size() || sons_to_reject.size(), "Both accepted and rejected lists can't be empty simultaneously");
      std::vector<vesting_balance_object_with_info> vbo_info = get_vesting_balances(voting_account);
      std::vector<vesting_balance_object_with_info>::iterator vbo_iter;
      vbo_iter = std::find_if(vbo_info.begin(), vbo_info.end(), [](vesting_balance_object_with_info const& obj){return obj.balance_type == vesting_balance_type::gpos;});
      if( vbo_info.size() == 0 ||  vbo_iter == vbo_info.end())
         FC_THROW("Account ${account} has no core Token ${TOKEN} vested and will not be allowed to vote for the SON account", ("account", voting_account)("TOKEN", GRAPHENE_SYMBOL));

      account_object voting_account_object = get_account(voting_account);
      for (const std::string& son : sons_to_approve)
      {
         account_id_type son_owner_account_id = get_account_id(son);
         fc::optional<son_object> son_obj = _remote_db->get_son_by_account_id(son_owner_account_id);
         if (!son_obj)
            FC_THROW("Account ${son} is not registered as a SON", ("son", son));
         auto insert_result = voting_account_object.options.votes.insert(son_obj->vote_id);
         if (!insert_result.second)
            FC_THROW("Account ${account} was already voting for SON ${son}", ("account", voting_account)("son", son));
      }
      for (const std::string& son : sons_to_reject)
      {
         account_id_type son_owner_account_id = get_account_id(son);
         fc::optional<son_object> son_obj = _remote_db->get_son_by_account_id(son_owner_account_id);
         if (!son_obj)
            FC_THROW("Account ${son} is not registered as a SON", ("son", son));
         unsigned votes_removed = voting_account_object.options.votes.erase(son_obj->vote_id);
         if (!votes_removed)
            FC_THROW("Account ${account} is already not voting for SON ${son}", ("account", voting_account)("son", son));
      }
      voting_account_object.options.num_son = desired_number_of_sons;

      account_update_operation account_update_op;
      account_update_op.account = voting_account_object.id;
      account_update_op.new_options = voting_account_object.options;

      signed_transaction tx;
      tx.operations.push_back( account_update_op );
      set_operation_fees( tx, _remote_db->get_global_properties().parameters.current_fees);
      tx.validate();

      return sign_transaction( tx, broadcast );
   } FC_CAPTURE_AND_RETHROW( (voting_account)(sons_to_approve)(sons_to_reject)(desired_number_of_sons)(broadcast) ) }

   signed_transaction sidechain_deposit_transaction(  const string &son_name_or_id,
                                                      const sidechain_type& sidechain,
                                                      const string &transaction_id,
                                                      uint32_t operation_index,
                                                      const string &sidechain_from,
                                                      const string &sidechain_to,
                                                      const string &sidechain_currency,
                                                      int64_t sidechain_amount,
                                                      const string &peerplays_from_name_or_id,
                                                      const string &peerplays_to_name_or_id )
   {
      //! Get data we need to procced transaction
      const auto dynamic_global_props = get_dynamic_global_properties();
      const auto global_props = get_global_properties();
      const auto son_obj = get_son(son_name_or_id);
      const std::string sidechain_str = [&sidechain](){
         switch (sidechain) {
            case sidechain_type::peerplays : return "peerplays";
            case sidechain_type::bitcoin : return "bitcoin";
            case sidechain_type::hive : return "acny";
            default:
               FC_THROW("Wrong sidechain type: ${sidechain}", ("sidechain", sidechain));
         }
      }();
      const auto peerplays_from_obj = get_account(peerplays_from_name_or_id);
      const auto peerplays_to_obj = get_account(peerplays_to_name_or_id);
      const price sidechain_currency_price = [this, &sidechain_currency, &global_props](){
         if(sidechain_currency == "BTC")
         {
            fc::optional<asset_object> asset_obj = get_asset(object_id_to_string(global_props.parameters.btc_asset()));
            FC_ASSERT(asset_obj, "Could not find asset matching ${asset}", ("asset", "BTC"));
            return asset_obj->options.core_exchange_rate;
         }
         else if(sidechain_currency == "AUSD")
         {
            fc::optional<asset_object> asset_obj = get_asset(object_id_to_string(global_props.parameters.hbd_asset()));
            FC_ASSERT(asset_obj, "Could not find asset matching ${asset}", ("asset", "AUSD"));
            return asset_obj->options.core_exchange_rate;
         }
         else if(sidechain_currency == "ACNY")
         {
            fc::optional<asset_object> asset_obj = get_asset(object_id_to_string(global_props.parameters.hive_asset()));
            FC_ASSERT(asset_obj, "Could not find asset matching ${asset}", ("asset", "ACNY"));
            return asset_obj->options.core_exchange_rate;
         }
         else
         {
            fc::optional<asset_object> asset_obj = get_asset(sidechain_currency);
            FC_ASSERT(asset_obj, "Could not find asset matching ${asset}", ("asset", sidechain_currency));
            return asset_obj->options.core_exchange_rate;
         }
      }();

      //! Create transaction
      signed_transaction son_wallet_deposit_create_transaction;
      son_wallet_deposit_create_operation op;
      op.payer = son_obj.son_account;
      op.son_id = son_obj.id;
      op.timestamp = dynamic_global_props.time;
      op.block_num = dynamic_global_props.head_block_number;
      op.sidechain = sidechain;
      op.sidechain_uid = sidechain_str + "-" + transaction_id + "-" + std::to_string(operation_index);
      op.sidechain_transaction_id = transaction_id;
      op.sidechain_from = sidechain_from;
      op.sidechain_to = sidechain_to;
      op.sidechain_currency = sidechain_currency;
      op.sidechain_amount = sidechain_amount;
      op.peerplays_from = peerplays_from_obj.id;
      op.peerplays_to = peerplays_to_obj.id;
      op.peerplays_asset = asset(op.sidechain_amount * sidechain_currency_price.base.amount / sidechain_currency_price.quote.amount);
      son_wallet_deposit_create_transaction.operations.push_back(op);

      return sign_transaction(son_wallet_deposit_create_transaction, true);
   }

   signed_transaction vote_for_witness(string voting_account,
                                       string witness,
                                       bool approve,
                                       bool broadcast /* = false */)
   { try {
      std::vector<vesting_balance_object_with_info> vbo_info = get_vesting_balances(voting_account);

      time_point_sec now = time_point::now();
      if(now >= HARDFORK_GPOS_TIME)  //can be removed after GPOS HARDFORK time pass
      {
         std::vector<vesting_balance_object_with_info>::iterator vbo_iter;
         vbo_iter = std::find_if(vbo_info.begin(), vbo_info.end(), [](vesting_balance_object_with_info const& obj){return obj.balance_type == vesting_balance_type::gpos;});
         if( vbo_info.size() == 0 ||  vbo_iter == vbo_info.end())
            FC_THROW("Account ${account} has no core Token ${TOKEN} vested and will not be allowed to vote for the witness", ("account", voting_account)("TOKEN", GRAPHENE_SYMBOL));
      }

      account_object voting_account_object = get_account(voting_account);

      fc::optional<witness_object> witness_obj = _remote_db->get_witness_by_account(witness);
      if (!witness_obj)
         FC_THROW("Account ${witness} is not registered as a witness", ("witness", witness));

      bool update_vote_time = false;
      if (approve)
      {
         auto insert_result = voting_account_object.options.votes.insert(witness_obj->vote_id);
         if(now >= HARDFORK_GPOS_TIME)  //can be removed after GPOS HARDFORK time pass
         {
            account_id_type stake_account = get_account_id(voting_account);
            const auto gpos_info = _remote_db->get_gpos_info(stake_account);
            const auto vesting_subperiod = _remote_db->get_global_properties().parameters.gpos_subperiod();
            const auto gpos_start_time = fc::time_point_sec(_remote_db->get_global_properties().parameters.gpos_period_start());
            const auto subperiod_start_time = gpos_start_time.sec_since_epoch() + (gpos_info.current_subperiod - 1) * vesting_subperiod;

            if (!insert_result.second && (gpos_info.last_voted_time.sec_since_epoch() >= subperiod_start_time))
               FC_THROW("Account ${account} was already voting for witness ${witness} in the current GPOS sub-period", ("account", voting_account)("witness", witness));
            else
               update_vote_time = true;   //Allow user to vote in each sub-period(Update voting time, which is reference in calculating VF)
         }
         else
         {
            if (!insert_result.second)
               FC_THROW("Account ${account} was already voting for witness ${witness}", ("account", voting_account)("witness", witness));
         }
      }
      else
      {
         unsigned votes_removed = voting_account_object.options.votes.erase(witness_obj->vote_id);
         if (!votes_removed)
            FC_THROW("Account ${account} has not voted for witness ${witness}", ("account", voting_account)("witness", witness));
      }

      account_update_operation account_update_op;
      account_update_op.account = voting_account_object.id;
      account_update_op.new_options = voting_account_object.options;
      account_update_op.extensions.value.update_last_voting_time = update_vote_time;

      signed_transaction tx;
      tx.operations.push_back( account_update_op );
      set_operation_fees( tx, _remote_db->get_global_properties().parameters.current_fees);
      tx.validate();

      return sign_transaction( tx, broadcast );
   } FC_CAPTURE_AND_RETHROW( (voting_account)(witness)(approve)(broadcast) ) }

   signed_transaction update_witness_votes(string voting_account,
                                           std::vector<std::string> witnesses_to_approve,
                                           std::vector<std::string> witnesses_to_reject,
                                           uint16_t desired_number_of_witnesses,
                                           bool broadcast /* = false */)
   { try {
      account_object voting_account_object = get_account(voting_account);
      for (const std::string& witness : witnesses_to_approve)
      {
         fc::optional<witness_object> witness_obj = _remote_db->get_witness_by_account(witness);
         if (!witness_obj)
            FC_THROW("Account ${witness} is not registered as a witness", ("witness", witness));
         auto insert_result = voting_account_object.options.votes.insert(witness_obj->vote_id);
         if (!insert_result.second)
            FC_THROW("Account ${account} was already voting for witness ${witness}", ("account", voting_account)("witness", witness));
      }
      for (const std::string& witness : witnesses_to_reject)
      {
         fc::optional<witness_object> witness_obj = _remote_db->get_witness_by_account(witness);
         if (!witness_obj)
            FC_THROW("Account ${witness} is not registered as a witness", ("witness", witness));
         unsigned votes_removed = voting_account_object.options.votes.erase(witness_obj->vote_id);
         if (!votes_removed)
            FC_THROW("Account ${account} is already not voting for witness ${witness}", ("account", voting_account)("witness", witness));
      }
      voting_account_object.options.num_witness = desired_number_of_witnesses;

      account_update_operation account_update_op;
      account_update_op.account = voting_account_object.id;
      account_update_op.new_options = voting_account_object.options;

      signed_transaction tx;
      tx.operations.push_back( account_update_op );
      set_operation_fees( tx, _remote_db->get_global_properties().parameters.current_fees);
      tx.validate();

      return sign_transaction( tx, broadcast );
   } FC_CAPTURE_AND_RETHROW( (voting_account)(witnesses_to_approve)(witnesses_to_reject)(desired_number_of_witnesses)(broadcast) ) }

   signed_transaction set_voting_proxy(string account_to_modify,
                                       optional<string> voting_account,
                                       bool broadcast /* = false */)
   { try {
      account_object account_object_to_modify = get_account(account_to_modify);
      if (voting_account)
      {
         account_id_type new_voting_account_id = get_account_id(*voting_account);
         if (account_object_to_modify.options.voting_account == new_voting_account_id)
            FC_THROW("Voting proxy for ${account} is already set to ${voter}", ("account", account_to_modify)("voter", *voting_account));
         account_object_to_modify.options.voting_account = new_voting_account_id;
      }
      else
      {
         if (account_object_to_modify.options.voting_account == GRAPHENE_PROXY_TO_SELF_ACCOUNT)
            FC_THROW("Account ${account} is already voting for itself", ("account", account_to_modify));
         account_object_to_modify.options.voting_account = GRAPHENE_PROXY_TO_SELF_ACCOUNT;
      }

      account_update_operation account_update_op;
      account_update_op.account = account_object_to_modify.id;
      account_update_op.new_options = account_object_to_modify.options;

      signed_transaction tx;
      tx.operations.push_back( account_update_op );
      set_operation_fees( tx, _remote_db->get_global_properties().parameters.current_fees);
      tx.validate();

      return sign_transaction( tx, broadcast );
   } FC_CAPTURE_AND_RETHROW( (account_to_modify)(voting_account)(broadcast) ) }

   signed_transaction set_desired_witness_and_committee_member_count(string account_to_modify,
                                                             uint16_t desired_number_of_witnesses,
                                                             uint16_t desired_number_of_committee_members,
                                                             bool broadcast /* = false */)
   { try {
      account_object account_object_to_modify = get_account(account_to_modify);

      if (account_object_to_modify.options.num_witness == desired_number_of_witnesses &&
          account_object_to_modify.options.num_committee == desired_number_of_committee_members)
         FC_THROW("Account ${account} is already voting for ${witnesses} witnesses and ${committee_members} committee_members",
                  ("account", account_to_modify)("witnesses", desired_number_of_witnesses)("committee_members",desired_number_of_witnesses));
      account_object_to_modify.options.num_witness = desired_number_of_witnesses;
      account_object_to_modify.options.num_committee = desired_number_of_committee_members;

      account_update_operation account_update_op;
      account_update_op.account = account_object_to_modify.id;
      account_update_op.new_options = account_object_to_modify.options;

      signed_transaction tx;
      tx.operations.push_back( account_update_op );
      set_operation_fees( tx, _remote_db->get_global_properties().parameters.current_fees);
      tx.validate();

      return sign_transaction( tx, broadcast );
   } FC_CAPTURE_AND_RETHROW( (account_to_modify)(desired_number_of_witnesses)(desired_number_of_committee_members)(broadcast) ) }

   signed_transaction sign_transaction(signed_transaction tx, bool broadcast = false)
   {
      set<public_key_type> pks = _remote_db->get_potential_signatures(tx);
      flat_set<public_key_type> owned_keys;
      owned_keys.reserve(pks.size());
      std::copy_if(pks.begin(), pks.end(), std::inserter(owned_keys, owned_keys.end()),
                   [this](const public_key_type &pk) { return _keys.find(pk) != _keys.end(); });
      tx.clear_signatures();
      set<public_key_type> approving_key_set = _remote_db->get_required_signatures(tx, owned_keys);

      auto dyn_props = get_dynamic_global_properties();
      tx.set_reference_block(dyn_props.head_block_id);

      // first, some bookkeeping, expire old items from _recently_generated_transactions
      // since transactions include the head block id, we just need the index for keeping transactions unique
      // when there are multiple transactions in the same block.  choose a time period that should be at
      // least one block long, even in the worst case.  2 minutes ought to be plenty.
      fc::time_point_sec oldest_transaction_ids_to_track(dyn_props.time - fc::minutes(2));
      auto oldest_transaction_record_iter = _recently_generated_transactions.get<timestamp_index>().lower_bound(oldest_transaction_ids_to_track);
      auto begin_iter = _recently_generated_transactions.get<timestamp_index>().begin();
      _recently_generated_transactions.get<timestamp_index>().erase(begin_iter, oldest_transaction_record_iter);

      uint32_t expiration_time_offset = 0;
      for (;;)
      {
         tx.set_expiration( dyn_props.time + fc::seconds(30 + expiration_time_offset) );
         tx.clear_signatures();

         for (const public_key_type &key : approving_key_set)
            tx.sign(get_private_key(key), _chain_id);

         graphene::chain::transaction_id_type this_transaction_id = tx.id();
         auto iter = _recently_generated_transactions.find(this_transaction_id);
         if (iter == _recently_generated_transactions.end())
         {
            // we haven't generated this transaction before, the usual case
            recently_generated_transaction_record this_transaction_record;
            this_transaction_record.generation_time = dyn_props.time;
            this_transaction_record.transaction_id = this_transaction_id;
            _recently_generated_transactions.insert(this_transaction_record);
            break;
         }

         // else we've generated a dupe, increment expiration time and re-sign it
         ++expiration_time_offset;
      }

      if( broadcast )
      {
         try
         {
            _remote_net_broadcast->broadcast_transaction(tx);
         }
         catch (const fc::exception& e)
         {
            elog("Caught exception while broadcasting tx ${id}:  ${e}", ("id", tx.id().str())("e", e.to_detail_string()));
            throw;
         }
      }

      return tx;
   }

   flat_set<public_key_type> get_transaction_signers(const signed_transaction &tx) const
   {
      return tx.get_signature_keys(_chain_id);
   }

   vector<vector<account_id_type>> get_key_references(const vector<public_key_type> &keys) const
   {
       return _remote_db->get_key_references(keys);
   }

   /**
    * Get the required public keys to sign the transaction which had been
    * owned by us
    *
    * NOTE, if `erase_existing_sigs` set to true, the original trasaction's
    * signatures will be erased
    *
    * @param tx           The transaction to be signed
    * @param erase_existing_sigs
    *        The transaction could have been partially signed already,
    *        if set to false, the corresponding public key of existing
    *        signatures won't be returned.
    *        If set to true, the existing signatures will be erased and
    *        all required keys returned.
   */
   set<public_key_type> get_owned_required_keys( signed_transaction &tx,
                                                    bool erase_existing_sigs = true)
   {
      set<public_key_type> pks = _remote_db->get_potential_signatures( tx );
      flat_set<public_key_type> owned_keys;
      owned_keys.reserve( pks.size() );
      std::copy_if( pks.begin(), pks.end(),
                    std::inserter( owned_keys, owned_keys.end() ),
                    [this]( const public_key_type &pk ) {
                       return _keys.find( pk ) != _keys.end();
                    } );

      if ( erase_existing_sigs )
         tx.signatures.clear();

      return _remote_db->get_required_signatures( tx, owned_keys );
   }

   signed_transaction add_transaction_signature( signed_transaction tx,
                                                 bool broadcast )
   {
      set<public_key_type> approving_key_set = get_owned_required_keys(tx, false);

      if ( ( ( tx.ref_block_num == 0 && tx.ref_block_prefix == 0 ) ||
             tx.expiration == fc::time_point_sec() ) &&
           tx.signatures.empty() )
      {
         auto dyn_props = get_dynamic_global_properties();
         auto parameters = get_global_properties().parameters;
         fc::time_point_sec now = dyn_props.time;
         tx.set_reference_block( dyn_props.head_block_id );
         tx.set_expiration( now + parameters.maximum_time_until_expiration );
      }
      for ( const public_key_type &key : approving_key_set )
         tx.sign( get_private_key( key ), _chain_id );

      if ( broadcast )
      {
         try
         {
            _remote_net_broadcast->broadcast_transaction( tx );
         }
         catch ( const fc::exception &e )
         {
            elog( "Caught exception while broadcasting tx ${id}:  ${e}",
                  ( "id", tx.id().str() )( "e", e.to_detail_string() ) );
            FC_THROW( "Caught exception while broadcasting tx" );
         }
      }

      return tx;
   }

   signed_transaction sell_asset(string seller_account,
                                 string amount_to_sell,
                                 string symbol_to_sell,
                                 string min_to_receive,
                                 string symbol_to_receive,
                                 uint32_t timeout_sec = 0,
                                 bool   fill_or_kill = false,
                                 bool   broadcast = false)
   {
      account_object seller   = get_account( seller_account );

      limit_order_create_operation op;
      op.seller = seller.id;
      op.amount_to_sell = get_asset(symbol_to_sell).amount_from_string(amount_to_sell);
      op.min_to_receive = get_asset(symbol_to_receive).amount_from_string(min_to_receive);
      if( timeout_sec )
         op.expiration = fc::time_point::now() + fc::seconds(timeout_sec);
      op.fill_or_kill = fill_or_kill;

      signed_transaction tx;
      tx.operations.push_back(op);
      set_operation_fees( tx, _remote_db->get_global_properties().parameters.current_fees);
      tx.validate();

      return sign_transaction( tx, broadcast );
   }

   signed_transaction borrow_asset(string seller_name, string amount_to_borrow, string asset_symbol,
                                       string amount_of_collateral, bool broadcast = false)
   {
      account_object seller = get_account(seller_name);
      asset_object mia = get_asset(asset_symbol);
      FC_ASSERT(mia.is_market_issued());
      asset_object collateral = get_asset(get_object(*mia.bitasset_data_id).options.short_backing_asset);

      call_order_update_operation op;
      op.funding_account = seller.id;
      op.delta_debt   = mia.amount_from_string(amount_to_borrow);
      op.delta_collateral = collateral.amount_from_string(amount_of_collateral);

      signed_transaction trx;
      trx.operations = {op};
      set_operation_fees( trx, _remote_db->get_global_properties().parameters.current_fees);
      trx.validate();

      return sign_transaction(trx, broadcast);
   }

   signed_transaction cancel_order(object_id_type order_id, bool broadcast = false)
   { try {
         FC_ASSERT(!is_locked());
         FC_ASSERT(order_id.space() == protocol_ids, "Invalid order ID ${id}", ("id", order_id));
         signed_transaction trx;

         limit_order_cancel_operation op;
         op.fee_paying_account = get_object<limit_order_object>(order_id).seller;
         op.order = order_id;
         trx.operations = {op};
         set_operation_fees( trx, _remote_db->get_global_properties().parameters.current_fees);

         trx.validate();
         return sign_transaction(trx, broadcast);
   } FC_CAPTURE_AND_RETHROW((order_id)) }

   signed_transaction transfer(string from, string to, string amount,
                               string asset_symbol, string memo, bool broadcast = false)
   { try {
      FC_ASSERT( !self.is_locked() );
      fc::optional<asset_object> asset_obj = get_asset(asset_symbol);
      FC_ASSERT(asset_obj, "Could not find asset matching ${asset}", ("asset", asset_symbol));

      account_object from_account = get_account(from);
      account_object to_account = get_account(to);
      account_id_type from_id = from_account.id;
      account_id_type to_id = get_account_id(to);

      transfer_operation xfer_op;

      xfer_op.from = from_id;
      xfer_op.to = to_id;
      xfer_op.amount = asset_obj->amount_from_string(amount);

      if( memo.size() )
         {
            xfer_op.memo = memo_data();
            xfer_op.memo->from = from_account.options.memo_key;
            xfer_op.memo->to = to_account.options.memo_key;
            xfer_op.memo->set_message(get_private_key(from_account.options.memo_key),
                                      to_account.options.memo_key, memo);
         }

      signed_transaction tx;
      tx.operations.push_back(xfer_op);
      set_operation_fees( tx, _remote_db->get_global_properties().parameters.current_fees);
      tx.validate();

      return sign_transaction(tx, broadcast);
   } FC_CAPTURE_AND_RETHROW( (from)(to)(amount)(asset_symbol)(memo)(broadcast) ) }

   signed_transaction issue_asset(string to_account, string amount, string symbol,
                                  string memo, bool broadcast = false)
   {
      auto asset_obj = get_asset(symbol);

      account_object to = get_account(to_account);
      account_object issuer = get_account(asset_obj.issuer);

      asset_issue_operation issue_op;
      issue_op.issuer           = asset_obj.issuer;
      issue_op.asset_to_issue   = asset_obj.amount_from_string(amount);
      issue_op.issue_to_account = to.id;

      if( memo.size() )
      {
         issue_op.memo = memo_data();
         issue_op.memo->from = issuer.options.memo_key;
         issue_op.memo->to = to.options.memo_key;
         issue_op.memo->set_message(get_private_key(issuer.options.memo_key),
                                    to.options.memo_key, memo);
      }

      signed_transaction tx;
      tx.operations.push_back(issue_op);
      set_operation_fees(tx,_remote_db->get_global_properties().parameters.current_fees);
      tx.validate();

      return sign_transaction(tx, broadcast);
   }

   std::map<string,std::function<string(fc::variant,const fc::variants&)>> get_result_formatters() const
   {
      std::map<string,std::function<string(fc::variant,const fc::variants&)> > m;
      m["help"] = [](variant result, const fc::variants& a)
      {
         return result.get_string();
      };

      m["gethelp"] = [](variant result, const fc::variants& a)
      {
         return result.get_string();
      };

      m["get_account_history"] = [this](variant result, const fc::variants& a)
      {
         auto r = result.as<vector<operation_detail>>( GRAPHENE_MAX_NESTED_OBJECTS );
         std::stringstream ss;

         for( operation_detail& d : r )
         {
            operation_history_object& i = d.op;
            auto b = _remote_db->get_block_header(i.block_num);
            FC_ASSERT(b);
            ss << b->timestamp.to_iso_string() << " ";
            i.op.visit(operation_printer(ss, *this, i.result));
            ss << " \n";
         }

         return ss.str();
      };
      m["get_relative_account_history"] = [this](variant result, const fc::variants& a)
      {
         auto r = result.as<vector<operation_detail>>( GRAPHENE_MAX_NESTED_OBJECTS );
         std::stringstream ss;

         for( operation_detail& d : r )
         {
            operation_history_object& i = d.op;
            auto b = _remote_db->get_block_header(i.block_num);
            FC_ASSERT(b);
            ss << b->timestamp.to_iso_string() << " ";
            i.op.visit(operation_printer(ss, *this, i.result));
            ss << " \n";
         }

         return ss.str();
      };

      m["list_account_balances"] = [this](variant result, const fc::variants& a)
      {
         auto r = result.as<vector<asset>>( GRAPHENE_MAX_NESTED_OBJECTS );
         vector<asset_object> asset_recs;
         std::transform(r.begin(), r.end(), std::back_inserter(asset_recs), [this](const asset& a) {
            return get_asset(a.asset_id);
         });

         std::stringstream ss;
         for( unsigned i = 0; i < asset_recs.size(); ++i )
            ss << asset_recs[i].amount_to_pretty_string(r[i]) << "\n";

         return ss.str();
      };

      m["list_core_accounts"] = [this](variant result, const fc::variants& a)
      {
         std::stringstream ss;

         auto balances = result.as<vector<account_balance_object>>( GRAPHENE_MAX_NESTED_OBJECTS );
         for (const account_balance_object& balance: balances)
         {
             const account_object& account = get_account(balance.owner);
             //ss << account.name << " " <<  std::string(balance.id) << " " << balance.balance.value << "\n";
             ss << account.name << " " <<  std::string(balance.id) << " " << get_asset(balance.asset_type).amount_to_pretty_string(balance.balance) << "\n";
         }

         return ss.str();
      };

      m["get_blind_balances"] = [this](variant result, const fc::variants& a)
      {
         auto r = result.as<vector<asset>>( GRAPHENE_MAX_NESTED_OBJECTS );
         vector<asset_object> asset_recs;
         std::transform(r.begin(), r.end(), std::back_inserter(asset_recs), [this](const asset& a) {
            return get_asset(a.asset_id);
         });

         std::stringstream ss;
         for( unsigned i = 0; i < asset_recs.size(); ++i )
            ss << asset_recs[i].amount_to_pretty_string(r[i]) << "\n";

         return ss.str();
      };
      m["transfer_to_blind"] = [this](variant result, const fc::variants& a)
      {
         auto r = result.as<blind_confirmation>( GRAPHENE_MAX_NESTED_OBJECTS );
         std::stringstream ss;
         r.trx.operations[0].visit( operation_printer( ss, *this, operation_result() ) );
         ss << "\n";
         for( const auto& out : r.outputs )
         {
            asset_object a = get_asset( out.decrypted_memo.amount.asset_id );
            ss << a.amount_to_pretty_string( out.decrypted_memo.amount ) << " to  " << out.label << "\n\t  receipt: " << out.confirmation_receipt <<"\n\n";
         }
         return ss.str();
      };
      m["blind_transfer"] = [this](variant result, const fc::variants& a)
      {
         auto r = result.as<blind_confirmation>( GRAPHENE_MAX_NESTED_OBJECTS );
         std::stringstream ss;
         r.trx.operations[0].visit( operation_printer( ss, *this, operation_result() ) );
         ss << "\n";
         for( const auto& out : r.outputs )
         {
            asset_object a = get_asset( out.decrypted_memo.amount.asset_id );
            ss << a.amount_to_pretty_string( out.decrypted_memo.amount ) << " to  " << out.label << "\n\t  receipt: " << out.confirmation_receipt <<"\n\n";
         }
         return ss.str();
      };
      m["receive_blind_transfer"] = [this](variant result, const fc::variants& a)
      {
         auto r = result.as<blind_receipt>( GRAPHENE_MAX_NESTED_OBJECTS );
         std::stringstream ss;
         asset_object as = get_asset( r.amount.asset_id );
         ss << as.amount_to_pretty_string( r.amount ) << "  " << r.from_label << "  =>  " << r.to_label  << "  " << r.memo <<"\n";
         return ss.str();
      };
      m["blind_history"] = [this](variant result, const fc::variants& a)
      {
         auto records = result.as<vector<blind_receipt>>( GRAPHENE_MAX_NESTED_OBJECTS );
         std::stringstream ss;
         ss << "WHEN         "
            << "  " << "AMOUNT"  << "  " << "FROM" << "  =>  " << "TO" << "  " << "MEMO" <<"\n";
         ss << "====================================================================================\n";
         for( auto& r : records )
         {
            asset_object as = get_asset( r.amount.asset_id );
            ss << fc::get_approximate_relative_time_string( r.date )
               << "  " << as.amount_to_pretty_string( r.amount ) << "  " << r.from_label << "  =>  " << r.to_label  << "  " << r.memo <<"\n";
         }
         return ss.str();
      };
      m["get_upcoming_tournaments"] = m["get_tournaments"] = m["get_tournaments_by_state"] = [this](variant result, const fc::variants& a)
      {
         const vector<tournament_object> tournaments = result.as<vector<tournament_object> >( GRAPHENE_MAX_NESTED_OBJECTS );
         std::stringstream ss;
         ss << "ID       GAME                  BUY IN    PLAYERS\n";
         ss << "====================================================================================\n";
         for( const tournament_object& tournament_obj : tournaments )
         {
            asset_object buy_in_asset = get_asset(tournament_obj.options.buy_in.asset_id);
            ss << fc::variant(tournament_obj.id, 1).as<std::string>( 1 ) << "  "
               << buy_in_asset.amount_to_pretty_string(tournament_obj.options.buy_in.amount) << "  "
               << tournament_obj.options.number_of_players << " players\n";
            switch (tournament_obj.get_state())
            {
            case tournament_state::accepting_registrations:
               {
                  ss << "  Waiting for players, " << tournament_obj.registered_players << " of " << tournament_obj.options.number_of_players << " have registered\n";
                  ss << "  If enough players register, the game will start ";
                  if (tournament_obj.options.start_time)
                     ss << "at " << tournament_obj.options.start_time->to_iso_string() << "\n";
                  else
                     ss << *tournament_obj.options.start_delay << " seconds after the last player registers\n";
                  break;
               }
            case tournament_state::awaiting_start:
               {
                  ss << "  All players have registered, tournament will start at " << tournament_obj.start_time->to_iso_string() << "\n";
                  break;
               }
            case tournament_state::in_progress:
               {
                  ss << "  Tournament started at " << tournament_obj.start_time->to_iso_string() << "\n";
                  break;
               }
            case tournament_state::registration_period_expired:
               {
                  ss << "  Tournament was canceled at " << tournament_obj.options.registration_deadline.to_iso_string() << ", not enough players registered\n";
                  break;
               }
            case tournament_state::concluded:
               {
                  ss << "  Tournament finished at " << tournament_obj.end_time->to_iso_string() << "\n";
                  break;
               }
            }
         }
         return ss.str();
      };
      m["get_tournament"] = [this](variant result, const fc::variants& a)
      {
         std::stringstream ss;

         tournament_object tournament = result.as<tournament_object>( GRAPHENE_MAX_NESTED_OBJECTS );
         tournament_details_object tournament_details = _remote_db->get_objects({result["tournament_details_id"].as<object_id_type>( 5 )})[0].as<tournament_details_object>( 5 );
         tournament_state state = tournament.get_state();
         if (state == tournament_state::accepting_registrations)
         {
            ss << "Tournament is accepting registrations\n";
            ss << "Players " <<  tournament.registered_players << "/" << tournament.options.number_of_players  << ":\n";
            for (const account_id_type& player : tournament_details.registered_players)
               ss << "\t" << get_account(player).name << "\n";
         }
         else if (state == tournament_state::registration_period_expired)
         {
            ss << "Tournament registration period expired\n";
            ss << "Players " <<  tournament.registered_players << "/" << tournament.options.number_of_players  << ":\n";
            for (const account_id_type& player : tournament_details.registered_players)
               ss << "\t" << get_account(player).name << "\n";
         }
         else if (state == tournament_state::awaiting_start)
         {
            ss << "Tournament starts at " << tournament.start_time->to_iso_string() << "\n";
            ss << "Players:\n";
            for (const account_id_type& player : tournament_details.registered_players)
               ss << "\t" << get_account(player).name << "\n";
         }
         else if (state == tournament_state::in_progress ||
                  state == tournament_state::concluded)
         {
            unsigned num_matches = tournament_details.matches.size();
            uint32_t num_rounds = boost::multiprecision::detail::find_msb(tournament_details.matches.size() + 1);
            unsigned num_rows = (num_matches + 1) * 2 - 1;
            for (unsigned row = 0; row < num_rows; ++row)
            {
               for (unsigned round = 0; round <= num_rounds; ++round)
               {
                  unsigned row_offset = (1 << round) - 1;
                  unsigned row_vertical_spacing = 1 << (round + 1);
                  if (row >= row_offset &&
                      (row - row_offset) % row_vertical_spacing == 0)
                  {
                     unsigned player_number_in_round = (row - row_offset) / row_vertical_spacing;
                     unsigned first_player_in_round = (num_matches - (num_matches >> round)) * 2;
                     unsigned player_number = first_player_in_round + player_number_in_round;

                     unsigned match_number = player_number / 2;
                     unsigned player_in_match = player_number % 2;

                     std::string player_name;
                     if (round == num_rounds)
                     {
                        match_object match = get_object<match_object>(tournament_details.matches[num_matches - 1]);
                        if (match.get_state() == match_state::match_complete &&
                            !match.match_winners.empty())
                        {
                           assert(match.match_winners.size() == 1);
                           player_name = get_account(*match.match_winners.begin()).name;
                        }
                     }
                     else
                     {
                        match_object match = get_object<match_object>(tournament_details.matches[match_number]);
                        if (!match.players.empty())
                        {
                           if (player_in_match < match.players.size())
                              player_name = get_account(match.players[player_in_match]).name;
                           else
                              player_name = "[bye]";
                        }
                     }

                     ss << "__";
                     ss << std::setfill('_') << std::setw(10) << player_name.substr(0,10);
                     ss << "__";
                  }
                  else
                     ss << "              ";

                  if (round != num_rounds)
                  {
                     unsigned round_horizontal_spacing = 1 << round;
                     unsigned next_row_vertical_spacing = 1 << (round + 2);
                     for (unsigned i = 0; i < round_horizontal_spacing; ++i)
                     {
                        if ((row - 1 - i - row_offset) % next_row_vertical_spacing == 0)
                           ss << "\\";
                        else if ((row - row_vertical_spacing + i - row_offset) % next_row_vertical_spacing == 0)
                           ss << "/";
                        else
                           ss << " ";
                     }
                  }
               }
               ss << "\n";
            }
         }

         return ss.str();
      };
      m["get_order_book"] = [this](variant result, const fc::variants& a)
      {
         auto orders = result.as<order_book>( GRAPHENE_MAX_NESTED_OBJECTS );
         auto bids = orders.bids;
         auto asks = orders.asks;
         std::stringstream ss;
         std::stringstream sum_stream;
         sum_stream << "Sum(" << orders.base << ')';
         double bid_sum = 0;
         double ask_sum = 0;
         const int spacing = 20;

         auto prettify_num = [&ss]( double n )
         {
            if (abs( round( n ) - n ) < 0.00000000001 )
            {
               ss << (int) n;
            }
            else if (n - floor(n) < 0.000001)
            {
               ss << setiosflags( ios::fixed ) << setprecision(10) << n;
            }
            else
            {
               ss << setiosflags( ios::fixed ) << setprecision(6) << n;
            }
         };

         ss << setprecision( 8 ) << setiosflags( ios::fixed ) << setiosflags( ios::left );

         ss << ' ' << setw( (spacing * 4) + 6 ) << "BUY ORDERS" << "SELL ORDERS\n"
            << ' ' << setw( spacing + 1 ) << "Price" << setw( spacing ) << orders.quote << ' ' << setw( spacing )
            << orders.base << ' ' << setw( spacing ) << sum_stream.str()
            << "   " << setw( spacing + 1 ) << "Price" << setw( spacing ) << orders.quote << ' ' << setw( spacing )
            << orders.base << ' ' << setw( spacing ) << sum_stream.str()
            << "\n====================================================================================="
            << "|=====================================================================================\n";

         for (unsigned i = 0; i < bids.size() || i < asks.size() ; i++)
         {
            if ( i < bids.size() )
            {
                bid_sum += bids[i].base;
                ss << ' ' << setw( spacing );
                prettify_num( bids[i].price );
                ss << ' ' << setw( spacing );
                prettify_num( bids[i].quote );
                ss << ' ' << setw( spacing );
                prettify_num( bids[i].base );
                ss << ' ' << setw( spacing );
                prettify_num( bid_sum );
                ss << ' ';
            }
            else
            {
                ss << setw( (spacing * 4) + 5 ) << ' ';
            }

            ss << '|';

            if ( i < asks.size() )
            {
               ask_sum += asks[i].base;
               ss << ' ' << setw( spacing );
               prettify_num( asks[i].price );
               ss << ' ' << setw( spacing );
               prettify_num( asks[i].quote );
               ss << ' ' << setw( spacing );
               prettify_num( asks[i].base );
               ss << ' ' << setw( spacing );
               prettify_num( ask_sum );
            }

            ss << '\n';
         }

         ss << endl
            << "Buy Total:  " << bid_sum << ' ' << orders.base << endl
            << "Sell Total: " << ask_sum << ' ' << orders.base << endl;

         return ss.str();
      };

      return m;
   }

   signed_transaction propose_parameter_change(
      const string& proposing_account,
      fc::time_point_sec expiration_time,
      const variant_object& changed_values,
      bool broadcast = false)
   {
      FC_ASSERT( !changed_values.contains("current_fees") );

      const chain_parameters& current_params = get_global_properties().parameters;
      chain_parameters new_params = current_params;
      fc::reflector<chain_parameters>::visit(
         fc::from_variant_visitor<chain_parameters>( changed_values, new_params, GRAPHENE_MAX_NESTED_OBJECTS )
         );

      committee_member_update_global_parameters_operation update_op;
      update_op.new_parameters = new_params;

      proposal_create_operation prop_op;

      prop_op.expiration_time = expiration_time;
      prop_op.review_period_seconds = current_params.committee_proposal_review_period;
      prop_op.fee_paying_account = get_account(proposing_account).id;

      prop_op.proposed_ops.emplace_back( update_op );
      current_params.current_fees->set_fee( prop_op.proposed_ops.back().op );

      signed_transaction tx;
      tx.operations.push_back(prop_op);
      set_operation_fees(tx, current_params.current_fees);
      tx.validate();

      return sign_transaction(tx, broadcast);
   }

   signed_transaction propose_fee_change(
      const string& proposing_account,
      fc::time_point_sec expiration_time,
      const variant_object& changed_fees,
      bool broadcast = false)
   {
      const chain_parameters& current_params = get_global_properties().parameters;
      const fee_schedule_type& current_fees = *(current_params.current_fees);

      flat_map< int, fee_parameters > fee_map;
      fee_map.reserve( current_fees.parameters.size() );
      for( const fee_parameters& op_fee : current_fees.parameters )
         fee_map[ op_fee.which() ] = op_fee;
      uint32_t scale = current_fees.scale;

      for( const auto& item : changed_fees )
      {
         const string& key = item.key();
         if( key == "scale" )
         {
            int64_t _scale = item.value().as_int64();
            FC_ASSERT( _scale >= 0 );
            FC_ASSERT( _scale <= std::numeric_limits<uint32_t>::max() );
            scale = uint32_t( _scale );
            continue;
         }
         // is key a number?
         auto is_numeric = [&key]() -> bool
         {
            size_t n = key.size();
            for( size_t i=0; i<n; i++ )
            {
               if( !isdigit( key[i] ) )
                  return false;
            }
            return true;
         };

         int which;
         if( is_numeric() )
            which = std::stoi( key );
         else
         {
            const auto& n2w = _operation_which_map.name_to_which;
            auto it = n2w.find( key );
            FC_ASSERT( it != n2w.end(), "unknown operation" );
            which = it->second;
         }

         fee_parameters fp = from_which_variant< fee_parameters >( which, item.value(), GRAPHENE_MAX_NESTED_OBJECTS );
         fee_map[ which ] = fp;
      }

      fee_schedule_type new_fees;

      for( const std::pair< int, fee_parameters >& item : fee_map )
         new_fees.parameters.insert( item.second );
      new_fees.scale = scale;

      chain_parameters new_params = current_params;
      new_params.current_fees = std::make_shared<fee_schedule>(new_fees);

      committee_member_update_global_parameters_operation update_op;
      update_op.new_parameters = new_params;

      proposal_create_operation prop_op;

      prop_op.expiration_time = expiration_time;
      prop_op.review_period_seconds = current_params.committee_proposal_review_period;
      prop_op.fee_paying_account = get_account(proposing_account).id;

      prop_op.proposed_ops.emplace_back( update_op );
      current_params.current_fees->set_fee( prop_op.proposed_ops.back().op );

      signed_transaction tx;
      tx.operations.push_back(prop_op);
      set_operation_fees(tx, current_params.current_fees);
      tx.validate();

      return sign_transaction(tx, broadcast);
   }

   signed_transaction propose_dividend_asset_update(
      const string& proposing_account,
      fc::time_point_sec expiration_time,
      const variant_object& changed_values,
      bool broadcast = false)
   {
      FC_ASSERT( changed_values.contains("asset_to_update") );

      const chain_parameters& current_params = get_global_properties().parameters;
      asset_update_dividend_operation changed_op;
      fc::reflector<asset_update_dividend_operation>::visit(
         fc::from_variant_visitor<asset_update_dividend_operation>( changed_values, changed_op, GRAPHENE_MAX_NESTED_OBJECTS )
         );

      optional<asset_object> asset_to_update = find_asset(changed_op.asset_to_update);
      if (!asset_to_update)
        FC_THROW("No asset with that symbol exists!");

      asset_update_dividend_operation update_op;
      update_op.issuer = asset_to_update->issuer;
      update_op.asset_to_update = asset_to_update->id;
      update_op.new_options = changed_op.new_options;

      proposal_create_operation prop_op;

      prop_op.expiration_time = expiration_time;
      prop_op.review_period_seconds = current_params.committee_proposal_review_period;
      prop_op.fee_paying_account = get_account(proposing_account).id;

      prop_op.proposed_ops.emplace_back( update_op );
      current_params.current_fees->set_fee( prop_op.proposed_ops.back().op );

      signed_transaction tx;
      tx.operations.push_back(prop_op);
      set_operation_fees(tx, current_params.current_fees);
      tx.validate();

      return sign_transaction(tx, broadcast);
   }

   signed_transaction approve_proposal(
      const string& fee_paying_account,
      const string& proposal_id,
      const approval_delta& delta,
      bool broadcast = false)
   {
      proposal_update_operation update_op;

      update_op.fee_paying_account = get_account(fee_paying_account).id;
      update_op.proposal = fc::variant(proposal_id, 1).as<proposal_id_type>( 1 );
      // make sure the proposal exists
      get_object( update_op.proposal );

      for( const std::string& name : delta.active_approvals_to_add )
         update_op.active_approvals_to_add.insert( get_account( name ).id );
      for( const std::string& name : delta.active_approvals_to_remove )
         update_op.active_approvals_to_remove.insert( get_account( name ).id );
      for( const std::string& name : delta.owner_approvals_to_add )
         update_op.owner_approvals_to_add.insert( get_account( name ).id );
      for( const std::string& name : delta.owner_approvals_to_remove )
         update_op.owner_approvals_to_remove.insert( get_account( name ).id );
      for( const std::string& k : delta.key_approvals_to_add )
         update_op.key_approvals_to_add.insert( public_key_type( k ) );
      for( const std::string& k : delta.key_approvals_to_remove )
         update_op.key_approvals_to_remove.insert( public_key_type( k ) );

      signed_transaction tx;
      tx.operations.push_back(update_op);
      set_operation_fees(tx, get_global_properties().parameters.current_fees);
      tx.validate();
      return sign_transaction(tx, broadcast);
   }

   signed_transaction create_custom_permission(string owner,
                                               string permission_name,
                                               authority auth,
                                               bool broadcast)
   {
      custom_permission_create_operation create_op;
      create_op.owner_account = get_account(owner).id;
      create_op.permission_name = permission_name;
      create_op.auth = auth;

      signed_transaction tx;
      tx.operations.push_back(create_op);
      set_operation_fees(tx, get_global_properties().parameters.current_fees);
      tx.validate();
      return sign_transaction(tx, broadcast);
   }

   signed_transaction update_custom_permission(string owner,
                                               custom_permission_id_type permission_id,
                                               fc::optional<authority> new_auth,
                                               bool broadcast)
   {
      custom_permission_update_operation update_op;
      update_op.owner_account = get_account(owner).id;
      update_op.permission_id = permission_id;
      update_op.new_auth = new_auth;

      signed_transaction tx;
      tx.operations.push_back(update_op);
      set_operation_fees(tx, get_global_properties().parameters.current_fees);
      tx.validate();
      return sign_transaction(tx, broadcast);
   }

   signed_transaction delete_custom_permission(string owner,
                                               custom_permission_id_type permission_id,
                                               bool broadcast)
   {
      custom_permission_delete_operation delete_op;
      delete_op.owner_account = get_account(owner).id;
      delete_op.permission_id = permission_id;

      signed_transaction tx;
      tx.operations.push_back(delete_op);
      set_operation_fees(tx, get_global_properties().parameters.current_fees);
      tx.validate();
      return sign_transaction(tx, broadcast);
   }

   signed_transaction create_custom_account_authority(string owner,
                                                      custom_permission_id_type permission_id,
                                                      int operation_type,
                                                      fc::time_point_sec valid_from,
                                                      fc::time_point_sec valid_to,
                                                      bool broadcast)
   {
      custom_account_authority_create_operation create_op;
      create_op.owner_account = get_account(owner).id;
      create_op.permission_id = permission_id;
      create_op.operation_type = operation_type;
      create_op.valid_from = valid_from;
      create_op.valid_to = valid_to;

      signed_transaction tx;
      tx.operations.push_back(create_op);
      set_operation_fees(tx, get_global_properties().parameters.current_fees);
      tx.validate();
      return sign_transaction(tx, broadcast);
   }

   signed_transaction update_custom_account_authority(string owner,
                                                      custom_account_authority_id_type auth_id,
                                                      fc::optional<fc::time_point_sec> new_valid_from,
                                                      fc::optional<fc::time_point_sec> new_valid_to,
                                                      bool broadcast)
   {
      custom_account_authority_update_operation update_op;
      update_op.owner_account = get_account(owner).id;
      update_op.auth_id = auth_id;
      update_op.new_valid_from = new_valid_from;
      update_op.new_valid_to = new_valid_to;

      signed_transaction tx;
      tx.operations.push_back(update_op);
      set_operation_fees(tx, get_global_properties().parameters.current_fees);
      tx.validate();
      return sign_transaction(tx, broadcast);
   }

   signed_transaction delete_custom_account_authority(string owner,
                                                      custom_account_authority_id_type auth_id,
                                                      bool broadcast)
   {
      custom_account_authority_delete_operation delete_op;
      delete_op.owner_account = get_account(owner).id;
      delete_op.auth_id = auth_id;

      signed_transaction tx;
      tx.operations.push_back(delete_op);
      set_operation_fees(tx, get_global_properties().parameters.current_fees);
      tx.validate();
      return sign_transaction(tx, broadcast);
   }

   vector<custom_permission_object> get_custom_permissions(string owner) const
   {
      return _remote_db->get_custom_permissions(get_account(owner).id);
   }

   fc::optional<custom_permission_object> get_custom_permission_by_name(string owner, string permission_name) const
   {
      return _remote_db->get_custom_permission_by_name(get_account(owner).id, permission_name);
   }

   vector<custom_account_authority_object> get_custom_account_authorities(string owner) const
   {
      return _remote_db->get_custom_account_authorities(get_account(owner).id);
   }

   vector<custom_account_authority_object> get_custom_account_authorities_by_permission_id(custom_permission_id_type permission_id) const
   {
      return _remote_db->get_custom_account_authorities_by_permission_id(permission_id);
   }

   vector<custom_account_authority_object> get_custom_account_authorities_by_permission_name(string owner, string permission_name) const
   {
      return _remote_db->get_custom_account_authorities_by_permission_name(get_account(owner).id, permission_name);
   }

   vector<authority> get_active_custom_account_authorities_by_operation(string owner, int operation_type) const
   {
      return _remote_db->get_active_custom_account_authorities_by_operation(get_account(owner).id, operation_type);
   }

   void dbg_make_uia(string creator, string symbol)
   {
      asset_options opts;
      opts.flags &= ~(white_list | disable_force_settle | global_settle);
      opts.issuer_permissions = opts.flags;
      opts.core_exchange_rate = price(asset(1), asset(1,asset_id_type(1)));
      create_asset(get_account(creator).name, symbol, 2, opts, {}, true);
   }

   void dbg_make_mia(string creator, string symbol)
   {
      asset_options opts;
      opts.flags &= ~white_list;
      opts.issuer_permissions = opts.flags;
      opts.core_exchange_rate = price(asset(1), asset(1,asset_id_type(1)));
      bitasset_options bopts;
      create_asset(get_account(creator).name, symbol, 2, opts, bopts, true);
   }

   void dbg_push_blocks( const std::string& src_filename, uint32_t count )
   {
      use_debug_api();
      (*_remote_debug)->debug_push_blocks( src_filename, count );
      (*_remote_debug)->debug_stream_json_objects_flush();
   }

   void dbg_generate_blocks( const std::string& debug_wif_key, uint32_t count )
   {
      use_debug_api();
      (*_remote_debug)->debug_generate_blocks( debug_wif_key, count );
      (*_remote_debug)->debug_stream_json_objects_flush();
   }

   void dbg_stream_json_objects( const std::string& filename )
   {
      use_debug_api();
      (*_remote_debug)->debug_stream_json_objects( filename );
      (*_remote_debug)->debug_stream_json_objects_flush();
   }

   void dbg_update_object( const fc::variant_object& update )
   {
      use_debug_api();
      (*_remote_debug)->debug_update_object( update );
      (*_remote_debug)->debug_stream_json_objects_flush();
   }

   void use_network_node_api()
   {
      if( _remote_net_node )
         return;
      try
      {
         _remote_net_node = _remote_api->network_node();
      }
      catch( const fc::exception& e )
      {
         std::cerr << "\nCouldn't get network node API.  You probably are not configured\n"
         "to access the network API on the witness_node you are\n"
         "connecting to.  Please follow the instructions in README.md to set up an apiaccess file.\n"
         "\n";
         throw(e);
      }
   }

   void use_debug_api()
   {
      if( _remote_debug )
         return;
      try
      {
        _remote_debug = _remote_api->debug();
      }
      catch( const fc::exception& e )
      {
         std::cerr << "\nCouldn't get debug node API.  You probably are not configured\n"
         "to access the debug API on the node you are connecting to.\n"
         "\n"
         "To fix this problem:\n"
         "- Please ensure you are running debug_node, not witness_node.\n"
         "- Please follow the instructions in README.md to set up an apiaccess file.\n"
         "\n";
      }
   }

   void use_sidechain_api()
   {
      if( _remote_sidechain )
         return;
      try
      {
        _remote_sidechain = _remote_api->sidechain();
      }
      catch( const fc::exception& e )
      {
         std::cerr << "\nCouldn't get sidechain API.  You probably are not configured\n"
         "to access the sidechain API on the node you are connecting to.\n"
         "\n"
         "To fix this problem:\n"
         "- Please ensure peerplays_sidechain plugin is enabled.\n"
         "- Please follow the instructions in README.md to set up an apiaccess file.\n"
         "\n";
      }
   }

   void network_add_nodes( const vector<string>& nodes )
   {
      use_network_node_api();
      for( const string& node_address : nodes )
      {
         (*_remote_net_node)->add_node( fc::ip::endpoint::from_string( node_address ) );
      }
   }

   vector< variant > network_get_connected_peers()
   {
      use_network_node_api();
      const auto peers = (*_remote_net_node)->get_connected_peers();
      vector< variant > result;
      result.reserve( peers.size() );
      for( const auto& peer : peers )
      {
         variant v;
         fc::to_variant( peer, v, GRAPHENE_MAX_NESTED_OBJECTS );
         result.push_back( v );
      }
      return result;
   }

   void flood_network(string prefix, uint32_t number_of_transactions)
   {
      try
      {
         const account_object& master = *_wallet.my_accounts.get<by_name>().lower_bound("import");
         int number_of_accounts = number_of_transactions / 3;
         number_of_transactions -= number_of_accounts;
         try {
            dbg_make_uia(master.name, "SHILL");
         } catch(...) {/* Ignore; the asset probably already exists.*/}

         fc::time_point start = fc::time_point::now();
         for( int i = 0; i < number_of_accounts; ++i )
         {
            std::ostringstream brain_key;
            brain_key << "brain key for account " << prefix << i;
            signed_transaction trx = create_account_with_brain_key(brain_key.str(), prefix + fc::to_string(i), master.name, master.name, /* broadcast = */ true, /* save wallet = */ false);
         }
         fc::time_point end = fc::time_point::now();
         ilog("Created ${n} accounts in ${time} milliseconds",
              ("n", number_of_accounts)("time", (end - start).count() / 1000));

         start = fc::time_point::now();
         for( int i = 0; i < number_of_accounts; ++i )
         {
            signed_transaction trx = transfer(master.name, prefix + fc::to_string(i), "10", "CORE", "", true);
            trx = transfer(master.name, prefix + fc::to_string(i), "1", "CORE", "", true);
         }
         end = fc::time_point::now();
         ilog("Transferred to ${n} accounts in ${time} milliseconds",
              ("n", number_of_accounts*2)("time", (end - start).count() / 1000));

         start = fc::time_point::now();
         for( int i = 0; i < number_of_accounts; ++i )
         {
            signed_transaction trx = issue_asset(prefix + fc::to_string(i), "1000", "SHILL", "", true);
         }
         end = fc::time_point::now();
         ilog("Issued to ${n} accounts in ${time} milliseconds",
              ("n", number_of_accounts)("time", (end - start).count() / 1000));
      }
      catch (...)
      {
         throw;
      }

   }

   operation get_prototype_operation( string operation_name )
   {
      auto it = _prototype_ops.find( operation_name );
      if( it == _prototype_ops.end() )
         FC_THROW("Unsupported operation: \"${operation_name}\"", ("operation_name", operation_name));
      return it->second;
   }

   vector<vote_id_type> get_votes_ids(const string &account_name_or_id) const
   {
      try
      {
         return _remote_db->get_votes_ids(account_name_or_id);
      }
      FC_CAPTURE_AND_RETHROW( (account_name_or_id) )
   }

   votes_info get_votes(const string &account_name_or_id) const
   {
      try
      {
         return _remote_db->get_votes(account_name_or_id);
      }
      FC_CAPTURE_AND_RETHROW( (account_name_or_id) )
   }

   vector<account_object> get_voters_by_id(const vote_id_type &vote_id) const
   {
      try
      {
         return _remote_db->get_voters_by_id(vote_id);
      }
      FC_CAPTURE_AND_RETHROW( (vote_id) )
   }

   voters_info get_voters(const string &account_name_or_id) const
   {
      try
      {
         return _remote_db->get_voters(account_name_or_id);
      }
      FC_CAPTURE_AND_RETHROW( (account_name_or_id) )
   }

   std::map<sidechain_type, std::vector<std::string>> get_son_listener_log()
   {
      use_sidechain_api();
      try
      {
         return (*_remote_sidechain)->get_son_listener_log();
      }
      FC_CAPTURE_AND_RETHROW()
   }

   string                  _wallet_filename;
   wallet_data             _wallet;

   map<public_key_type,string> _keys;
   fc::sha512                  _checksum;

   chain_id_type           _chain_id;
   fc::api<login_api>      _remote_api;
   fc::api<database_api>   _remote_db;
   fc::api<network_broadcast_api>   _remote_net_broadcast;
   fc::api<history_api>    _remote_hist;
   fc::api<bookie_api>    _remote_bookie;
   optional< fc::api<network_node_api> > _remote_net_node;
   optional< fc::api<graphene::debug_witness::debug_api> > _remote_debug;
   optional< fc::api<graphene::peerplays_sidechain::sidechain_api> > _remote_sidechain;

   flat_map<string, operation> _prototype_ops;

   static_variant_map _operation_which_map = create_static_variant_map< operation >();

   typedef multi_index_container<
      tournament_object,
      indexed_by<
         ordered_unique< tag<by_id>, member< object, object_id_type, &object::id > > > > tournament_index_type;
   tournament_index_type tournament_cache;

   typedef multi_index_container<
      match_object,
      indexed_by<
         ordered_unique< tag<by_id>, member< object, object_id_type, &object::id > > > > match_index_type;
   match_index_type match_cache;

   typedef multi_index_container<
      game_object,
      indexed_by<
         ordered_unique< tag<by_id>, member< object, object_id_type, &object::id > > > > game_index_type;
   game_index_type game_cache;

#ifdef __unix__
   mode_t                  _old_umask;
#endif
   const string _wallet_filename_extension = ".wallet";

   mutable map<asset_id_type, asset_object> _asset_cache;
};

std::string operation_printer::fee(const asset& a)const {
   out << "   (Fee: " << wallet.get_asset(a.asset_id).amount_to_pretty_string(a) << ")";
   return "";
}

template<typename T>
std::string operation_printer::operator()(const T& op)const
{
   //balance_accumulator acc;
   //op.get_balance_delta( acc, result );
   auto a = wallet.get_asset( op.fee.asset_id );
   auto payer = wallet.get_account( op.fee_payer() );

   string op_name = fc::get_typename<T>::name();
   if( op_name.find_last_of(':') != string::npos )
      op_name.erase(0, op_name.find_last_of(':')+1);
   out << op_name <<" ";
  // out << "balance delta: " << fc::json::to_string(acc.balance) <<"   ";
   out << payer.name << " fee: " << a.amount_to_pretty_string( op.fee );
   operation_result_printer rprinter(wallet);
   std::string str_result = result.visit(rprinter);
   if( str_result != "" )
      out << "   result: " << str_result;
   return "";
}
std::string operation_printer::operator()(const transfer_from_blind_operation& op)const
{
   auto a = wallet.get_asset( op.fee.asset_id );
   auto receiver = wallet.get_account( op.to );

   out <<  receiver.name
   << " received " << a.amount_to_pretty_string( op.amount ) << " from blinded balance";
   return "";
}
std::string operation_printer::operator()(const transfer_to_blind_operation& op)const
{
   auto fa = wallet.get_asset( op.fee.asset_id );
   auto a = wallet.get_asset( op.amount.asset_id );
   auto sender = wallet.get_account( op.from );

   out <<  sender.name
   << " sent " << a.amount_to_pretty_string( op.amount ) << " to " << op.outputs.size() << " blinded balance" << (op.outputs.size()>1?"s":"")
   << " fee: " << fa.amount_to_pretty_string( op.fee );
   return "";
}
string operation_printer::operator()(const transfer_operation& op) const
{
   out << "Transfer " << wallet.get_asset(op.amount.asset_id).amount_to_pretty_string(op.amount)
       << " from " << wallet.get_account(op.from).name << " to " << wallet.get_account(op.to).name;
   std::string memo;
   if( op.memo )
   {
      bool is_encrypted = ((op.memo->from != public_key_type()) && (op.memo->to != public_key_type()));
      if (is_encrypted) {
         if( wallet.is_locked() )
         {
            out << " -- Unlock wallet to see memo.";
         } else {
            try {
               FC_ASSERT(wallet._keys.count(op.memo->to) || wallet._keys.count(op.memo->from), "Memo is encrypted to a key ${to} or ${from} not in this wallet.", ("to", op.memo->to)("from",op.memo->from));
               if( wallet._keys.count(op.memo->to) ) {
                  auto my_key = wif_to_key(wallet._keys.at(op.memo->to));
                  FC_ASSERT(my_key, "Unable to recover private key to decrypt memo. Wallet may be corrupted.");
                  memo = op.memo->get_message(*my_key, op.memo->from);
                  out << " -- Memo: " << memo;
               } else {
                  auto my_key = wif_to_key(wallet._keys.at(op.memo->from));
                  FC_ASSERT(my_key, "Unable to recover private key to decrypt memo. Wallet may be corrupted.");
                  memo = op.memo->get_message(*my_key, op.memo->to);
                  out << " -- Memo: " << memo;
               }
            } catch (const fc::exception& e) {
               out << " -- could not decrypt memo";
               elog("Error when decrypting memo: ${e}", ("e", e.to_detail_string()));
            }
         }
      } else {
         memo = op.memo->get_message(private_key_type(), public_key_type());
         out << " -- Memo: " << memo;
      }
   }
   fee(op.fee);
   return memo;
}

std::string operation_printer::operator()(const account_create_operation& op) const
{
   out << "Create Account '" << op.name << "'";
   return fee(op.fee);
}

std::string operation_printer::operator()(const account_update_operation& op) const
{
   out << "Update Account '" << wallet.get_account(op.account).name << "'";
   return fee(op.fee);
}

std::string operation_printer::operator()(const asset_create_operation& op) const
{
   out << "Create ";
   if( op.bitasset_opts.valid() )
      out << "BitAsset ";
   else
      out << "User-Issued Asset ";
   out << "'" << op.symbol << "' with issuer " << wallet.get_account(op.issuer).name;
   return fee(op.fee);
}

std::string operation_printer::operator()(const lottery_asset_create_operation& op) const
{
   out << "Create ";
   if( op.bitasset_opts.valid() )
      out << "BitAsset ";
   else
      out << "User-Issued Asset ";
   out << "'" << op.symbol << "' with issuer " << wallet.get_account(op.issuer).name;
   return fee(op.fee);
}

std::string operation_printer::operator()(const asset_dividend_distribution_operation& op)const
{
   asset_object dividend_paying_asset = wallet.get_asset(op.dividend_asset_id);
   account_object receiver = wallet.get_account(op.account_id);

   out << receiver.name << " received dividend payments for " << dividend_paying_asset.symbol << ": ";
   std::vector<std::string> pretty_payout_amounts;
   for (const asset& payment : op.amounts)
   {
      asset_object payout_asset = wallet.get_asset(payment.asset_id);
      pretty_payout_amounts.push_back(payout_asset.amount_to_pretty_string(payment));
   }
   out << boost::algorithm::join(pretty_payout_amounts, ", ");
   return "";
}

std::string operation_printer::operator()(const tournament_payout_operation& op)const
{
    asset_object payout_asset = wallet.get_asset(op.payout_amount.asset_id);

    out << "Tournament #" << std::string(object_id_type(op.tournament_id)) << " Payout : "
        << "Account '" << wallet.get_account(op.payout_account_id).name
        << "', Amount " <<  payout_asset.amount_to_pretty_string(op.payout_amount) << ", Type "
        << (op.type == payout_type::buyin_refund ? "buyin refund" : (op.type == payout_type::rake_fee ? "rake fee" : "prize award"))
        << ".";
    return "";
}

std::string operation_printer::operator()(const bet_place_operation& op)const
{
   auto fee_asset = wallet.get_asset(op.fee.asset_id);
   auto asset = wallet.get_asset(op.amount_to_bet.asset_id);
   auto bettor = wallet.get_account(op.bettor_id);

   out << bettor.name << " placed a " << fc::json::to_string(op.back_or_lay) << " bet for "
       << asset.amount_to_pretty_string(op.amount_to_bet) << " at odds " << ((double)op.backer_multiplier /  GRAPHENE_BETTING_ODDS_PRECISION)
       << " on market " << fc::json::to_string(op.betting_market_id)
       << " fee: " << fee_asset.amount_to_pretty_string(op.fee);
   return "";
}

std::string operation_printer::operator()(const bet_matched_operation& op)const
{
   auto asset = wallet.get_asset(op.amount_bet.asset_id);
   auto bettor = wallet.get_account(op.bettor_id);

   out << "  " << bettor.name << "'s bet " << fc::json::to_string(op.bet_id) << " matched " << asset.amount_to_pretty_string(op.amount_bet) << " at odds " << ((double)op.backer_multiplier /  GRAPHENE_BETTING_ODDS_PRECISION);
   return "";
}

std::string operation_printer::operator()(const bet_canceled_operation& op)const
{
   auto asset = wallet.get_asset(op.stake_returned.asset_id);
   auto bettor = wallet.get_account(op.bettor_id);

   out << "  " << bettor.name << "'s bet " << fc::json::to_string(op.bet_id) << " was canceled, " << asset.amount_to_pretty_string(op.stake_returned) << " returned";
   return "";
}

std::string operation_printer::operator()(const bet_adjusted_operation& op)const
{
   auto asset = wallet.get_asset(op.stake_returned.asset_id);
   auto bettor = wallet.get_account(op.bettor_id);

   out << "  " << bettor.name << "'s bet " << fc::json::to_string(op.bet_id) << " was adjusted, " << asset.amount_to_pretty_string(op.stake_returned) << " returned";
   return "";
}

std::string operation_result_printer::operator()(const void_result& x) const
{
   return "";
}

std::string operation_result_printer::operator()(const object_id_type& oid)
{
   return std::string(oid);
}

std::string operation_result_printer::operator()(const asset& a)
{
   return _wallet.get_asset(a.asset_id).amount_to_pretty_string(a);
}

}}}

namespace graphene { namespace wallet {
   vector<brain_key_info> utility::derive_owner_keys_from_brain_key(string brain_key, int number_of_desired_keys)
   {
      // Safety-check
      FC_ASSERT( number_of_desired_keys >= 1 );

      // Create as many derived owner keys as requested
      vector<brain_key_info> results;
      brain_key = graphene::wallet::detail::normalize_brain_key(brain_key);
      for (int i = 0; i < number_of_desired_keys; ++i) {
        fc::ecc::private_key priv_key = graphene::wallet::detail::derive_private_key( brain_key, i );

        brain_key_info result;
        result.brain_priv_key = brain_key;
        result.wif_priv_key = key_to_wif( priv_key );
        result.pub_key = priv_key.get_public_key();

        results.push_back(result);
      }

      return results;
   }
}}

namespace graphene { namespace wallet {

wallet_api::wallet_api(const wallet_data& initial_data, fc::api<login_api> rapi)
   : my(new detail::wallet_api_impl(*this, initial_data, rapi))
{
}

wallet_api::~wallet_api()
{
}

bool wallet_api::copy_wallet_file(string destination_filename)
{
   return my->copy_wallet_file(destination_filename);
}

optional<signed_block_with_info> wallet_api::get_block(uint32_t num)
{
   return my->_remote_db->get_block(num);
}

vector<optional<signed_block>> wallet_api::get_blocks(uint32_t block_num_from, uint32_t block_num_to) const
{
   return my->_remote_db->get_blocks(block_num_from, block_num_to);
}

uint64_t wallet_api::get_account_count() const
{
   return my->_remote_db->get_account_count();
}

vector<account_object> wallet_api::list_my_accounts()
{
   return vector<account_object>(my->_wallet.my_accounts.begin(), my->_wallet.my_accounts.end());
}

map<string,account_id_type> wallet_api::list_accounts(const string& lowerbound, uint32_t limit)
{
   return my->_remote_db->lookup_accounts(lowerbound, limit);
}

vector<asset> wallet_api::list_account_balances(const string& id)
{
   if( auto real_id = detail::maybe_id<account_id_type>(id) )
      return my->_remote_db->get_account_balances(id, flat_set<asset_id_type>());
   return my->_remote_db->get_account_balances(id, flat_set<asset_id_type>());
}

vector<asset_object> wallet_api::list_assets(const string& lowerbound, uint32_t limit)const
{
   return my->_remote_db->list_assets( lowerbound, limit );
}

uint64_t wallet_api::get_asset_count()const
{
   return my->_remote_db->get_asset_count();
}

vector<asset_object> wallet_api::get_lotteries( asset_id_type stop,
                                                unsigned limit,
                                                asset_id_type start )const
{
   return my->_remote_db->get_lotteries( stop, limit, start );
}

vector<asset_object> wallet_api::get_account_lotteries(  account_id_type issuer,
                                                         asset_id_type stop,
                                                         unsigned limit,
                                                         asset_id_type start )const
{
   return my->_remote_db->get_account_lotteries( issuer, stop, limit, start );
}

asset wallet_api::get_lottery_balance( asset_id_type lottery_id )const
{
   return my->_remote_db->get_lottery_balance( lottery_id );
}

vector<operation_detail> wallet_api::get_account_history(string name, int limit) const
{
   FC_ASSERT(my->get_account(name).name == name, "Account name: ${account_name} doesn't exist", ("account_name", name));
   vector<operation_detail> result;

   while (limit > 0)
   {
      bool skip_first_row = false;
      operation_history_id_type start;
      if (result.size())
      {
         start = result.back().op.id;
         if (start == operation_history_id_type()) // no more data
            break;
         start = start + (-1);
         if (start == operation_history_id_type()) // will return most recent history if directly call remote API with this
         {
            start = start + 1;
            skip_first_row = true;
         }
      }

      int page_limit = skip_first_row ? std::min(100, limit + 1) : std::min(100, limit);

      vector<operation_history_object> current = my->_remote_hist->get_account_history(name, operation_history_id_type(),
                                                                                       page_limit, start);
      bool first_row = true;
      for (auto &o : current)
      {
         if (first_row)
         {
            first_row = false;
            if (skip_first_row)
            {
               continue;
            }
         }
         std::stringstream ss;
         auto memo = o.op.visit(detail::operation_printer(ss, *my, o.result));
         result.push_back(operation_detail{memo, ss.str(), o});
      }

      if (int(current.size()) < page_limit)
         break;

      limit -= current.size();
      if (skip_first_row)
         ++limit;
   }

   return result;
}

vector<operation_detail> wallet_api::get_relative_account_history(string name, uint32_t stop, int limit, uint32_t start)const
{

   FC_ASSERT( start > 0 || limit <= 100 );
   FC_ASSERT(my->get_account(name).name == name, "Account name: ${account_name} doesn't exist", ("account_name", name));


   vector<operation_detail> result;

   while( limit > 0 )
   {
      vector <operation_history_object> current = my->_remote_hist->get_relative_account_history(name, stop, std::min<uint32_t>(100, limit), start);
      for (auto &o : current) {
         std::stringstream ss;
         auto memo = o.op.visit(detail::operation_printer(ss, *my, o.result));
         result.push_back(operation_detail{memo, ss.str(), o});
      }
      if (current.size() < std::min<uint32_t>(100, limit))
         break;
      limit -= current.size();
      start -= 100;
      if( start == 0 ) break;
   }
   return result;
}
vector<account_balance_object> wallet_api::list_core_accounts()const
{
   return my->_remote_hist->list_core_accounts();
}

vector<bucket_object> wallet_api::get_market_history( string symbol1, string symbol2, uint32_t bucket , fc::time_point_sec start, fc::time_point_sec end )const
{
   return my->_remote_hist->get_market_history( symbol1, symbol2, bucket, start, end );
}

vector<limit_order_object> wallet_api::get_limit_orders(string a, string b, uint32_t limit)const
{
   return my->_remote_db->get_limit_orders(a, b, limit);
}

vector<call_order_object> wallet_api::get_call_orders(string a, uint32_t limit)const
{
   return my->_remote_db->get_call_orders(a, limit);
}

vector<force_settlement_object> wallet_api::get_settle_orders(string a, uint32_t limit)const
{
   return my->_remote_db->get_settle_orders(a, limit);
}

brain_key_info wallet_api::suggest_brain_key()const
{
   brain_key_info result;
   // create a private key for secure entropy
   fc::sha256 sha_entropy1 = fc::ecc::private_key::generate().get_secret();
   fc::sha256 sha_entropy2 = fc::ecc::private_key::generate().get_secret();
   fc::bigint entropy1( sha_entropy1.data(), sha_entropy1.data_size() );
   fc::bigint entropy2( sha_entropy2.data(), sha_entropy2.data_size() );
   fc::bigint entropy(entropy1);
   entropy <<= 8*sha_entropy1.data_size();
   entropy += entropy2;
   string brain_key = "";

   for( int i=0; i<BRAIN_KEY_WORD_COUNT; i++ )
   {
      fc::bigint choice = entropy % graphene::words::word_list_size;
      entropy /= graphene::words::word_list_size;
      if( i > 0 )
         brain_key += " ";
      brain_key += graphene::words::word_list[ choice.to_int64() ];
   }

   brain_key = normalize_brain_key(brain_key);
   fc::ecc::private_key priv_key = derive_private_key( brain_key, 0 );
   result.brain_priv_key = brain_key;
   result.wif_priv_key = key_to_wif( priv_key );
   result.pub_key = priv_key.get_public_key();
   return result;
}

vector<brain_key_info> wallet_api::derive_owner_keys_from_brain_key(string brain_key, int number_of_desired_keys) const
{
   return graphene::wallet::utility::derive_owner_keys_from_brain_key(brain_key, number_of_desired_keys);
}

bool wallet_api::is_public_key_registered(string public_key) const
{
   bool is_known = my->_remote_db->is_public_key_registered(public_key);
   return is_known;
}

pair<public_key_type,string> wallet_api::get_private_key_from_password( string account, string role, string password )const {
   auto seed = password + account + role;
   FC_ASSERT( seed.size() );
   auto secret = fc::sha256::hash( seed.c_str(), seed.size() );
   auto priv = fc::ecc::private_key::regenerate( secret );
   return std::make_pair( public_key_type( priv.get_public_key() ), key_to_wif( priv ) );
}

string wallet_api::serialize_transaction( signed_transaction tx )const
{
   return fc::to_hex(fc::raw::pack(tx));
}

variant wallet_api::get_object( object_id_type id ) const
{
   return my->_remote_db->get_objects({id});
}

string wallet_api::get_wallet_filename() const
{
   return my->get_wallet_filename();
}

transaction_handle_type wallet_api::begin_builder_transaction()
{
   return my->begin_builder_transaction();
}

void wallet_api::add_operation_to_builder_transaction(transaction_handle_type transaction_handle, const operation& op)
{
   my->add_operation_to_builder_transaction(transaction_handle, op);
}

void wallet_api::replace_operation_in_builder_transaction(transaction_handle_type handle, unsigned operation_index, const operation& new_op)
{
   my->replace_operation_in_builder_transaction(handle, operation_index, new_op);
}

asset wallet_api::set_fees_on_builder_transaction(transaction_handle_type handle, string fee_asset)
{
   return my->set_fees_on_builder_transaction(handle, fee_asset);
}

transaction wallet_api::preview_builder_transaction(transaction_handle_type handle)
{
   return my->preview_builder_transaction(handle);
}

signed_transaction wallet_api::sign_builder_transaction(transaction_handle_type transaction_handle, bool broadcast)
{
   return my->sign_builder_transaction(transaction_handle, broadcast);
}

pair<transaction_id_type,signed_transaction> wallet_api::broadcast_transaction(signed_transaction tx)
{
    return my->broadcast_transaction(tx);
}

signed_transaction wallet_api::propose_builder_transaction(
   transaction_handle_type handle,
   time_point_sec expiration,
   uint32_t review_period_seconds,
   bool broadcast)
{
   return my->propose_builder_transaction(handle, expiration, review_period_seconds, broadcast);
}

signed_transaction wallet_api::propose_builder_transaction2(
   transaction_handle_type handle,
   string account_name_or_id,
   time_point_sec expiration,
   uint32_t review_period_seconds,
   bool broadcast)
{
   return my->propose_builder_transaction2(handle, account_name_or_id, expiration, review_period_seconds, broadcast);
}

void wallet_api::remove_builder_transaction(transaction_handle_type handle)
{
   return my->remove_builder_transaction(handle);
}

account_object wallet_api::get_account(string account_name_or_id) const
{
   return my->get_account(account_name_or_id);
}

asset_object wallet_api::get_asset(string asset_name_or_id) const
{
   auto a = my->find_asset(asset_name_or_id);
   FC_ASSERT(a, "Asset name or id: ${asset_name_or_id} doesn't exist", ("asset_name_or_id", asset_name_or_id) );
   return *a;
}

asset_bitasset_data_object wallet_api::get_bitasset_data(string asset_name_or_id) const
{
   auto asset = get_asset(asset_name_or_id);
   FC_ASSERT(asset.is_market_issued() && asset.bitasset_data_id);
   return my->get_object<asset_bitasset_data_object>(*asset.bitasset_data_id);
}

account_id_type wallet_api::get_account_id(string account_name_or_id) const
{
   return my->get_account_id(account_name_or_id);
}

asset_id_type wallet_api::get_asset_id(string asset_symbol_or_id) const
{
   return my->get_asset_id(asset_symbol_or_id);
}

bool wallet_api::import_key(string account_name_or_id, string wif_key)
{
   FC_ASSERT(!is_locked(), "Wallet is locked, please unlock to get all operations available");
   // backup wallet
   fc::optional<fc::ecc::private_key> optional_private_key = wif_to_key(wif_key);
   if (!optional_private_key)
      FC_THROW("Invalid private key");
   string shorthash = detail::address_to_shorthash(optional_private_key->get_public_key());
   copy_wallet_file( "before-import-key-" + shorthash );

   if( my->import_key(account_name_or_id, wif_key) )
   {
      save_wallet_file();
      copy_wallet_file( "after-import-key-" + shorthash );
      return true;
   }
   return false;
}

map<string, bool> wallet_api::import_accounts( string filename, string password )
{
   FC_ASSERT( !is_locked(), "Wallet is locked, please unlock to get all operations available" );
   FC_ASSERT( fc::exists( filename ) );

   const auto imported_keys = fc::json::from_file<exported_keys>( filename );

   const auto password_hash = fc::sha512::hash( password );
   FC_ASSERT( fc::sha512::hash( password_hash ) == imported_keys.password_checksum );

   map<string, bool> result;
   for( const auto& item : imported_keys.account_keys )
   {
       const auto import_this_account = [ & ]() -> bool
       {
           try
           {
               const account_object account = get_account( item.account_name );
               const auto& owner_keys = account.owner.get_keys();
               const auto& active_keys = account.active.get_keys();

               for( const auto& public_key : item.public_keys )
               {
                   if( std::find( owner_keys.begin(), owner_keys.end(), public_key ) != owner_keys.end() )
                       return true;

                   if( std::find( active_keys.begin(), active_keys.end(), public_key ) != active_keys.end() )
                       return true;
               }
           }
           catch( ... )
           {
           }

           return false;
       };

       const auto should_proceed = import_this_account();
       result[ item.account_name ] = should_proceed;

       if( should_proceed )
       {
           uint32_t import_successes = 0;
           uint32_t import_failures = 0;
           // TODO: First check that all private keys match public keys
           for( const auto& encrypted_key : item.encrypted_private_keys )
           {
               try
               {
                  const auto plain_text = fc::aes_decrypt( password_hash, encrypted_key );
                  const auto private_key = fc::raw::unpack<private_key_type>( plain_text );

                  import_key( item.account_name, string( graphene::utilities::key_to_wif( private_key ) ) );
                  ++import_successes;
               }
               catch( const fc::exception& e )
               {
                  elog( "Couldn't import key due to exception ${e}", ("e", e.to_detail_string()) );
                  ++import_failures;
               }
           }
           ilog( "successfully imported ${n} keys for account ${name}", ("n", import_successes)("name", item.account_name) );
           if( import_failures > 0 )
              elog( "failed to import ${n} keys for account ${name}", ("n", import_failures)("name", item.account_name) );
       }
   }

   return result;
}

bool wallet_api::import_account_keys( string filename, string password, string src_account_name, string dest_account_name )
{
   FC_ASSERT( !is_locked(), "Wallet is locked, please unlock to get all operations available" );
   FC_ASSERT( fc::exists( filename ) );

   bool is_my_account = false;
   const auto accounts = list_my_accounts();
   for( const auto& account : accounts )
   {
       if( account.name == dest_account_name )
       {
           is_my_account = true;
           break;
       }
   }
   FC_ASSERT( is_my_account );

   const auto imported_keys = fc::json::from_file<exported_keys>( filename );

   const auto password_hash = fc::sha512::hash( password );
   FC_ASSERT( fc::sha512::hash( password_hash ) == imported_keys.password_checksum );

   bool found_account = false;
   for( const auto& item : imported_keys.account_keys )
   {
       if( item.account_name != src_account_name )
           continue;

       found_account = true;

       for( const auto& encrypted_key : item.encrypted_private_keys )
       {
           const auto plain_text = fc::aes_decrypt( password_hash, encrypted_key );
           const auto private_key = fc::raw::unpack<private_key_type>( plain_text );

           my->import_key( dest_account_name, string( graphene::utilities::key_to_wif( private_key ) ) );
       }

       return true;
   }
   save_wallet_file();

   FC_ASSERT( found_account );

   return false;
}

string wallet_api::normalize_brain_key(string s) const
{
   return detail::normalize_brain_key( s );
}

variant wallet_api::info()
{
   return my->info();
}

variant_object wallet_api::about() const
{
    return my->about();
}

fc::ecc::private_key wallet_api::derive_private_key(const std::string& prefix_string, int sequence_number) const
{
   return detail::derive_private_key( prefix_string, sequence_number );
}

signed_transaction wallet_api::register_account(string name,
                                                public_key_type owner_pubkey,
                                                public_key_type active_pubkey,
                                                string  registrar_account,
                                                string  referrer_account,
                                                uint32_t referrer_percent,
                                                bool broadcast)
{
   return my->register_account( name, owner_pubkey, active_pubkey, registrar_account, referrer_account, referrer_percent, broadcast );
}
signed_transaction wallet_api::create_account_with_brain_key(string brain_key, string account_name,
                                                             string registrar_account, string referrer_account,
                                                             bool broadcast /* = false */)
{
   return my->create_account_with_brain_key(
            brain_key, account_name, registrar_account,
            referrer_account, broadcast
            );
}
signed_transaction wallet_api::issue_asset(string to_account, string amount, string symbol,
                                           string memo, bool broadcast)
{
   return my->issue_asset(to_account, amount, symbol, memo, broadcast);
}

signed_transaction wallet_api::transfer(string from, string to, string amount,
                                        string asset_symbol, string memo, bool broadcast /* = false */)
{
   return my->transfer(from, to, amount, asset_symbol, memo, broadcast);
}
signed_transaction wallet_api::create_asset(string issuer,
                                            string symbol,
                                            uint8_t precision,
                                            asset_options common,
                                            fc::optional<bitasset_options> bitasset_opts,
                                            bool broadcast)

{
   return my->create_asset(issuer, symbol, precision, common, bitasset_opts, broadcast);
}

signed_transaction wallet_api::create_lottery(string issuer,
      string symbol,
      asset_options common,
      lottery_asset_options lottery_opts,
      bool broadcast)

{
   return my->create_lottery(issuer, symbol, common, lottery_opts, broadcast);
}


signed_transaction wallet_api::buy_ticket( asset_id_type lottery, account_id_type buyer, uint64_t tickets_to_buy )
{
   return my->buy_ticket(lottery, buyer, tickets_to_buy);
}

signed_transaction wallet_api::update_asset(string symbol,
                                            optional<string> new_issuer,
                                            asset_options new_options,
                                            bool broadcast /* = false */)
{
   return my->update_asset(symbol, new_issuer, new_options, broadcast);
}

signed_transaction wallet_api::update_bitasset(string symbol,
                                               bitasset_options new_options,
                                               bool broadcast /* = false */)
{
   return my->update_bitasset(symbol, new_options, broadcast);
}

signed_transaction wallet_api::update_dividend_asset(string symbol,
                                                     dividend_asset_options new_options,
                                                     bool broadcast /* = false */)
{
   return my->update_dividend_asset(symbol, new_options, broadcast);
}


signed_transaction wallet_api::update_asset_feed_producers(string symbol,
                                                           flat_set<string> new_feed_producers,
                                                           bool broadcast /* = false */)
{
   return my->update_asset_feed_producers(symbol, new_feed_producers, broadcast);
}

signed_transaction wallet_api::publish_asset_feed(string publishing_account,
                                                  string symbol,
                                                  price_feed feed,
                                                  bool broadcast /* = false */)
{
   return my->publish_asset_feed(publishing_account, symbol, feed, broadcast);
}

signed_transaction wallet_api::fund_asset_fee_pool(string from,
                                                   string symbol,
                                                   string amount,
                                                   bool broadcast /* = false */)
{
   return my->fund_asset_fee_pool(from, symbol, amount, broadcast);
}

signed_transaction wallet_api::reserve_asset(string from,
                                          string amount,
                                          string symbol,
                                          bool broadcast /* = false */)
{
   return my->reserve_asset(from, amount, symbol, broadcast);
}

signed_transaction wallet_api::global_settle_asset(string symbol,
                                                   price settle_price,
                                                   bool broadcast /* = false */)
{
   return my->global_settle_asset(symbol, settle_price, broadcast);
}

signed_transaction wallet_api::settle_asset(string account_to_settle,
                                            string amount_to_settle,
                                            string symbol,
                                            bool broadcast /* = false */)
{
   return my->settle_asset(account_to_settle, amount_to_settle, symbol, broadcast);
}

signed_transaction wallet_api::whitelist_account(string authorizing_account,
                                                 string account_to_list,
                                                 account_whitelist_operation::account_listing new_listing_status,
                                                 bool broadcast /* = false */)
{
   return my->whitelist_account(authorizing_account, account_to_list, new_listing_status, broadcast);
}

signed_transaction wallet_api::create_committee_member(string owner_account, string url,
                                               bool broadcast /* = false */)
{
   return my->create_committee_member(owner_account, url, broadcast);
}

map<string,witness_id_type> wallet_api::list_witnesses(const string& lowerbound, uint32_t limit)
{
   return my->_remote_db->lookup_witness_accounts(lowerbound, limit);
}

map<string,committee_member_id_type> wallet_api::list_committee_members(const string& lowerbound, uint32_t limit)
{
   return my->_remote_db->lookup_committee_member_accounts(lowerbound, limit);
}

map<string, worker_id_type> wallet_api::list_workers(const string& lowerbound, uint32_t limit)
{
   return my->_remote_db->lookup_worker_accounts(lowerbound, limit);
}

son_object wallet_api::get_son(string owner_account)
{
   return my->get_son(owner_account);
}

witness_object wallet_api::get_witness(string owner_account)
{
   return my->get_witness(owner_account);
}

bool wallet_api::is_witness(string owner_account)
{
   return my->is_witness(owner_account);
}

committee_member_object wallet_api::get_committee_member(string owner_account)
{
   return my->get_committee_member(owner_account);
}

vector<worker_object> wallet_api::get_workers(string owner_account)
{
   return my->get_workers(owner_account);
}

signed_transaction wallet_api::create_vesting_balance(string owner_account,
                                              string amount,
                                              string asset_symbol,
                                              vesting_balance_type vesting_type,
                                              bool broadcast /* = false */)
{
   return my->create_vesting_balance(owner_account, amount, asset_symbol, vesting_type, broadcast);
}

signed_transaction wallet_api::create_son(string owner_account,
                                          string url,
                                          vesting_balance_id_type deposit_id,
                                          vesting_balance_id_type pay_vb_id,
                                          flat_map<sidechain_type, string> sidechain_public_keys,
                                          bool broadcast /* = false */)
{
   return my->create_son(owner_account, url, deposit_id, pay_vb_id, sidechain_public_keys, broadcast);
}

signed_transaction wallet_api::try_create_son(string owner_account,
                              string url,
                              flat_map<sidechain_type, string> sidechain_public_keys,
                              bool broadcast /* = false */)
{
   vesting_balance_id_type deposit_id;
   bool deposit_found = false;
   vesting_balance_id_type pay_vb_id;
   bool pay_vb_found = false;
   vector<vesting_balance_object_with_info> vbs = get_vesting_balances(owner_account);
   for(const auto& vb: vbs)
   {
      if ((vb.balance_type == vesting_balance_type::son) &&
          (vb.get_asset_amount() >= my->get_global_properties().parameters.son_vesting_amount()) &&
          (vb.policy.which() == vesting_policy::tag<dormant_vesting_policy>::value))
      {
         deposit_found = true;
         deposit_id = vb.id;
      }
      if ((vb.balance_type == vesting_balance_type::normal) &&
          (vb.policy.which() == vesting_policy::tag<linear_vesting_policy>::value))
      {
         pay_vb_found = true;
         pay_vb_id = vb.id;
      }
   }
   if (!deposit_found || !pay_vb_found)
      FC_THROW("Failed to find vesting balance objects");
   return my->create_son(owner_account, url, deposit_id, pay_vb_id, sidechain_public_keys, broadcast);
}

signed_transaction wallet_api::update_son(string owner_account,
                                          string url,
                                          string block_signing_key,
                                          flat_map<sidechain_type, string> sidechain_public_keys,
                                          bool broadcast /* = false */)
{
   return my->update_son(owner_account, url, block_signing_key, sidechain_public_keys, broadcast);
}

signed_transaction wallet_api::activate_deregistered_son(const string & owner_account, bool broadcast) {
   return my->activate_deregistered_son(owner_account, broadcast);
}


signed_transaction wallet_api::update_son_vesting_balances(string owner_account,
                                                           optional<vesting_balance_id_type> new_deposit,
                                                           optional<vesting_balance_id_type> new_pay_vb,
                                                           bool broadcast /* = false */)
{
   return my->update_son_vesting_balances(owner_account, new_deposit, new_pay_vb, broadcast);
}

signed_transaction wallet_api::request_son_maintenance(string owner_account, bool broadcast)
{
   return my->request_son_maintenance(owner_account, broadcast);
}

signed_transaction wallet_api::cancel_request_son_maintenance(string owner_account, bool broadcast)
{
   return my->cancel_request_son_maintenance(owner_account, broadcast);
}

map<string, son_id_type> wallet_api::list_sons(const string& lowerbound, uint32_t limit)
{
   return my->_remote_db->lookup_son_accounts(lowerbound, limit);
}

map<string, son_id_type> wallet_api::list_active_sons()
{
    return my->list_active_sons();
}

map<son_id_type, string> wallet_api::get_son_network_status()
{
    return my->get_son_network_status();
}

optional<son_wallet_object> wallet_api::get_active_son_wallet()
{
    return my->get_active_son_wallet();
}

optional<son_wallet_object> wallet_api::get_son_wallet_by_time_point(time_point_sec time_point)
{
    return my->get_son_wallet_by_time_point(time_point);
}

vector<optional<son_wallet_object>> wallet_api::get_son_wallets(uint32_t limit)
{
    return my->get_son_wallets(limit);
}

signed_transaction wallet_api::add_sidechain_address(string account,
                                          sidechain_type sidechain,
                                          string deposit_public_key,
                                          string deposit_address,
                                          string withdraw_public_key,
                                          string withdraw_address,
                                          bool broadcast /* = false */)
{
   return my->add_sidechain_address(account, sidechain, deposit_public_key, deposit_address, withdraw_public_key, withdraw_address, broadcast);
}

signed_transaction wallet_api::delete_sidechain_address(string account,
                                          sidechain_type sidechain,
                                          bool broadcast /* = false */)
{
   return my->delete_sidechain_address(account, sidechain, broadcast);
}

signed_transaction wallet_api::sidechain_withdrawal_transaction(const string &son_name_or_id,
		                                      uint32_t block_num,
                                                      const sidechain_type& sidechain,
                                                      const std::string &peerplays_uid,
                                                      const std::string &peerplays_transaction_id,
                                                      const chain::account_id_type &peerplays_from,
                                                      const sidechain_type& withdraw_sidechain,
                                                      const std::string &withdraw_address,
                                                      const std::string &withdraw_currency,
                                                      const string &withdraw_amount)
{
   return my->sidechain_withdrawal_transaction(son_name_or_id,
      block_num, 
      sidechain,
      peerplays_uid,
      peerplays_transaction_id,
      peerplays_from,
      withdraw_sidechain,
      withdraw_address,
      withdraw_currency,
      withdraw_amount);
}

vector<optional<sidechain_address_object>> wallet_api::get_sidechain_addresses_by_account(string account)
{
   account_id_type account_id = get_account_id(account);
   return my->_remote_db->get_sidechain_addresses_by_account(account_id);
}

vector<optional<sidechain_address_object>> wallet_api::get_sidechain_addresses_by_sidechain(sidechain_type sidechain)
{
   return my->_remote_db->get_sidechain_addresses_by_sidechain(sidechain);
}

fc::optional<sidechain_address_object> wallet_api::get_sidechain_address_by_account_and_sidechain(string account, sidechain_type sidechain)
{
   account_id_type account_id = get_account_id(account);
   return my->_remote_db->get_sidechain_address_by_account_and_sidechain(account_id, sidechain);
}

uint64_t wallet_api::get_sidechain_addresses_count()
{
   return my->_remote_db->get_sidechain_addresses_count();
}

signed_transaction wallet_api::create_witness(string owner_account,
                                              string url,
                                              bool broadcast /* = false */)
{
   return my->create_witness(owner_account, url, broadcast);
}

signed_transaction wallet_api::create_worker(
   string owner_account,
   time_point_sec work_begin_date,
   time_point_sec work_end_date,
   share_type daily_pay,
   string name,
   string url,
   variant worker_settings,
   bool broadcast /* = false */)
{
   return my->create_worker( owner_account, work_begin_date, work_end_date,
      daily_pay, name, url, worker_settings, broadcast );
}

signed_transaction wallet_api::update_worker_votes(
   string owner_account,
   worker_vote_delta delta,
   bool broadcast /* = false */)
{
   return my->update_worker_votes( owner_account, delta, broadcast );
}

signed_transaction wallet_api::update_witness(
   string witness_name,
   string url,
   string block_signing_key,
   bool broadcast /* = false */)
{
   return my->update_witness(witness_name, url, block_signing_key, broadcast);
}

vector< vesting_balance_object_with_info > wallet_api::get_vesting_balances( string account_name )
{
   return my->get_vesting_balances( account_name );
}

signed_transaction wallet_api::withdraw_vesting(
   string witness_name,
   string amount,
   string asset_symbol,
   bool broadcast)
{
   return my->withdraw_vesting( witness_name, amount, asset_symbol, broadcast );
}

signed_transaction wallet_api::withdraw_GPOS_vesting_balance(
   string account_name,
   string amount,
   string asset_symbol,
   bool broadcast)
{
   return my->withdraw_GPOS_vesting_balance( account_name, amount, asset_symbol, broadcast );
}

signed_transaction wallet_api::vote_for_committee_member(string voting_account,
                                                 string witness,
                                                 bool approve,
                                                 bool broadcast /* = false */)
{
   return my->vote_for_committee_member(voting_account, witness, approve, broadcast);
}

signed_transaction wallet_api::vote_for_son(string voting_account,
                                                   string son,
                                                   bool approve,
                                                   bool broadcast /* = false */)
{
   return my->vote_for_son(voting_account, son, approve, broadcast);
}

signed_transaction wallet_api::update_son_votes(string voting_account,
                                                    std::vector<std::string> sons_to_approve,
                                                    std::vector<std::string> sons_to_reject,
                                                    uint16_t desired_number_of_sons,
                                                    bool broadcast /* = false */)
{
   return my->update_son_votes(voting_account, sons_to_approve, sons_to_reject, desired_number_of_sons, broadcast);
}

signed_transaction wallet_api::sidechain_deposit_transaction(  const string &son_name_or_id,
                                                               const sidechain_type& sidechain,
                                                               const string &transaction_id,
                                                               uint32_t operation_index,
                                                               const string &sidechain_from,
                                                               const string &sidechain_to,
                                                               const string &sidechain_currency,
                                                               int64_t sidechain_amount,
                                                               const string &peerplays_from_name_or_id,
                                                               const string &peerplays_to_name_or_id)
{
   return my->sidechain_deposit_transaction(son_name_or_id, 
      sidechain,
      transaction_id,
      operation_index,
      sidechain_from,
      sidechain_to,
      sidechain_currency,
      sidechain_amount,
      peerplays_from_name_or_id,
      peerplays_to_name_or_id);
}

signed_transaction wallet_api::vote_for_witness(string voting_account,
                                                string witness,
                                                bool approve,
                                                bool broadcast /* = false */)
{
   return my->vote_for_witness(voting_account, witness, approve, broadcast);
}

signed_transaction wallet_api::update_witness_votes(string voting_account,
                                                    std::vector<std::string> witnesses_to_approve,
                                                    std::vector<std::string> witnesses_to_reject,
                                                    uint16_t desired_number_of_witnesses,
                                                    bool broadcast /* = false */)
{
   return my->update_witness_votes(voting_account, witnesses_to_approve, witnesses_to_reject, desired_number_of_witnesses, broadcast);
}

signed_transaction wallet_api::set_voting_proxy(string account_to_modify,
                                                optional<string> voting_account,
                                                bool broadcast /* = false */)
{
   return my->set_voting_proxy(account_to_modify, voting_account, broadcast);
}

signed_transaction wallet_api::set_desired_witness_and_committee_member_count(string account_to_modify,
                                                                      uint16_t desired_number_of_witnesses,
                                                                      uint16_t desired_number_of_committee_members,
                                                                      bool broadcast /* = false */)
{
   return my->set_desired_witness_and_committee_member_count(account_to_modify, desired_number_of_witnesses,
                                                     desired_number_of_committee_members, broadcast);
}

void wallet_api::set_wallet_filename(string wallet_filename)
{
   my->_wallet_filename = wallet_filename;
}

signed_transaction wallet_api::sign_transaction(signed_transaction tx, bool broadcast /* = false */)
{ try {
   return my->sign_transaction( tx, broadcast);
} FC_CAPTURE_AND_RETHROW( (tx) ) }

signed_transaction wallet_api::add_transaction_signature( signed_transaction tx,
                                                          bool broadcast )
{
   return my->add_transaction_signature( tx, broadcast );
}

flat_set<public_key_type> wallet_api::get_transaction_signers(const signed_transaction &tx) const
{ try {
   return my->get_transaction_signers(tx);
} FC_CAPTURE_AND_RETHROW( (tx) ) }

vector<vector<account_id_type>> wallet_api::get_key_references(const vector<public_key_type> &keys) const
{ try {
   return my->get_key_references(keys);
} FC_CAPTURE_AND_RETHROW( (keys) ) }

operation wallet_api::get_prototype_operation(string operation_name)
{
   return my->get_prototype_operation( operation_name );
}

void wallet_api::dbg_make_uia(string creator, string symbol)
{
   FC_ASSERT(!is_locked(), "Wallet is locked, please unlock to get all operations available");
   my->dbg_make_uia(creator, symbol);
}

void wallet_api::dbg_make_mia(string creator, string symbol)
{
   FC_ASSERT(!is_locked(), "Wallet is locked, please unlock to get all operations available");
   my->dbg_make_mia(creator, symbol);
}

void wallet_api::dbg_push_blocks( std::string src_filename, uint32_t count )
{
   my->dbg_push_blocks( src_filename, count );
}

void wallet_api::dbg_generate_blocks( std::string debug_wif_key, uint32_t count )
{
   my->dbg_generate_blocks( debug_wif_key, count );
}

void wallet_api::dbg_stream_json_objects( const std::string& filename )
{
   my->dbg_stream_json_objects( filename );
}

void wallet_api::dbg_update_object( fc::variant_object update )
{
   my->dbg_update_object( update );
}

void wallet_api::network_add_nodes( const vector<string>& nodes )
{
   my->network_add_nodes( nodes );
}

vector< variant > wallet_api::network_get_connected_peers()
{
   return my->network_get_connected_peers();
}

void wallet_api::flood_network(string prefix, uint32_t number_of_transactions)
{
   FC_ASSERT(!is_locked(), "Wallet is locked, please unlock to get all operations available");
   my->flood_network(prefix, number_of_transactions);
}

signed_transaction wallet_api::propose_parameter_change(
   const string& proposing_account,
   fc::time_point_sec expiration_time,
   const variant_object& changed_values,
   bool broadcast /* = false */
   )
{
   return my->propose_parameter_change( proposing_account, expiration_time, changed_values, broadcast );
}

signed_transaction wallet_api::propose_fee_change(
   const string& proposing_account,
   fc::time_point_sec expiration_time,
   const variant_object& changed_fees,
   bool broadcast /* = false */
   )
{
   return my->propose_fee_change( proposing_account, expiration_time, changed_fees, broadcast );
}

signed_transaction wallet_api::propose_dividend_asset_update(
   const string& proposing_account,
   fc::time_point_sec expiration_time,
   const variant_object& changed_fees,
   bool broadcast /* = false */
   )
{
   return my->propose_dividend_asset_update( proposing_account, expiration_time, changed_fees, broadcast );
}

signed_transaction wallet_api::approve_proposal(
   const string& fee_paying_account,
   const string& proposal_id,
   const approval_delta& delta,
   bool broadcast /* = false */
   )
{
   return my->approve_proposal( fee_paying_account, proposal_id, delta, broadcast );
}

signed_transaction wallet_api::create_custom_permission(string owner,
                                                        string permission_name,
                                                        authority auth,
                                                        bool broadcast)
{
   return my->create_custom_permission(owner, permission_name, auth, broadcast);
}

signed_transaction wallet_api::update_custom_permission(string owner,
                                                        custom_permission_id_type permission_id,
                                                        fc::optional<authority> new_auth,
                                                        bool broadcast)
{
   return my->update_custom_permission(owner, permission_id, new_auth, broadcast);
}

signed_transaction wallet_api::delete_custom_permission(string owner,
                                                        custom_permission_id_type permission_id,
                                                        bool broadcast)
{
   return my->delete_custom_permission(owner, permission_id, broadcast);
}

signed_transaction wallet_api::create_custom_account_authority(string owner,
                                                               custom_permission_id_type permission_id,
                                                               int operation_type,
                                                               fc::time_point_sec valid_from,
                                                               fc::time_point_sec valid_to,
                                                               bool broadcast)
{
   return my->create_custom_account_authority(owner, permission_id, operation_type, valid_from, valid_to, broadcast);
}

signed_transaction wallet_api::update_custom_account_authority(string owner,
                                                               custom_account_authority_id_type auth_id,
                                                               fc::optional<fc::time_point_sec> new_valid_from,
                                                               fc::optional<fc::time_point_sec> new_valid_to,
                                                               bool broadcast)
{
   return my->update_custom_account_authority(owner, auth_id, new_valid_from, new_valid_to, broadcast);
}

signed_transaction wallet_api::delete_custom_account_authority(string owner,
                                                               custom_account_authority_id_type auth_id,
                                                               bool broadcast)
{
   return my->delete_custom_account_authority(owner, auth_id, broadcast);
}

vector<custom_permission_object> wallet_api::get_custom_permissions(string owner) const
{
   return my->get_custom_permissions(owner);
}

fc::optional<custom_permission_object> wallet_api::get_custom_permission_by_name(string owner, string permission_name) const
{
   return my->get_custom_permission_by_name(owner, permission_name);
}

vector<custom_account_authority_object> wallet_api::get_custom_account_authorities(string owner) const
{
   return my->get_custom_account_authorities(owner);
}

vector<custom_account_authority_object> wallet_api::get_custom_account_authorities_by_permission_id(custom_permission_id_type permission_id) const
{
   return my->get_custom_account_authorities_by_permission_id(permission_id);
}

vector<custom_account_authority_object> wallet_api::get_custom_account_authorities_by_permission_name(string owner, string permission_name) const
{
   return my->get_custom_account_authorities_by_permission_name(owner, permission_name);
}

vector<authority> wallet_api::get_active_custom_account_authorities_by_operation(string owner, int operation_type) const
{
   return my->get_active_custom_account_authorities_by_operation(owner, operation_type);
}

global_property_object wallet_api::get_global_properties() const
{
   return my->get_global_properties();
}

dynamic_global_property_object wallet_api::get_dynamic_global_properties() const
{
   return my->get_dynamic_global_properties();
}

string wallet_api::help()const
{
   std::vector<std::string> method_names = my->method_documentation.get_method_names();
   std::stringstream ss;
   for (const std::string method_name : method_names)
   {
      try
      {
         ss << my->method_documentation.get_brief_description(method_name);
      }
      catch (const fc::key_not_found_exception&)
      {
         ss << method_name << " (no help available)\n";
      }
   }
   return ss.str();
}

string wallet_api::gethelp(const string& method)const
{
   fc::api<wallet_api> tmp;
   std::stringstream ss;
   ss << "\n";

   if( method == "import_key" )
   {
      ss << "usage: import_key ACCOUNT_NAME_OR_ID  WIF_PRIVATE_KEY\n\n";
      ss << "example: import_key \"1.3.11\" 5KQwrPbwdL6PhXujxW37FSSQZ1JiwsST4cqQzDeyXtP79zkvFD3\n";
      ss << "example: import_key \"usera\" 5KQwrPbwdL6PhXujxW37FSSQZ1JiwsST4cqQzDeyXtP79zkvFD3\n";
   }
   else if( method == "transfer" )
   {
      ss << "usage: transfer FROM TO AMOUNT SYMBOL \"memo\" BROADCAST\n\n";
      ss << "example: transfer \"1.3.11\" \"1.3.4\" 1000.03 CORE \"memo\" true\n";
      ss << "example: transfer \"usera\" \"userb\" 1000.123 CORE \"memo\" true\n";
   }
   else if( method == "create_account_with_brain_key" )
   {
      ss << "usage: create_account_with_brain_key BRAIN_KEY ACCOUNT_NAME REGISTRAR REFERRER BROADCAST\n\n";
      ss << "example: create_account_with_brain_key \"my really long brain key\" \"newaccount\" \"1.3.11\" \"1.3.11\" true\n";
      ss << "example: create_account_with_brain_key \"my really long brain key\" \"newaccount\" \"someaccount\" \"otheraccount\" true\n";
      ss << "\n";
      ss << "This method should be used if you would like the wallet to generate new keys derived from the brain key.\n";
      ss << "The BRAIN_KEY will be used as the owner key, and the active key will be derived from the BRAIN_KEY.  Use\n";
      ss << "register_account if you already know the keys you know the public keys that you would like to register.\n";

   }
   else if( method == "register_account" )
   {
      ss << "usage: register_account ACCOUNT_NAME OWNER_PUBLIC_KEY ACTIVE_PUBLIC_KEY REGISTRAR REFERRER REFERRER_PERCENT BROADCAST\n\n";
      ss << "example: register_account \"newaccount\" \"CORE6MRyAjQq8ud7hVNYcfnVPJqcVpscN5So8BhtHuGYqET5GDW5CV\" \"CORE6MRyAjQq8ud7hVNYcfnVPJqcVpscN5So8BhtHuGYqET5GDW5CV\" \"1.3.11\" \"1.3.11\" 50 true\n";
      ss << "\n";
      ss << "Use this method to register an account for which you do not know the private keys.";
   }
   else if( method == "create_asset" )
   {
      ss << "usage: ISSUER SYMBOL PRECISION_DIGITS OPTIONS BITASSET_OPTIONS BROADCAST\n\n";
      ss << "PRECISION_DIGITS: the number of digits after the decimal point\n\n";
      ss << "Example value of OPTIONS: \n";
      ss << fc::json::to_pretty_string( graphene::chain::asset_options() );
      ss << "\nExample value of BITASSET_OPTIONS: \n";
      ss << fc::json::to_pretty_string( graphene::chain::bitasset_options() );
      ss << "\nBITASSET_OPTIONS may be null\n";
   }
   else
   {
      std::string doxygenHelpString = my->method_documentation.get_detailed_description(method);
      if (!doxygenHelpString.empty())
         ss << doxygenHelpString;
      else
         ss << "No help defined for method " << method << "\n";
   }

   return ss.str();
}

bool wallet_api::load_wallet_file( string wallet_filename )
{
   return my->load_wallet_file( wallet_filename );
}

void wallet_api::quit()
{
   my->quit();
}

void wallet_api::save_wallet_file( string wallet_filename )
{
   my->save_wallet_file( wallet_filename );
}

std::map<string,std::function<string(fc::variant,const fc::variants&)> >
wallet_api::get_result_formatters() const
{
   return my->get_result_formatters();
}

bool wallet_api::is_locked()const
{
   return my->is_locked();
}
bool wallet_api::is_new()const
{
   return my->_wallet.cipher_keys.size() == 0;
}

void wallet_api::encrypt_keys()
{
   my->encrypt_keys();
}

void wallet_api::lock()
{ try {
   FC_ASSERT( !is_locked() );
   encrypt_keys();
   for( auto key : my->_keys )
      key.second = key_to_wif(fc::ecc::private_key());
   my->_keys.clear();
   my->_checksum = fc::sha512();
   my->self.lock_changed(true);
} FC_CAPTURE_AND_RETHROW() }

void wallet_api::unlock(string password)
{ try {
   FC_ASSERT(password.size() > 0);
   auto pw = fc::sha512::hash(password.c_str(), password.size());
   vector<char> decrypted = fc::aes_decrypt(pw, my->_wallet.cipher_keys);
   auto pk = fc::raw::unpack<plain_keys>(decrypted);
   FC_ASSERT(pk.checksum == pw);
   my->_keys = std::move(pk.keys);
   my->_checksum = pk.checksum;
   my->self.lock_changed(false);
   my->resync_active_tournaments();
} FC_CAPTURE_AND_RETHROW() }

void wallet_api::set_password( string password )
{
   if( !is_new() )
      FC_ASSERT( !is_locked(), "The wallet must be unlocked before the password can be set" );
   my->_checksum = fc::sha512::hash( password.c_str(), password.size() );
   lock();
}

vector< signed_transaction > wallet_api::import_balance( string name_or_id, const vector<string>& wif_keys, bool broadcast )
{
   return my->import_balance( name_or_id, wif_keys, broadcast );
}

namespace detail {

vector< signed_transaction > wallet_api_impl::import_balance( string name_or_id, const vector<string>& wif_keys, bool broadcast )
{ try {
   FC_ASSERT(!is_locked());
   const dynamic_global_property_object& dpo = _remote_db->get_dynamic_global_properties();
   account_object claimer = get_account( name_or_id );
   uint32_t max_ops_per_tx = 30;

   map< address, private_key_type > keys;  // local index of address -> private key
   vector< address > addrs;
   bool has_wildcard = false;
   addrs.reserve( wif_keys.size() );
   for( const string& wif_key : wif_keys )
   {
      if( wif_key == "*" )
      {
         if( has_wildcard )
            continue;
         for( const public_key_type& pub : _wallet.extra_keys[ claimer.id ] )
         {
            addrs.push_back( pub );
            auto it = _keys.find( pub );
            if( it != _keys.end() )
            {
               fc::optional< fc::ecc::private_key > privkey = wif_to_key( it->second );
               FC_ASSERT( privkey );
               keys[ addrs.back() ] = *privkey;
            }
            else
            {
               wlog( "Somehow _keys has no private key for extra_keys public key ${k}", ("k", pub) );
            }
         }
         has_wildcard = true;
      }
      else
      {
         optional< private_key_type > key = wif_to_key( wif_key );
         FC_ASSERT( key.valid(), "Invalid private key" );
         fc::ecc::public_key pk = key->get_public_key();
         addrs.push_back( pk );
         keys[addrs.back()] = *key;
         // see chain/balance_evaluator.cpp
         addrs.push_back( pts_address( pk, false, 56 ) );
         keys[addrs.back()] = *key;
         addrs.push_back( pts_address( pk, true, 56 ) );
         keys[addrs.back()] = *key;
         addrs.push_back( pts_address( pk, false, 0 ) );
         keys[addrs.back()] = *key;
         addrs.push_back( pts_address( pk, true, 0 ) );
         keys[addrs.back()] = *key;
      }
   }

   vector< balance_object > balances = _remote_db->get_balance_objects( addrs );
   wdump((balances));
   addrs.clear();

   set<asset_id_type> bal_types;
   for( auto b : balances ) bal_types.insert( b.balance.asset_id );

   struct claim_tx
   {
      vector< balance_claim_operation > ops;
      set< address > addrs;
   };
   vector< claim_tx > claim_txs;

   for( const asset_id_type& a : bal_types )
   {
      balance_claim_operation op;
      op.deposit_to_account = claimer.id;
      for( const balance_object& b : balances )
      {
         if( b.balance.asset_id == a )
         {
            op.total_claimed = b.available( dpo.time );
            if( op.total_claimed.amount == 0 )
               continue;
            op.balance_to_claim = b.id;
            op.balance_owner_key = keys[b.owner].get_public_key();
            if( (claim_txs.empty()) || (claim_txs.back().ops.size() >= max_ops_per_tx) )
               claim_txs.emplace_back();
            claim_txs.back().ops.push_back(op);
            claim_txs.back().addrs.insert(b.owner);
         }
      }
   }

   vector< signed_transaction > result;

   for( const claim_tx& ctx : claim_txs )
   {
      signed_transaction tx;
      tx.operations.reserve( ctx.ops.size() );
      for( const balance_claim_operation& op : ctx.ops )
         tx.operations.emplace_back( op );
      set_operation_fees( tx, _remote_db->get_global_properties().parameters.current_fees );
      tx.validate();
      signed_transaction signed_tx = sign_transaction( tx, false );
      for( const address& addr : ctx.addrs )
         signed_tx.sign( keys[addr], _chain_id );
      // if the key for a balance object was the same as a key for the account we're importing it into,
      // we may end up with duplicate signatures, so remove those
      boost::erase(signed_tx.signatures, boost::unique<boost::return_found_end>(boost::sort(signed_tx.signatures)));
      result.push_back( signed_tx );
      if( broadcast )
         _remote_net_broadcast->broadcast_transaction(signed_tx);
   }

   return result;
} FC_CAPTURE_AND_RETHROW( (name_or_id) ) }

}

map<public_key_type, string> wallet_api::dump_private_keys()
{
   FC_ASSERT(!is_locked());
   return my->_keys;
}

signed_transaction wallet_api::update_account_keys(string name,
                                          public_key_type old_owner,
                                          public_key_type new_owner,
                                          public_key_type old_active,
                                          public_key_type new_active,
                                          bool broadcast )
{
   return my->update_account_keys(name, old_owner, new_owner, old_active, new_active, broadcast);
}

signed_transaction wallet_api::update_account_auth_key(string account_name,
                                                       authority_type type,
                                                       public_key_type key,
                                                       weight_type weight,
                                                       bool broadcast)
{
   return my->update_account_auth_key(account_name, type, key, weight, broadcast);
}

signed_transaction wallet_api::upgrade_account( string name, bool broadcast )
{
   return my->upgrade_account(name,broadcast);
}

signed_transaction wallet_api::sell_asset(string seller_account,
                                          string amount_to_sell,
                                          string symbol_to_sell,
                                          string min_to_receive,
                                          string symbol_to_receive,
                                          uint32_t expiration,
                                          bool   fill_or_kill,
                                          bool   broadcast)
{
   return my->sell_asset(seller_account, amount_to_sell, symbol_to_sell, min_to_receive,
                         symbol_to_receive, expiration, fill_or_kill, broadcast);
}

signed_transaction wallet_api::sell( string seller_account,
                                     string base,
                                     string quote,
                                     double rate,
                                     double amount,
                                     bool broadcast )
{
   std::stringstream ss;
   ss.str(std::string());
   ss << std::noshowpoint << amount;
   std::string amount_to_sell = ss.str();
   ss.str(std::string());
   ss << std::noshowpoint << rate * amount;
   std::string min_to_receive = ss.str();
   return my->sell_asset( seller_account, amount_to_sell, base,
                          min_to_receive, quote, 0, false, broadcast );
}

signed_transaction wallet_api::buy( string buyer_account,
                                    string base,
                                    string quote,
                                    double rate,
                                    double amount,
                                    bool broadcast )
{
   std::stringstream ss;
   ss.str(std::string());
   ss << std::noshowpoint << rate * amount;
   std::string amount_to_sell = ss.str();
   ss.str(std::string());
   ss << std::noshowpoint << amount;
   std::string min_to_receive = ss.str();
   return my->sell_asset( buyer_account, amount_to_sell, quote,
                          min_to_receive, base, 0, false, broadcast );
}

signed_transaction wallet_api::borrow_asset(string seller_name, string amount_to_sell,
                                                string asset_symbol, string amount_of_collateral, bool broadcast)
{
   FC_ASSERT(!is_locked(), "Wallet is locked, please unlock to get all operations available");
   return my->borrow_asset(seller_name, amount_to_sell, asset_symbol, amount_of_collateral, broadcast);
}

signed_transaction wallet_api::cancel_order(object_id_type order_id, bool broadcast)
{
   FC_ASSERT(!is_locked(), "Wallet is locked, please unlock to get all operations available");
   return my->cancel_order(order_id, broadcast);
}

string wallet_api::get_key_label( public_key_type key )const
{
   auto key_itr   = my->_wallet.labeled_keys.get<by_key>().find(key);
   if( key_itr != my->_wallet.labeled_keys.get<by_key>().end() )
      return key_itr->label;
   return string();
}

string wallet_api::get_private_key( public_key_type pubkey )const
{
   return key_to_wif( my->get_private_key( pubkey ) );
}

public_key_type  wallet_api::get_public_key( string label )const
{
   try { return fc::variant(label, 1).as<public_key_type>( 1 ); } catch ( ... ){}

   auto key_itr   = my->_wallet.labeled_keys.get<by_label>().find(label);
   if( key_itr != my->_wallet.labeled_keys.get<by_label>().end() )
      return key_itr->key;
   return public_key_type();
}

bool               wallet_api::set_key_label( public_key_type key, string label )
{
   auto result = my->_wallet.labeled_keys.insert( key_label{label,key} );
   if( result.second  ) return true;

   auto key_itr   = my->_wallet.labeled_keys.get<by_key>().find(key);
   auto label_itr = my->_wallet.labeled_keys.get<by_label>().find(label);
   if( label_itr == my->_wallet.labeled_keys.get<by_label>().end() )
   {
      if( key_itr != my->_wallet.labeled_keys.get<by_key>().end() )
         return my->_wallet.labeled_keys.get<by_key>().modify( key_itr, [&]( key_label& obj ){ obj.label = label; } );
   }
   return false;
}
map<string,public_key_type> wallet_api::get_blind_accounts()const
{
   map<string,public_key_type> result;
   for( const auto& item : my->_wallet.labeled_keys )
      result[item.label] = item.key;
   return result;
}
map<string,public_key_type> wallet_api::get_my_blind_accounts()const
{
   FC_ASSERT( !is_locked(), "Wallet is locked, please unlock to get all operations available" );
   map<string,public_key_type> result;
   for( const auto& item : my->_wallet.labeled_keys )
   {
      if( my->_keys.find(item.key) != my->_keys.end() )
         result[item.label] = item.key;
   }
   return result;
}

public_key_type    wallet_api::create_blind_account( string label, string brain_key  )
{
   FC_ASSERT( !is_locked(), "Wallet is locked, please unlock to get all operations available" );

   auto label_itr = my->_wallet.labeled_keys.get<by_label>().find(label);
   if( label_itr !=  my->_wallet.labeled_keys.get<by_label>().end() )
      FC_ASSERT( !"Key with label already exists" );
   brain_key = fc::trim_and_normalize_spaces( brain_key );
   auto secret = fc::sha256::hash( brain_key.c_str(), brain_key.size() );
   auto priv_key = fc::ecc::private_key::regenerate( secret );
   public_key_type pub_key  = priv_key.get_public_key();

   FC_ASSERT( set_key_label( pub_key, label ) );

   my->_keys[pub_key] = graphene::utilities::key_to_wif( priv_key );

   save_wallet_file();
   return pub_key;
}

vector<asset>   wallet_api::get_blind_balances( string key_or_label )
{
   vector<asset> result;
   map<asset_id_type, share_type> balances;

   vector<commitment_type> used;

   auto pub_key = get_public_key( key_or_label );
   auto& to_asset_used_idx = my->_wallet.blind_receipts.get<by_to_asset_used>();
   auto start = to_asset_used_idx.lower_bound( std::make_tuple(pub_key,asset_id_type(0),false)  );
   auto end = to_asset_used_idx.lower_bound( std::make_tuple(pub_key,asset_id_type(uint32_t(0xffffffff)),true)  );
   while( start != end )
   {
      if( !start->used  )
      {
         auto answer = my->_remote_db->get_blinded_balances( {start->commitment()} );
         if( answer.size() )
            balances[start->amount.asset_id] += start->amount.amount;
         else
            used.push_back( start->commitment() );
      }
      ++start;
   }
   for( const auto& u : used )
   {
      auto itr = my->_wallet.blind_receipts.get<by_commitment>().find( u );
      my->_wallet.blind_receipts.modify( itr, []( blind_receipt& r ){ r.used = true; } );
   }
   for( auto item : balances )
      result.push_back( asset( item.second, item.first ) );
   return result;
}

blind_confirmation wallet_api::transfer_from_blind( string from_blind_account_key_or_label,
                                                    string to_account_id_or_name,
                                                    string amount_in,
                                                    string symbol,
                                                    bool broadcast )
{ try {
   transfer_from_blind_operation from_blind;


   auto fees  = my->_remote_db->get_global_properties().parameters.current_fees;
   fc::optional<asset_object> asset_obj = get_asset(symbol);
   FC_ASSERT(asset_obj.valid(), "Could not find asset matching ${asset}", ("asset", symbol));
   auto amount = asset_obj->amount_from_string(amount_in);

   from_blind.fee  = fees->calculate_fee( from_blind, asset_obj->options.core_exchange_rate );

   auto blind_in = asset_obj->amount_to_string( from_blind.fee + amount );


   auto conf = blind_transfer_help( from_blind_account_key_or_label,
                               from_blind_account_key_or_label,
                               blind_in, symbol, false, true/*to_temp*/ );
   FC_ASSERT( conf.outputs.size() > 0 );

   auto to_account = my->get_account( to_account_id_or_name );
   from_blind.to = to_account.id;
   from_blind.amount = amount;
   from_blind.blinding_factor = conf.outputs.back().decrypted_memo.blinding_factor;
   from_blind.inputs.push_back( {conf.outputs.back().decrypted_memo.commitment, authority() } );
   from_blind.fee  = fees->calculate_fee( from_blind, asset_obj->options.core_exchange_rate );

   idump( (from_blind) );
   conf.trx.operations.push_back(from_blind);
   ilog( "about to validate" );
   conf.trx.validate();

   if( broadcast && conf.outputs.size() == 2 ) {

       // Save the change
       blind_confirmation::output conf_output;
       blind_confirmation::output change_output = conf.outputs[0];

       // The wallet must have a private key for confirmation.to, this is used to decrypt the memo
       public_key_type from_key = get_public_key(from_blind_account_key_or_label);
       conf_output.confirmation.to = from_key;
       conf_output.confirmation.one_time_key = change_output.confirmation.one_time_key;
       conf_output.confirmation.encrypted_memo = change_output.confirmation.encrypted_memo;
       conf_output.confirmation_receipt = conf_output.confirmation;
       //try {
       receive_blind_transfer( conf_output.confirmation_receipt, from_blind_account_key_or_label, "@"+to_account.name );
       //} catch ( ... ){}
   }

   ilog( "about to broadcast" );
   conf.trx = sign_transaction( conf.trx, broadcast );

   return conf;
} FC_CAPTURE_AND_RETHROW( (from_blind_account_key_or_label)(to_account_id_or_name)(amount_in)(symbol) ) }

blind_confirmation wallet_api::blind_transfer( string from_key_or_label,
                                               string to_key_or_label,
                                               string amount_in,
                                               string symbol,
                                               bool broadcast )
{
   return blind_transfer_help( from_key_or_label, to_key_or_label, amount_in, symbol, broadcast, false );
}
blind_confirmation wallet_api::blind_transfer_help( string from_key_or_label,
                                               string to_key_or_label,
                                               string amount_in,
                                               string symbol,
                                               bool broadcast,
                                               bool to_temp )
{
   blind_confirmation confirm;
   try {

   FC_ASSERT( !is_locked(), "Wallet is locked, please unlock to get all operations available" );
   public_key_type from_key = get_public_key(from_key_or_label);
   public_key_type to_key   = get_public_key(to_key_or_label);

   fc::optional<asset_object> asset_obj = get_asset(symbol);
   FC_ASSERT(asset_obj.valid(), "Could not find asset matching ${asset}", ("asset", symbol));

   blind_transfer_operation blind_tr;
   blind_tr.outputs.resize(2);

   auto fees  = my->_remote_db->get_global_properties().parameters.current_fees;

   auto amount = asset_obj->amount_from_string(amount_in);

   asset total_amount = asset_obj->amount(0);

   vector<fc::sha256> blinding_factors;

   //auto from_priv_key = my->get_private_key( from_key );

   blind_tr.fee  = fees->calculate_fee( blind_tr, asset_obj->options.core_exchange_rate );

   vector<commitment_type> used;

   auto& to_asset_used_idx = my->_wallet.blind_receipts.get<by_to_asset_used>();
   auto start = to_asset_used_idx.lower_bound( std::make_tuple(from_key,amount.asset_id,false)  );
   auto end = to_asset_used_idx.lower_bound( std::make_tuple(from_key,amount.asset_id,true)  );
   while( start != end )
   {
      auto result = my->_remote_db->get_blinded_balances( {start->commitment() } );
      if( result.size() == 0 )
      {
         used.push_back( start->commitment() );
      }
      else
      {
         blind_tr.inputs.push_back({start->commitment(), start->control_authority});
         blinding_factors.push_back( start->data.blinding_factor );
         total_amount += start->amount;

         if( total_amount >= amount + blind_tr.fee )
            break;
      }
      ++start;
   }
   for( const auto& u : used )
   {
      auto itr = my->_wallet.blind_receipts.get<by_commitment>().find( u );
      my->_wallet.blind_receipts.modify( itr, []( blind_receipt& r ){ r.used = true; } );
   }

   FC_ASSERT( total_amount >= amount+blind_tr.fee, "Insufficent Balance", ("available",total_amount)("amount",amount)("fee",blind_tr.fee) );

   auto one_time_key = fc::ecc::private_key::generate();
   auto secret       = one_time_key.get_shared_secret( to_key );
   auto child        = fc::sha256::hash( secret );
   auto nonce        = fc::sha256::hash( one_time_key.get_secret() );
   auto blind_factor = fc::sha256::hash( child );

   auto from_secret  = one_time_key.get_shared_secret( from_key );
   auto from_child   = fc::sha256::hash( from_secret );
   auto from_nonce   = fc::sha256::hash( nonce );

   auto change = total_amount - amount - blind_tr.fee;
   fc::sha256 change_blind_factor;
   fc::sha256 to_blind_factor;
   if( change.amount > 0 )
   {
      idump(("to_blind_factor")(blind_factor) );
      blinding_factors.push_back( blind_factor );
      change_blind_factor = fc::ecc::blind_sum( blinding_factors, blinding_factors.size() - 1 );
      wdump(("change_blind_factor")(change_blind_factor) );
   }
   else // change == 0
   {
      blind_tr.outputs.resize(1);
      blind_factor = fc::ecc::blind_sum( blinding_factors, blinding_factors.size() );
      idump(("to_sum_blind_factor")(blind_factor) );
      blinding_factors.push_back( blind_factor );
      idump(("nochange to_blind_factor")(blind_factor) );
   }
   fc::ecc::public_key from_pub_key = from_key;
   fc::ecc::public_key to_pub_key = to_key;

   blind_output to_out;
   to_out.owner       = to_temp ? authority() : authority( 1, public_key_type( to_pub_key.child( child ) ), 1 );
   to_out.commitment  = fc::ecc::blind( blind_factor, amount.amount.value );
   idump(("to_out.blind")(blind_factor)(to_out.commitment) );


   if( blind_tr.outputs.size() > 1 )
   {
      to_out.range_proof = fc::ecc::range_proof_sign( 0, to_out.commitment, blind_factor, nonce,  0, 0, amount.amount.value );

      blind_output change_out;
      change_out.owner       = authority( 1, public_key_type( from_pub_key.child( from_child ) ), 1 );
      change_out.commitment  = fc::ecc::blind( change_blind_factor, change.amount.value );
      change_out.range_proof = fc::ecc::range_proof_sign( 0, change_out.commitment, change_blind_factor, from_nonce,  0, 0, change.amount.value );
      blind_tr.outputs[1] = change_out;


      blind_confirmation::output conf_output;
      conf_output.label = from_key_or_label;
      conf_output.pub_key = from_key;
      conf_output.decrypted_memo.from = from_key;
      conf_output.decrypted_memo.amount = change;
      conf_output.decrypted_memo.blinding_factor = change_blind_factor;
      conf_output.decrypted_memo.commitment = change_out.commitment;
      conf_output.decrypted_memo.check   = from_secret._hash[0];
      conf_output.confirmation.one_time_key = one_time_key.get_public_key();
      conf_output.confirmation.to = from_key;
      conf_output.confirmation.encrypted_memo = fc::aes_encrypt( from_secret, fc::raw::pack( conf_output.decrypted_memo ) );
      conf_output.auth = change_out.owner;
      conf_output.confirmation_receipt = conf_output.confirmation;

      confirm.outputs.push_back( conf_output );
   }
   blind_tr.outputs[0] = to_out;

   blind_confirmation::output conf_output;
   conf_output.label = to_key_or_label;
   conf_output.pub_key = to_key;
   conf_output.decrypted_memo.from = from_key;
   conf_output.decrypted_memo.amount = amount;
   conf_output.decrypted_memo.blinding_factor = blind_factor;
   conf_output.decrypted_memo.commitment = to_out.commitment;
   conf_output.decrypted_memo.check   = secret._hash[0];
   conf_output.confirmation.one_time_key = one_time_key.get_public_key();
   conf_output.confirmation.to = to_key;
   conf_output.confirmation.encrypted_memo = fc::aes_encrypt( secret, fc::raw::pack( conf_output.decrypted_memo ) );
   conf_output.auth = to_out.owner;
   conf_output.confirmation_receipt = conf_output.confirmation;

   confirm.outputs.push_back( conf_output );

   /** commitments must be in sorted order */
   std::sort( blind_tr.outputs.begin(), blind_tr.outputs.end(),
              [&]( const blind_output& a, const blind_output& b ){ return a.commitment < b.commitment; } );
   std::sort( blind_tr.inputs.begin(), blind_tr.inputs.end(),
              [&]( const blind_input& a, const blind_input& b ){ return a.commitment < b.commitment; } );

   confirm.trx.operations.emplace_back( std::move(blind_tr) );
   ilog( "validate before" );
   confirm.trx.validate();
   confirm.trx = sign_transaction(confirm.trx, broadcast);

   if( broadcast )
   {
      for( const auto& out : confirm.outputs )
      {
         try { receive_blind_transfer( out.confirmation_receipt, from_key_or_label, "" ); } catch ( ... ){}
      }
   }

   return confirm;
} FC_CAPTURE_AND_RETHROW( (from_key_or_label)(to_key_or_label)(amount_in)(symbol)(broadcast)(confirm) ) }



/**
 *  Transfers a public balance from @from to one or more blinded balances using a
 *  stealth transfer.
 */
blind_confirmation wallet_api::transfer_to_blind( string from_account_id_or_name,
                                                  string asset_symbol,
                                                  /** map from key or label to amount */
                                                  vector<pair<string, string>> to_amounts,
                                                  bool broadcast )
{ try {
   FC_ASSERT( !is_locked(), "Wallet is locked, please unlock to get all operations available" );
   idump((to_amounts));

   blind_confirmation confirm;
   account_object from_account = my->get_account(from_account_id_or_name);

   fc::optional<asset_object> asset_obj = get_asset(asset_symbol);
   FC_ASSERT(asset_obj, "Could not find asset matching ${asset}", ("asset", asset_symbol));

   transfer_to_blind_operation bop;
   bop.from   = from_account.id;

   vector<fc::sha256> blinding_factors;

   asset total_amount = asset_obj->amount(0);

   for( auto item : to_amounts )
   {
      auto one_time_key = fc::ecc::private_key::generate();
      auto to_key       = get_public_key( item.first );
      auto secret       = one_time_key.get_shared_secret( to_key );
      auto child        = fc::sha256::hash( secret );
      auto nonce        = fc::sha256::hash( one_time_key.get_secret() );
      auto blind_factor = fc::sha256::hash( child );

      blinding_factors.push_back( blind_factor );

      auto amount = asset_obj->amount_from_string(item.second);
      total_amount += amount;


      fc::ecc::public_key to_pub_key = to_key;
      blind_output out;
      out.owner       = authority( 1, public_key_type( to_pub_key.child( child ) ), 1 );
      out.commitment  = fc::ecc::blind( blind_factor, amount.amount.value );
      if( to_amounts.size() > 1 )
         out.range_proof = fc::ecc::range_proof_sign( 0, out.commitment, blind_factor, nonce,  0, 0, amount.amount.value );


      blind_confirmation::output conf_output;
      conf_output.label = item.first;
      conf_output.pub_key = to_key;
      conf_output.decrypted_memo.amount = amount;
      conf_output.decrypted_memo.blinding_factor = blind_factor;
      conf_output.decrypted_memo.commitment = out.commitment;
      conf_output.decrypted_memo.check   = secret._hash[0];
      conf_output.confirmation.one_time_key = one_time_key.get_public_key();
      conf_output.confirmation.to = to_key;
      conf_output.confirmation.encrypted_memo = fc::aes_encrypt( secret, fc::raw::pack( conf_output.decrypted_memo ) );
      conf_output.confirmation_receipt = conf_output.confirmation;

      confirm.outputs.push_back( conf_output );

      bop.outputs.push_back(out);
   }
   bop.amount          = total_amount;
   bop.blinding_factor = fc::ecc::blind_sum( blinding_factors, blinding_factors.size() );

   /** commitments must be in sorted order */
   std::sort( bop.outputs.begin(), bop.outputs.end(),
              [&]( const blind_output& a, const blind_output& b ){ return a.commitment < b.commitment; } );

   confirm.trx.operations.push_back( bop );
   my->set_operation_fees( confirm.trx, my->_remote_db->get_global_properties().parameters.current_fees);
   confirm.trx.validate();
   confirm.trx = sign_transaction(confirm.trx, broadcast);

   if( broadcast )
   {
      for( const auto& out : confirm.outputs )
      {
         try { receive_blind_transfer( out.confirmation_receipt, "@"+from_account.name, "from @"+from_account.name ); } catch ( ... ){}
      }
   }

   return confirm;
} FC_CAPTURE_AND_RETHROW( (from_account_id_or_name)(asset_symbol)(to_amounts) ) }

blind_receipt wallet_api::receive_blind_transfer( string confirmation_receipt, string opt_from, string opt_memo )
{
   FC_ASSERT( !is_locked(), "Wallet is locked, please unlock to get all operations available" );
   stealth_confirmation conf(confirmation_receipt);
   FC_ASSERT( conf.to );

   blind_receipt result;
   result.conf = conf;

   auto to_priv_key_itr = my->_keys.find( *conf.to );
   FC_ASSERT( to_priv_key_itr != my->_keys.end(), "No private key for receiver", ("conf",conf) );


   auto to_priv_key = wif_to_key( to_priv_key_itr->second );
   FC_ASSERT( to_priv_key );

   auto secret       = to_priv_key->get_shared_secret( conf.one_time_key );
   auto child        = fc::sha256::hash( secret );

   auto child_priv_key = to_priv_key->child( child );
   //auto blind_factor = fc::sha256::hash( child );

   auto plain_memo = fc::aes_decrypt( secret, conf.encrypted_memo );
   auto memo = fc::raw::unpack<stealth_confirmation::memo_data>( plain_memo );

   result.to_key   = *conf.to;
   result.to_label = get_key_label( result.to_key );
   if( memo.from )
   {
      result.from_key = *memo.from;
      result.from_label = get_key_label( result.from_key );
      if( result.from_label == string() )
      {
         result.from_label = opt_from;
         set_key_label( result.from_key, result.from_label );
      }
   }
   else
   {
      result.from_label = opt_from;
   }
   result.amount = memo.amount;
   result.memo = opt_memo;

   // confirm the amount matches the commitment (verify the blinding factor)
   auto commtiment_test = fc::ecc::blind( memo.blinding_factor, memo.amount.amount.value );
   FC_ASSERT( fc::ecc::verify_sum( {commtiment_test}, {memo.commitment}, 0 ) );

   blind_balance bal;
   bal.amount = memo.amount;
   bal.to     = *conf.to;
   if( memo.from ) bal.from   = *memo.from;
   bal.one_time_key = conf.one_time_key;
   bal.blinding_factor = memo.blinding_factor;
   bal.commitment = memo.commitment;
   bal.used = false;

   auto child_pubkey = child_priv_key.get_public_key();
   auto owner = authority(1, public_key_type(child_pubkey), 1);
   result.control_authority = owner;
   result.data = memo;

   auto child_key_itr = owner.key_auths.find( child_pubkey );
   if( child_key_itr != owner.key_auths.end() )
      my->_keys[child_key_itr->first] = key_to_wif( child_priv_key );

   // my->_wallet.blinded_balances[memo.amount.asset_id][bal.to].push_back( bal );

   result.date = fc::time_point::now();
   my->_wallet.blind_receipts.insert( result );
   my->_keys[child_pubkey] = key_to_wif( child_priv_key );

   save_wallet_file();

   return result;
}

vector<blind_receipt> wallet_api::blind_history( string key_or_account )
{
   vector<blind_receipt> result;
   auto pub_key = get_public_key( key_or_account );

   if( pub_key == public_key_type() )
      return vector<blind_receipt>();

   for( auto& r : my->_wallet.blind_receipts )
   {
      if( r.from_key == pub_key || r.to_key == pub_key )
         result.push_back( r );
   }
   std::sort( result.begin(), result.end(), [&]( const blind_receipt& a, const blind_receipt& b ){ return a.date > b.date; } );
   return result;
}

///////////////
// peerplays //
///////////////

vector<sport_object> wallet_api::list_sports() const
{
    return my->_remote_db->list_sports();
}

vector<event_group_object> wallet_api::list_event_groups(sport_id_type sport_id) const
{
    return my->_remote_db->list_event_groups(sport_id);
}

vector<betting_market_group_object> wallet_api::list_betting_market_groups(event_id_type event_id) const
{
    return my->_remote_db->list_betting_market_groups(event_id);
}

vector<betting_market_object> wallet_api::list_betting_markets(betting_market_group_id_type betting_market_group_id) const
{
    return my->_remote_db->list_betting_markets(betting_market_group_id);
}

global_betting_statistics_object wallet_api::get_global_betting_statistics() const
{
    return my->_remote_db->get_global_betting_statistics();
}

vector<event_object>                wallet_api::list_events_in_group(event_group_id_type event_group_id) const
{
    return my->_remote_db->list_events_in_group(event_group_id);
}

vector<bet_object> wallet_api::get_unmatched_bets_for_bettor(betting_market_id_type betting_market_id, account_id_type account_id) const
{
    return my->_remote_db->get_unmatched_bets_for_bettor(betting_market_id, account_id);
}

vector<bet_object> wallet_api::get_all_unmatched_bets_for_bettor(account_id_type account_id) const
{
    return my->_remote_db->get_all_unmatched_bets_for_bettor(account_id);
}

signed_transaction wallet_api::propose_create_sport(
        const string& proposing_account,
        fc::time_point_sec expiration_time,
        internationalized_string_type name,
        bool broadcast /*= false*/)
{
   FC_ASSERT( !is_locked(), "Wallet is locked, please unlock to get all operations available" );
   const chain_parameters& current_params = get_global_properties().parameters;

   sport_create_operation sport_create_op;
   sport_create_op.name = name;

   proposal_create_operation prop_op;
   prop_op.expiration_time = expiration_time;
   prop_op.review_period_seconds = current_params.committee_proposal_review_period;
   prop_op.fee_paying_account = get_account(proposing_account).id;
   prop_op.proposed_ops.emplace_back( sport_create_op );
   current_params.current_fees->set_fee( prop_op.proposed_ops.back().op );

   signed_transaction tx;
   tx.operations.push_back(prop_op);
   my->set_operation_fees(tx, current_params.current_fees);
   tx.validate();

   return my->sign_transaction(tx, broadcast);
}

signed_transaction wallet_api::propose_update_sport(
        const string& proposing_account,
        fc::time_point_sec expiration_time,
        sport_id_type sport_id,
        fc::optional<internationalized_string_type> name,
        bool broadcast /*= false*/)
{
   FC_ASSERT( !is_locked() , "Wallet is locked, please unlock to get all operations available" );
   const chain_parameters& current_params = get_global_properties().parameters;

   sport_update_operation sport_update_op;
   sport_update_op.sport_id = sport_id;
   sport_update_op.new_name = name;

   proposal_create_operation prop_op;
   prop_op.expiration_time = expiration_time;
   prop_op.review_period_seconds = current_params.committee_proposal_review_period;
   prop_op.fee_paying_account = get_account(proposing_account).id;
   prop_op.proposed_ops.emplace_back( sport_update_op );
   current_params.current_fees->set_fee( prop_op.proposed_ops.back().op );

   signed_transaction tx;
   tx.operations.push_back(prop_op);
   my->set_operation_fees(tx, current_params.current_fees);
   tx.validate();

   return my->sign_transaction(tx, broadcast);
}

signed_transaction wallet_api::propose_delete_sport(
        const string& proposing_account,
        fc::time_point_sec expiration_time,
        sport_id_type sport_id,
        bool broadcast /*= false*/)
{
    FC_ASSERT( !is_locked() , "Wallet is locked, please unlock to get all operations available" );
    const chain_parameters& current_params = get_global_properties().parameters;

    sport_delete_operation sport_delete_op;
    sport_delete_op.sport_id = sport_id;

    proposal_create_operation prop_op;
    prop_op.expiration_time = expiration_time;
    prop_op.review_period_seconds = current_params.committee_proposal_review_period;
    prop_op.fee_paying_account = get_account(proposing_account).id;
    prop_op.proposed_ops.emplace_back( sport_delete_op );
    current_params.current_fees->set_fee( prop_op.proposed_ops.back().op );

    signed_transaction tx;
    tx.operations.push_back(prop_op);
    my->set_operation_fees(tx, current_params.current_fees);
    tx.validate();

    return my->sign_transaction(tx, broadcast);
}

signed_transaction wallet_api::propose_create_event_group(
        const string& proposing_account,
        fc::time_point_sec expiration_time,
        internationalized_string_type name,
        sport_id_type sport_id,
        bool broadcast /*= false*/)
{
    FC_ASSERT( !is_locked(), "Wallet is locked, please unlock to get all operations available" );
    const chain_parameters& current_params = get_global_properties().parameters;

    event_group_create_operation event_group_create_op;
    event_group_create_op.name = name;
    event_group_create_op.sport_id = sport_id;

    proposal_create_operation prop_op;
    prop_op.expiration_time = expiration_time;
    prop_op.review_period_seconds = current_params.committee_proposal_review_period;
    prop_op.fee_paying_account = get_account(proposing_account).id;
    prop_op.proposed_ops.emplace_back( event_group_create_op );
    current_params.current_fees->set_fee( prop_op.proposed_ops.back().op );

    signed_transaction tx;
    tx.operations.push_back(prop_op);
    my->set_operation_fees(tx, current_params.current_fees);
    tx.validate();

    return my->sign_transaction(tx, broadcast);
}

signed_transaction wallet_api::propose_update_event_group(
        const string& proposing_account,
        fc::time_point_sec expiration_time,
        event_group_id_type event_group,
        fc::optional<object_id_type> sport_id,
        fc::optional<internationalized_string_type> name,
        bool broadcast /*= false*/)
{
    FC_ASSERT( !is_locked() , "Wallet is locked, please unlock to get all operations available" );
    const chain_parameters& current_params = get_global_properties().parameters;

    event_group_update_operation event_group_update_op;
    event_group_update_op.new_sport_id = sport_id;
    event_group_update_op.new_name = name;
    event_group_update_op.event_group_id = event_group;

    proposal_create_operation prop_op;
    prop_op.expiration_time = expiration_time;
    prop_op.review_period_seconds = current_params.committee_proposal_review_period;
    prop_op.fee_paying_account = get_account(proposing_account).id;
    prop_op.proposed_ops.emplace_back( event_group_update_op );
    current_params.current_fees->set_fee( prop_op.proposed_ops.back().op );

    signed_transaction tx;
    tx.operations.push_back(prop_op);
    my->set_operation_fees(tx, current_params.current_fees);
    tx.validate();

    return my->sign_transaction(tx, broadcast);
}

signed_transaction wallet_api::propose_delete_event_group(
        const string& proposing_account,
        fc::time_point_sec expiration_time,
        event_group_id_type event_group,
        bool broadcast /*= false*/)
{
    FC_ASSERT( !is_locked() , "Wallet is locked, please unlock to get all operations available" );
    const chain_parameters& current_params = get_global_properties().parameters;

    event_group_delete_operation event_group_delete_op;
    event_group_delete_op.event_group_id = event_group;

    proposal_create_operation prop_op;
    prop_op.expiration_time = expiration_time;
    prop_op.review_period_seconds = current_params.committee_proposal_review_period;
    prop_op.fee_paying_account = get_account(proposing_account).id;
    prop_op.proposed_ops.emplace_back( event_group_delete_op );
    current_params.current_fees->set_fee( prop_op.proposed_ops.back().op );

    signed_transaction tx;
    tx.operations.push_back(prop_op);
    my->set_operation_fees(tx, current_params.current_fees);
    tx.validate();

    return my->sign_transaction(tx, broadcast);
}

signed_transaction wallet_api::propose_create_event(
        const string& proposing_account,
        fc::time_point_sec expiration_time,
        internationalized_string_type name,
        internationalized_string_type season,
        fc::optional<time_point_sec> start_time,
        event_group_id_type event_group_id,
        bool broadcast /*= false*/)
{
    FC_ASSERT( !is_locked() , "Wallet is locked, please unlock to get all operations available" );
    const chain_parameters& current_params = get_global_properties().parameters;

    event_create_operation event_create_op;
    event_create_op.start_time = start_time;
    event_create_op.name = name;
    event_create_op.season = season;
    event_create_op.event_group_id = event_group_id;

    proposal_create_operation prop_op;
    prop_op.expiration_time = expiration_time;
    prop_op.review_period_seconds = current_params.committee_proposal_review_period;
    prop_op.fee_paying_account = get_account(proposing_account).id;
    prop_op.proposed_ops.emplace_back( event_create_op );
    current_params.current_fees->set_fee( prop_op.proposed_ops.back().op );

    signed_transaction tx;
    tx.operations.push_back(prop_op);
    my->set_operation_fees(tx, current_params.current_fees);
    tx.validate();

    return my->sign_transaction(tx, broadcast);
}

signed_transaction wallet_api::propose_update_event(
        const string& proposing_account,
        fc::time_point_sec expiration_time,
        event_id_type event_id,
        fc::optional<object_id_type> event_group_id,
        fc::optional<internationalized_string_type> name,
        fc::optional<internationalized_string_type> season,
        fc::optional<event_status> status,
        fc::optional<time_point_sec> start_time,
        bool broadcast /*= false*/)
{
    FC_ASSERT( !is_locked() , "Wallet is locked, please unlock to get all operations available" );
    const chain_parameters& current_params = get_global_properties().parameters;

    event_update_operation event_update_op;
    event_update_op.event_id = event_id;
    event_update_op.new_event_group_id = event_group_id;
    event_update_op.new_start_time = start_time;
    event_update_op.new_name = name;
    event_update_op.new_season = season;
    event_update_op.new_status = status;

    proposal_create_operation prop_op;
    prop_op.expiration_time = expiration_time;
    prop_op.review_period_seconds = current_params.committee_proposal_review_period;
    prop_op.fee_paying_account = get_account(proposing_account).id;
    prop_op.proposed_ops.emplace_back( event_update_op );
    current_params.current_fees->set_fee( prop_op.proposed_ops.back().op );

    signed_transaction tx;
    tx.operations.push_back(prop_op);
    my->set_operation_fees(tx, current_params.current_fees);
    tx.validate();

    return my->sign_transaction(tx, broadcast);
}

signed_transaction wallet_api::propose_create_betting_market_rules(
        const string& proposing_account,
        fc::time_point_sec expiration_time,
        internationalized_string_type name,
        internationalized_string_type description,
        bool broadcast /*= false*/)
{
    FC_ASSERT( !is_locked() , "Wallet is locked, please unlock to get all operations available" );
    const chain_parameters& current_params = get_global_properties().parameters;

    betting_market_rules_create_operation betting_market_rules_create_op;
    betting_market_rules_create_op.name = name;
    betting_market_rules_create_op.description = description;

    proposal_create_operation prop_op;
    prop_op.expiration_time = expiration_time;
    prop_op.review_period_seconds = current_params.committee_proposal_review_period;
    prop_op.fee_paying_account = get_account(proposing_account).id;
    prop_op.proposed_ops.emplace_back( betting_market_rules_create_op );
    current_params.current_fees->set_fee( prop_op.proposed_ops.back().op );

    signed_transaction tx;
    tx.operations.push_back(prop_op);
    my->set_operation_fees(tx, current_params.current_fees);
    tx.validate();

    return my->sign_transaction(tx, broadcast);
}

signed_transaction wallet_api::propose_update_betting_market_rules(
        const string& proposing_account,
        fc::time_point_sec expiration_time,
        betting_market_rules_id_type rules_id,
        fc::optional<internationalized_string_type> name,
        fc::optional<internationalized_string_type> description,
        bool broadcast /*= false*/)
{
    FC_ASSERT( !is_locked() , "Wallet is locked, please unlock to get all operations available" );
    const chain_parameters& current_params = get_global_properties().parameters;

    betting_market_rules_update_operation betting_market_rules_update_op;
    betting_market_rules_update_op.betting_market_rules_id = rules_id;
    betting_market_rules_update_op.new_name = name;
    betting_market_rules_update_op.new_description = description;

    proposal_create_operation prop_op;
    prop_op.expiration_time = expiration_time;
    prop_op.review_period_seconds = current_params.committee_proposal_review_period;
    prop_op.fee_paying_account = get_account(proposing_account).id;
    prop_op.proposed_ops.emplace_back( betting_market_rules_update_op );
    current_params.current_fees->set_fee( prop_op.proposed_ops.back().op );

    signed_transaction tx;
    tx.operations.push_back(prop_op);
    my->set_operation_fees(tx, current_params.current_fees);
    tx.validate();

    return my->sign_transaction(tx, broadcast);
}

signed_transaction wallet_api::propose_create_betting_market_group(
        const string& proposing_account,
        fc::time_point_sec expiration_time,
        internationalized_string_type description,
        event_id_type event_id,
        betting_market_rules_id_type rules_id,
        asset_id_type asset_id,
        bool broadcast /*= false*/)
{
    FC_ASSERT( !is_locked() , "Wallet is locked, please unlock to get all operations available" );
    const chain_parameters& current_params = get_global_properties().parameters;

    betting_market_group_create_operation betting_market_group_create_op;
    betting_market_group_create_op.description = description;
    betting_market_group_create_op.event_id = event_id;
    betting_market_group_create_op.rules_id = rules_id;
    betting_market_group_create_op.asset_id = asset_id;

    proposal_create_operation prop_op;
    prop_op.expiration_time = expiration_time;
    prop_op.review_period_seconds = current_params.committee_proposal_review_period;
    prop_op.fee_paying_account = get_account(proposing_account).id;
    prop_op.proposed_ops.emplace_back( betting_market_group_create_op );
    current_params.current_fees->set_fee( prop_op.proposed_ops.back().op );

    signed_transaction tx;
    tx.operations.push_back(prop_op);
    my->set_operation_fees(tx, current_params.current_fees);
    tx.validate();

    return my->sign_transaction(tx, broadcast);
}

signed_transaction wallet_api::propose_update_betting_market_group(
        const string& proposing_account,
        fc::time_point_sec expiration_time,
        betting_market_group_id_type betting_market_group_id,
        fc::optional<internationalized_string_type> description,
        fc::optional<object_id_type> rules_id,
        fc::optional<betting_market_group_status> status,
        bool broadcast /*= false*/)
{
    FC_ASSERT( !is_locked() , "Wallet is locked, please unlock to get all operations available" );
    const chain_parameters& current_params = get_global_properties().parameters;

    betting_market_group_update_operation betting_market_group_update_op;
    betting_market_group_update_op.betting_market_group_id = betting_market_group_id;
    betting_market_group_update_op.new_description = description;
    betting_market_group_update_op.new_rules_id = rules_id;
    betting_market_group_update_op.status = status;

    proposal_create_operation prop_op;
    prop_op.expiration_time = expiration_time;
    prop_op.review_period_seconds = current_params.committee_proposal_review_period;
    prop_op.fee_paying_account = get_account(proposing_account).id;
    prop_op.proposed_ops.emplace_back( betting_market_group_update_op );
    current_params.current_fees->set_fee( prop_op.proposed_ops.back().op );

    signed_transaction tx;
    tx.operations.push_back(prop_op);
    my->set_operation_fees(tx, current_params.current_fees);
    tx.validate();

    return my->sign_transaction(tx, broadcast);
}

signed_transaction wallet_api::propose_create_betting_market(
        const string& proposing_account,
        fc::time_point_sec expiration_time,
        betting_market_group_id_type group_id,
        internationalized_string_type description,
        internationalized_string_type payout_condition,
        bool broadcast /*= false*/)
{
    FC_ASSERT( !is_locked() , "Wallet is locked, please unlock to get all operations available" );
    const chain_parameters& current_params = get_global_properties().parameters;

    betting_market_create_operation betting_market_create_op;
    betting_market_create_op.group_id = group_id;
    betting_market_create_op.description = description;
    betting_market_create_op.payout_condition = payout_condition;

    proposal_create_operation prop_op;
    prop_op.expiration_time = expiration_time;
    prop_op.review_period_seconds = current_params.committee_proposal_review_period;
    prop_op.fee_paying_account = get_account(proposing_account).id;
    prop_op.proposed_ops.emplace_back( betting_market_create_op );
    current_params.current_fees->set_fee( prop_op.proposed_ops.back().op );

    signed_transaction tx;
    tx.operations.push_back(prop_op);
    my->set_operation_fees(tx, current_params.current_fees);
    tx.validate();

    return my->sign_transaction(tx, broadcast);
}

signed_transaction wallet_api::propose_update_betting_market(
        const string& proposing_account,
        fc::time_point_sec expiration_time,
        betting_market_id_type market_id,
        fc::optional<object_id_type> group_id,
        fc::optional<internationalized_string_type> description,
        fc::optional<internationalized_string_type> payout_condition,
        bool broadcast /*= false*/)
{
    FC_ASSERT( !is_locked() , "Wallet is locked, please unlock to get all operations available" );
    const chain_parameters& current_params = get_global_properties().parameters;

    betting_market_update_operation betting_market_update_op;
    betting_market_update_op.betting_market_id = market_id;
    betting_market_update_op.new_group_id = group_id;
    betting_market_update_op.new_description = description;
    betting_market_update_op.new_payout_condition = payout_condition;

    proposal_create_operation prop_op;
    prop_op.expiration_time = expiration_time;
    prop_op.review_period_seconds = current_params.committee_proposal_review_period;
    prop_op.fee_paying_account = get_account(proposing_account).id;
    prop_op.proposed_ops.emplace_back( betting_market_update_op );
    current_params.current_fees->set_fee( prop_op.proposed_ops.back().op );

    signed_transaction tx;
    tx.operations.push_back(prop_op);
    my->set_operation_fees(tx, current_params.current_fees);
    tx.validate();

    return my->sign_transaction(tx, broadcast);
}

signed_transaction wallet_api::place_bet(string betting_account,
                                         betting_market_id_type betting_market_id,
                                         bet_type back_or_lay,
                                         string amount,
                                         string asset_symbol,
                                         double backer_multiplier,
                                         bool broadcast /*= false*/)
{
    FC_ASSERT( !is_locked(), "Wallet is locked, please unlock to get all operations available" );
    fc::optional<asset_object> asset_obj = get_asset(asset_symbol);
    FC_ASSERT(asset_obj, "Could not find asset matching ${asset}", ("asset", asset_symbol));

    const chain_parameters& current_params = get_global_properties().parameters;

    bet_place_operation bet_place_op;
    bet_place_op.bettor_id = get_account(betting_account).id;
    bet_place_op.betting_market_id = betting_market_id;
    bet_place_op.amount_to_bet = asset_obj->amount_from_string(amount);
    bet_place_op.backer_multiplier = (bet_multiplier_type)(backer_multiplier * GRAPHENE_BETTING_ODDS_PRECISION);
    bet_place_op.back_or_lay = back_or_lay;

    signed_transaction tx;
    tx.operations.push_back(bet_place_op);
    my->set_operation_fees(tx, current_params.current_fees);
    tx.validate();

    return my->sign_transaction(tx, broadcast);
}

signed_transaction wallet_api::cancel_bet(string betting_account,
                                          bet_id_type bet_id,
                                          bool broadcast /*= false*/)
{
    FC_ASSERT( !is_locked(), "Wallet is locked, please unlock to get all operations available" );

    const chain_parameters& current_params = get_global_properties().parameters;

    bet_cancel_operation bet_cancel_op;
    bet_cancel_op.bettor_id = get_account(betting_account).id;
    bet_cancel_op.bet_to_cancel = bet_id;

    signed_transaction tx;
    tx.operations.push_back(bet_cancel_op);
    my->set_operation_fees(tx, current_params.current_fees);
    tx.validate();

    return my->sign_transaction(tx, broadcast);
}

signed_transaction wallet_api::propose_resolve_betting_market_group(
        const string& proposing_account,
        fc::time_point_sec expiration_time,
        betting_market_group_id_type betting_market_group_id,
        const std::map<betting_market_id_type, betting_market_resolution_type>& resolutions,
        bool broadcast /*= false*/)
{
    FC_ASSERT( !is_locked(), "Wallet is locked, please unlock to get all operations available" );
    const chain_parameters& current_params = get_global_properties().parameters;

    betting_market_group_resolve_operation betting_market_group_resolve_op;
    betting_market_group_resolve_op.betting_market_group_id = betting_market_group_id;
    betting_market_group_resolve_op.resolutions = resolutions;

    proposal_create_operation prop_op;
    prop_op.expiration_time = expiration_time;
    prop_op.review_period_seconds = current_params.committee_proposal_review_period;
    prop_op.fee_paying_account = get_account(proposing_account).id;
    prop_op.proposed_ops.emplace_back( betting_market_group_resolve_op );
    current_params.current_fees->set_fee( prop_op.proposed_ops.back().op );

    signed_transaction tx;
    tx.operations.push_back(prop_op);
    my->set_operation_fees(tx, current_params.current_fees);
    tx.validate();

    return my->sign_transaction(tx, broadcast);
}

signed_transaction wallet_api::propose_cancel_betting_market_group(
        const string& proposing_account,
        fc::time_point_sec expiration_time,
        betting_market_group_id_type betting_market_group_id,
        bool broadcast /*= false*/)
{
    FC_ASSERT( !is_locked(), "Wallet is locked, please unlock to get all operations available" );
    const chain_parameters& current_params = get_global_properties().parameters;

    betting_market_group_cancel_unmatched_bets_operation betting_market_group_cancel_unmatched_bets_op;
    betting_market_group_cancel_unmatched_bets_op.betting_market_group_id = betting_market_group_id;

    proposal_create_operation prop_op;
    prop_op.expiration_time = expiration_time;
    prop_op.review_period_seconds = current_params.committee_proposal_review_period;
    prop_op.fee_paying_account = get_account(proposing_account).id;
    prop_op.proposed_ops.emplace_back( betting_market_group_cancel_unmatched_bets_op );
    current_params.current_fees->set_fee( prop_op.proposed_ops.back().op );

    signed_transaction tx;
    tx.operations.push_back(prop_op);
    my->set_operation_fees(tx, current_params.current_fees);
    tx.validate();

    return my->sign_transaction(tx, broadcast);
}

signed_transaction wallet_api::tournament_create( string creator, tournament_options options, bool broadcast )
{
   FC_ASSERT( !is_locked(), "Wallet is locked, please unlock to get all operations available" );
   account_object creator_account_obj = get_account(creator);

   signed_transaction tx;
   tournament_create_operation op;
   op.creator = creator_account_obj.get_id();
   op.options = options;
   tx.operations = {op};
   my->set_operation_fees( tx, my->_remote_db->get_global_properties().parameters.current_fees );
   tx.validate();

   return my->sign_transaction( tx, broadcast );
}

signed_transaction wallet_api::tournament_join( string payer_account,
                                                string player_account,
                                                tournament_id_type tournament_id,
                                                string buy_in_amount,
                                                string buy_in_asset_symbol,
                                                bool broadcast )
{
   FC_ASSERT( !is_locked(), "Wallet is locked, please unlock to get all operations available" );
   account_object payer_account_obj = get_account(payer_account);
   account_object player_account_obj = get_account(player_account);
   //graphene::chain::tournament_object tournament_obj = my->get_object<graphene::chain::tournament_object>(tournament_id);

   fc::optional<asset_object> buy_in_asset_obj = get_asset(buy_in_asset_symbol);
   FC_ASSERT(buy_in_asset_obj, "Could not find asset matching ${asset}", ("asset", buy_in_asset_symbol));

   signed_transaction tx;
   tournament_join_operation op;
   op.payer_account_id = payer_account_obj.get_id();
   op.player_account_id = player_account_obj.get_id();
   op.tournament_id = tournament_id;
   op.buy_in = buy_in_asset_obj->amount_from_string(buy_in_amount);

   tx.operations = {op};
   my->set_operation_fees( tx, my->_remote_db->get_global_properties().parameters.current_fees );
   tx.validate();

   return my->sign_transaction( tx, broadcast );
}

signed_transaction wallet_api::tournament_leave( string canceling_account,
                                                 string player_account,
                                                 tournament_id_type tournament_id,
                                                 bool broadcast)
{
    FC_ASSERT( !is_locked(), "Wallet is locked, please unlock to get all operations available" );
    account_object player_account_obj = get_account(player_account);
    account_object canceling_account_obj = get_account(canceling_account);
    //graphene::chain::tournament_object tournament_obj = my->get_object<graphene::chain::tournament_object>(tournament_id);

    signed_transaction tx;
    tournament_leave_operation op;
    op.canceling_account_id = canceling_account_obj.get_id();
    op.player_account_id = player_account_obj.get_id();
    op.tournament_id = tournament_id;

    tx.operations = {op};
    my->set_operation_fees( tx, my->_remote_db->get_global_properties().parameters.current_fees );
    tx.validate();

    return my->sign_transaction( tx, broadcast );
}

vector<tournament_object> wallet_api::get_upcoming_tournaments(uint32_t limit)
{
   return my->_remote_db->get_tournaments_in_state(tournament_state::accepting_registrations, limit);
}
vector<tournament_object> wallet_api::get_tournaments(tournament_id_type stop,
                                          unsigned limit,
                                          tournament_id_type start) {
   return my->_remote_db->get_tournaments(stop, limit, start);
}

vector<tournament_object> wallet_api::get_tournaments_by_state(tournament_id_type stop,
                                                   unsigned limit,
                                                   tournament_id_type start,
                                                   tournament_state state) {
   return my->_remote_db->get_tournaments_by_state(stop, limit, start, state);
}

tournament_object wallet_api::get_tournament(tournament_id_type id)
{
   return my->_remote_db->get_objects({id})[0].as<tournament_object>( GRAPHENE_MAX_NESTED_OBJECTS );
}

signed_transaction wallet_api::rps_throw(game_id_type game_id,
                                         string player_account,
                                         rock_paper_scissors_gesture gesture,
                                         bool broadcast)
{
   FC_ASSERT( !is_locked(), "Wallet is locked, please unlock to get all operations available" );

   // check whether the gesture is appropriate for the game we're playing
   graphene::chain::game_object game_obj = my->get_object<graphene::chain::game_object>(game_id);
   graphene::chain::match_object match_obj = my->get_object<graphene::chain::match_object>(game_obj.match_id);
   graphene::chain::tournament_object tournament_obj = my->get_object<graphene::chain::tournament_object>(match_obj.tournament_id);
   graphene::chain::rock_paper_scissors_game_options game_options =
      tournament_obj.options.game_options.get<graphene::chain::rock_paper_scissors_game_options>();
   if ((int)gesture >= game_options.number_of_gestures)
      FC_THROW("Gesture ${gesture} not supported in this game", ("gesture", gesture));

   account_object player_account_obj = get_account(player_account);

   // construct the complete throw, the commit, and reveal
   rock_paper_scissors_throw full_throw;
   std::random_device rd;
   std::mt19937_64 gen(rd());
   std::uniform_int_distribution<uint64_t> dis;
   full_throw.nonce1 = dis(gen);
   full_throw.nonce2 = dis(gen);

   full_throw.gesture = gesture;

   rock_paper_scissors_throw_commit commit_throw;
   commit_throw.nonce1 = full_throw.nonce1;
   std::vector<char> full_throw_packed(fc::raw::pack(full_throw));
   commit_throw.throw_hash = fc::sha256::hash(full_throw_packed.data(), full_throw_packed.size());

   rock_paper_scissors_throw_reveal reveal_throw;
   reveal_throw.nonce2 = full_throw.nonce2;
   reveal_throw.gesture = full_throw.gesture;

   // store off the reveal for transmitting after both players commit
   my->_wallet.committed_game_moves[commit_throw] = reveal_throw;

   // broadcast the commit
   signed_transaction tx;
   game_move_operation move_operation;
   move_operation.game_id = game_id;
   move_operation.player_account_id = player_account_obj.id;
   move_operation.move = commit_throw;
   tx.operations = {move_operation};
   my->set_operation_fees( tx, my->_remote_db->get_global_properties().parameters.current_fees );
   tx.validate();

   return my->sign_transaction( tx, broadcast );
}

signed_transaction wallet_api::nft_metadata_create(string owner_account_id_or_name,
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
                                                   bool broadcast)
{
   account_object owner_account = my->get_account(owner_account_id_or_name);

   nft_metadata_create_operation op;
   op.owner = owner_account.id;
   op.name = name;
   op.symbol = symbol;
   op.base_uri = base_uri;
   if( revenue_partner )
   {
      account_object partner_account = my->get_account(*revenue_partner);
      op.revenue_partner = partner_account.id;
      uint16_t rev_split = 0;
      if( revenue_split )
      {
         rev_split = *revenue_split;
      }
      op.revenue_split = rev_split;
   }
   op.is_transferable = is_transferable;
   op.is_sellable = is_sellable;
   op.account_role = role_id;
   op.max_supply = max_supply;
   op.lottery_options = lottery_options;

   signed_transaction trx;
   trx.operations.push_back(op);
   my->set_operation_fees( trx, my->_remote_db->get_global_properties().parameters.current_fees );
   trx.validate();

   return my->sign_transaction( trx, broadcast );
}

signed_transaction wallet_api::nft_metadata_update(string owner_account_id_or_name,
                                                   nft_metadata_id_type nft_metadata_id,
                                                   optional<string> name,
                                                   optional<string> symbol,
                                                   optional<string> base_uri,
                                                   optional<string> revenue_partner,
                                                   optional<uint16_t> revenue_split,
                                                   optional<bool> is_transferable,
                                                   optional<bool> is_sellable,
                                                   optional<account_role_id_type> role_id,
                                                   bool broadcast)
{
   account_object owner_account = my->get_account(owner_account_id_or_name);

   nft_metadata_update_operation op;
   op.nft_metadata_id = nft_metadata_id;
   op.owner = owner_account.id;
   op.name = name;
   op.symbol = symbol;
   op.base_uri = base_uri;
   if( revenue_partner )
   {
      account_object partner_account = my->get_account(*revenue_partner);
      op.revenue_partner = partner_account.id;
      uint16_t rev_split = 0;
      if( revenue_split )
      {
         rev_split = *revenue_split;
      }
      op.revenue_split = rev_split;
   }
   op.is_transferable = is_transferable;
   op.is_sellable = is_sellable;
   op.account_role = role_id;

   signed_transaction trx;
   trx.operations.push_back(op);
   my->set_operation_fees( trx, my->_remote_db->get_global_properties().parameters.current_fees );
   trx.validate();

   return my->sign_transaction( trx, broadcast );
}

signed_transaction wallet_api::nft_create(string metadata_owner_account_id_or_name,
                                          nft_metadata_id_type metadata_id,
                                          string owner_account_id_or_name,
                                          string approved_account_id_or_name,
                                          string token_uri,
                                          bool broadcast)
{
   account_object metadata_owner_account = my->get_account(metadata_owner_account_id_or_name);
   account_object owner_account = my->get_account(owner_account_id_or_name);
   account_object approved_account = my->get_account(approved_account_id_or_name);

   nft_mint_operation op;
   op.payer = metadata_owner_account.id;
   op.nft_metadata_id = metadata_id;
   op.owner = owner_account.id;
   op.approved = approved_account.id;
   op.token_uri = token_uri;

   signed_transaction trx;
   trx.operations.push_back(op);
   my->set_operation_fees( trx, my->_remote_db->get_global_properties().parameters.current_fees );
   trx.validate();

   return my->sign_transaction( trx, broadcast );
}

uint64_t wallet_api::nft_get_balance(string owner_account_id_or_name) const
{
   account_object owner_account = my->get_account(owner_account_id_or_name);
   return my->_remote_db->nft_get_balance(owner_account.id);
}

optional<account_id_type> wallet_api::nft_owner_of(const nft_id_type token_id) const
{
   return my->_remote_db->nft_owner_of(token_id);
}

signed_transaction wallet_api::nft_safe_transfer_from(string operator_account_id_or_name,
                                                      string from_account_id_or_name,
                                                      string to_account_id_or_name,
                                                      nft_id_type token_id,
                                                      string data,
                                                      bool broadcast)
{
   account_object operator_account = my->get_account(operator_account_id_or_name);
   account_object from_account = my->get_account(from_account_id_or_name);
   account_object to_account = my->get_account(to_account_id_or_name);

   nft_safe_transfer_from_operation op;
   op.operator_ = operator_account.id;
   op.from = from_account.id;
   op.to = to_account.id;
   op.token_id = token_id;
   op.data = data;

   signed_transaction trx;
   trx.operations.push_back(op);
   my->set_operation_fees( trx, my->_remote_db->get_global_properties().parameters.current_fees );
   trx.validate();

   return my->sign_transaction( trx, broadcast );
}

signed_transaction wallet_api::nft_transfer_from(string operator_account_id_or_name,
                                                 string from_account_id_or_name,
                                                 string to_account_id_or_name,
                                                 nft_id_type token_id,
                                                 bool broadcast)
{
   return nft_safe_transfer_from(operator_account_id_or_name, from_account_id_or_name, to_account_id_or_name, token_id, "", broadcast);
}

signed_transaction wallet_api::nft_approve(string operator_account_id_or_name,
                                           string approved_account_id_or_name,
                                           nft_id_type token_id,
                                           bool broadcast)
{
   account_object operator_account = my->get_account(operator_account_id_or_name);
   account_object approved_account = my->get_account(approved_account_id_or_name);

   nft_approve_operation op;
   op.operator_ = operator_account.id;
   op.approved = approved_account.id;
   op.token_id = token_id;

   signed_transaction trx;
   trx.operations.push_back(op);
   my->set_operation_fees( trx, my->_remote_db->get_global_properties().parameters.current_fees );
   trx.validate();

   return my->sign_transaction( trx, broadcast );
}

signed_transaction wallet_api::nft_set_approval_for_all(string owner_account_id_or_name,
                                                        string operator_account_id_or_name,
                                                        bool approved,
                                                        bool broadcast)
{
   account_object owner_account = my->get_account(owner_account_id_or_name);
   account_object operator_account = my->get_account(operator_account_id_or_name);

   nft_set_approval_for_all_operation op;
   op.owner = owner_account.id;
   op.operator_ = operator_account.id;
   op.approved = approved;

   signed_transaction trx;
   trx.operations.push_back(op);
   my->set_operation_fees( trx, my->_remote_db->get_global_properties().parameters.current_fees );
   trx.validate();

   return my->sign_transaction( trx, broadcast );
}

optional<account_id_type> wallet_api::nft_get_approved(const nft_id_type token_id) const
{
   return my->_remote_db->nft_get_approved(token_id);
}

bool wallet_api::nft_is_approved_for_all(string owner_account_id_or_name, string operator_account_id_or_name) const
{
   account_object owner_account = my->get_account(owner_account_id_or_name);
   account_object operator_account = my->get_account(operator_account_id_or_name);
   return my->_remote_db->nft_is_approved_for_all(owner_account.id, operator_account.id);
}

vector<nft_object> wallet_api::nft_get_all_tokens() const
{
   return my->_remote_db->nft_get_all_tokens();
}

signed_transaction wallet_api::nft_lottery_buy_ticket( nft_metadata_id_type lottery, account_id_type buyer, uint64_t tickets_to_buy, bool broadcast )
{
   nft_lottery_token_purchase_operation op;
   op.lottery_id = lottery;
   op.buyer = buyer;
   op.tickets_to_buy = tickets_to_buy;

   signed_transaction trx;
   trx.operations.push_back(op);
   my->set_operation_fees( trx, my->_remote_db->get_global_properties().parameters.current_fees );
   trx.validate();

   return my->sign_transaction( trx, broadcast );
}

signed_transaction wallet_api::create_offer(set<nft_id_type> item_ids,
                                             string issuer_accound_id_or_name,
                                             asset minimum_price,
                                             asset maximum_price,
                                             bool buying_item,
                                             time_point_sec offer_expiration_date,
                                             optional<memo_data> memo,
                                             bool broadcast)
{
   account_object issuer_account = my->get_account(issuer_accound_id_or_name);

   offer_operation op;
   op.item_ids = item_ids;
   op.issuer = issuer_account.id;
   op.minimum_price = minimum_price;
   op.maximum_price = maximum_price;
   op.buying_item = buying_item;
   op.offer_expiration_date = offer_expiration_date;
   op.memo = memo;

   signed_transaction trx;
   trx.operations.push_back(op);
   my->set_operation_fees( trx, my->_remote_db->get_global_properties().parameters.current_fees );
   trx.validate();

   return my->sign_transaction( trx, broadcast );
}

signed_transaction wallet_api::create_bid(string bidder_account_id_or_name,
                                          asset bid_price,
                                          offer_id_type offer_id,
                                          bool broadcast)
{
   account_object bidder_account = my->get_account(bidder_account_id_or_name);

   bid_operation op;
   op.bidder = bidder_account.id;
   op.offer_id = offer_id;
   op.bid_price = bid_price;

   signed_transaction trx;
   trx.operations.push_back(op);
   my->set_operation_fees( trx, my->_remote_db->get_global_properties().parameters.current_fees );
   trx.validate();

   return my->sign_transaction( trx, broadcast );
}

signed_transaction wallet_api::cancel_offer(string issuer_account_id_or_name,
                                            offer_id_type offer_id,
                                            bool broadcast)
{
   account_object issuer_account = my->get_account(issuer_account_id_or_name);

   cancel_offer_operation op;
   op.issuer = issuer_account.id;
   op.offer_id = offer_id;

   signed_transaction trx;
   trx.operations.push_back(op);
   my->set_operation_fees( trx, my->_remote_db->get_global_properties().parameters.current_fees );
   trx.validate();

   return my->sign_transaction( trx, broadcast );
}

vector<offer_object> wallet_api::list_offers(uint32_t limit, optional<offer_id_type> lower_id) const
{
   offer_id_type lb_id;
   if(lower_id)
      lb_id = *lower_id;
   return my->_remote_db->list_offers(lb_id, limit);
}

vector<offer_object> wallet_api::list_sell_offers(uint32_t limit, optional<offer_id_type> lower_id) const
{
   offer_id_type lb_id;
   if(lower_id)
      lb_id = *lower_id;
   return my->_remote_db->list_sell_offers(lb_id, limit);
}

vector<offer_object> wallet_api::list_buy_offers(uint32_t limit, optional<offer_id_type> lower_id) const
{
   offer_id_type lb_id;
   if(lower_id)
      lb_id = *lower_id;
   return my->_remote_db->list_buy_offers(lb_id, limit);
}

vector<offer_history_object> wallet_api::list_offer_history(uint32_t limit, optional<offer_history_id_type> lower_id) const
{
   offer_history_id_type lb_id;
   if(lower_id)
      lb_id = *lower_id;
   return my->_remote_db->list_offer_history(lb_id, limit);
}

vector<offer_object> wallet_api::get_offers_by_issuer(string issuer_account_id_or_name,
                                                      uint32_t limit, optional<offer_id_type> lower_id) const
{
   offer_id_type lb_id;
   if(lower_id)
      lb_id = *lower_id;
   account_object issuer_account = my->get_account(issuer_account_id_or_name);
   return my->_remote_db->get_offers_by_issuer(lb_id, issuer_account.id, limit);
}

vector<offer_object> wallet_api::get_offers_by_item(const nft_id_type item, uint32_t limit, optional<offer_id_type> lower_id) const
{
   offer_id_type lb_id;
   if(lower_id)
      lb_id = *lower_id;
   return my->_remote_db->get_offers_by_item(lb_id, item, limit);
}

vector<offer_history_object> wallet_api::get_offer_history_by_issuer(string issuer_account_id_or_name, uint32_t limit, optional<offer_history_id_type> lower_id) const
{
   offer_history_id_type lb_id;
   if(lower_id)
      lb_id = *lower_id;
   account_object issuer_account = my->get_account(issuer_account_id_or_name);
   return my->_remote_db->get_offer_history_by_issuer(lb_id, issuer_account.id, limit);
}

vector<offer_history_object> wallet_api::get_offer_history_by_item(const nft_id_type item, uint32_t limit, optional<offer_history_id_type> lower_id) const
{
   offer_history_id_type lb_id;
   if(lower_id)
      lb_id = *lower_id;
   return my->_remote_db->get_offer_history_by_item(lb_id, item, limit);
}

vector<offer_history_object> wallet_api::get_offer_history_by_bidder(string bidder_account_id_or_name, uint32_t limit, optional<offer_history_id_type> lower_id) const
{
   offer_history_id_type lb_id;
   if(lower_id)
      lb_id = *lower_id;
   account_object bidder_account = my->get_account(bidder_account_id_or_name);
   return my->_remote_db->get_offer_history_by_bidder(lb_id, bidder_account.id, limit);
}

signed_transaction wallet_api::create_account_role(string owner_account_id_or_name,
                                                   string name,
                                                   string metadata,
                                                   flat_set<int> allowed_operations,
                                                   flat_set<account_id_type> whitelisted_accounts,
                                                   time_point_sec valid_to,
                                                   bool broadcast)
{
   account_object owner_account = my->get_account(owner_account_id_or_name);

   account_role_create_operation op;
   op.owner = owner_account.id;
   op.name = name;
   op.metadata = metadata;
   op.allowed_operations = allowed_operations;
   op.whitelisted_accounts = whitelisted_accounts;
   op.valid_to = valid_to;

   signed_transaction trx;
   trx.operations.push_back(op);
   my->set_operation_fees( trx, my->_remote_db->get_global_properties().parameters.current_fees );
   trx.validate();

   return my->sign_transaction( trx, broadcast );
}

signed_transaction wallet_api::update_account_role(string owner_account_id_or_name,
                                                   account_role_id_type role_id,
                                                   optional<string> name,
                                                   optional<string> metadata,
                                                   flat_set<int> operations_to_add,
                                                   flat_set<int> operations_to_remove,
                                                   flat_set<account_id_type> accounts_to_add,
                                                   flat_set<account_id_type> accounts_to_remove,
                                                   optional<time_point_sec> valid_to,
                                                   bool broadcast)
{
   account_object owner_account = my->get_account(owner_account_id_or_name);

   account_role_update_operation op;
   op.owner = owner_account.id;
   op.account_role_id = role_id;
   op.name = name;
   op.metadata = metadata;
   op.allowed_operations_to_add = operations_to_add;
   op.allowed_operations_to_remove = operations_to_remove;
   op.accounts_to_add = accounts_to_add;
   op.accounts_to_remove = accounts_to_remove;
   op.valid_to = valid_to;

   signed_transaction trx;
   trx.operations.push_back(op);
   my->set_operation_fees( trx, my->_remote_db->get_global_properties().parameters.current_fees );
   trx.validate();

   return my->sign_transaction( trx, broadcast );
}

signed_transaction wallet_api::delete_account_role(string owner_account_id_or_name,
                                                   account_role_id_type role_id,
                                                   bool broadcast)
{
   account_object owner_account = my->get_account(owner_account_id_or_name);

   account_role_delete_operation op;
   op.owner = owner_account.id;
   op.account_role_id = role_id;

   signed_transaction trx;
   trx.operations.push_back(op);
   my->set_operation_fees( trx, my->_remote_db->get_global_properties().parameters.current_fees );
   trx.validate();

   return my->sign_transaction( trx, broadcast );
}

vector<account_role_object> wallet_api::get_account_roles_by_owner(string owner_account_id_or_name) const
{
   account_object owner_account = my->get_account(owner_account_id_or_name);
   return my->_remote_db->get_account_roles_by_owner(owner_account.id);
}
// default ctor necessary for FC_REFLECT
signed_block_with_info::signed_block_with_info()
{
}

order_book wallet_api::get_order_book( const string& base, const string& quote, unsigned limit )
{
   return( my->_remote_db->get_order_book( base, quote, limit ) );
}

asset wallet_api::get_total_matched_bet_amount_for_betting_market_group(betting_market_group_id_type group_id)
{
    return( my->_remote_bookie->get_total_matched_bet_amount_for_betting_market_group(group_id) );
}

std::vector<event_object> wallet_api::get_events_containing_sub_string(const std::string& sub_string, const std::string& language)
{
    return( my->_remote_bookie->get_events_containing_sub_string(sub_string, language) );
}

binned_order_book wallet_api::get_binned_order_book(graphene::chain::betting_market_id_type betting_market_id, int32_t precision)
{
    return( my->_remote_bookie->get_binned_order_book(betting_market_id, precision) );
}

std::vector<matched_bet_object> wallet_api::get_matched_bets_for_bettor(account_id_type bettor_id) const
{
    return( my->_remote_bookie->get_matched_bets_for_bettor(bettor_id) );
}

std::vector<matched_bet_object> wallet_api::get_all_matched_bets_for_bettor(account_id_type bettor_id, bet_id_type start, unsigned limit) const
{
    return( my->_remote_bookie->get_all_matched_bets_for_bettor(bettor_id, start, limit) );
}

vector<vote_id_type> wallet_api::get_votes_ids(const string &account_name_or_id) const
{
   return my->get_votes_ids(account_name_or_id);
}

votes_info wallet_api::get_votes(const string &account_name_or_id) const
{
   return my->get_votes(account_name_or_id);
}

vector<account_object> wallet_api::get_voters_by_id(const vote_id_type &vote_id) const
{
   return my->get_voters_by_id(vote_id);
}

voters_info wallet_api::get_voters(const string &account_name_or_id) const
{
   return my->get_voters(account_name_or_id);
}

std::map<sidechain_type, std::vector<std::string>> wallet_api::get_son_listener_log() const
{
   return my->get_son_listener_log();
}

// default ctor necessary for FC_REFLECT
signed_block_with_info::signed_block_with_info( const signed_block& block )
   : signed_block( block )
{
   block_id = id();
   signing_key = signee();
   transaction_ids.reserve( transactions.size() );
   for( const processed_transaction& tx : transactions )
      transaction_ids.push_back( tx.id() );
}

vesting_balance_object_with_info::vesting_balance_object_with_info()
   : vesting_balance_object()
{
}

vesting_balance_object_with_info::vesting_balance_object_with_info( const vesting_balance_object& vbo, fc::time_point_sec now )
   : vesting_balance_object( vbo )
{
   allowed_withdraw = get_allowed_withdraw( now );
   if(vbo.balance_type == vesting_balance_type::gpos ||
      ((vbo.balance_type == vesting_balance_type::son) && (vbo.policy.which() == vesting_policy::tag<linear_vesting_policy>::value)))
      allowed_withdraw_time = vbo.policy.get<linear_vesting_policy>().begin_timestamp + vbo.policy.get<linear_vesting_policy>().vesting_cliff_seconds;
   else
      allowed_withdraw_time = now;
}

} } // graphene::wallet

namespace fc {
   void to_variant( const account_multi_index_type& accts, variant& vo, uint32_t max_depth )
   {
      to_variant( std::vector<account_object>(accts.begin(), accts.end()), vo, max_depth );
   }

   void from_variant( const variant& var, account_multi_index_type& vo, uint32_t max_depth )
   {
      const std::vector<account_object>& v = var.as<std::vector<account_object>>( max_depth );
      vo = account_multi_index_type(v.begin(), v.end());
   }
}
