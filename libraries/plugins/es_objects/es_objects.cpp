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

#include <graphene/es_objects/es_objects.hpp>

#include <curl/curl.h>
#include <graphene/chain/proposal_object.hpp>
#include <graphene/chain/balance_object.hpp>
#include <graphene/chain/market_object.hpp>
#include <graphene/chain/asset_object.hpp>
#include <graphene/chain/account_object.hpp>

#include <graphene/chain/account_role_object.hpp>
#include <graphene/chain/committee_member_object.hpp>
#include <graphene/chain/nft_object.hpp>
#include <graphene/chain/offer_object.hpp>
#include <graphene/chain/sidechain_address_object.hpp>
#include <graphene/chain/sidechain_transaction_object.hpp>
#include <graphene/chain/son_object.hpp>
#include <graphene/chain/son_proposal_object.hpp>
#include <graphene/chain/son_wallet_object.hpp>
#include <graphene/chain/son_wallet_deposit_object.hpp>
#include <graphene/chain/son_wallet_withdraw_object.hpp>
#include <graphene/chain/transaction_object.hpp>
#include <graphene/chain/vesting_balance_object.hpp>
#include <graphene/chain/witness_object.hpp>
#include <graphene/chain/worker_object.hpp>

#include <graphene/utilities/elasticsearch.hpp>

namespace graphene { namespace es_objects {

namespace detail
{

class es_objects_plugin_impl
{
   public:
      es_objects_plugin_impl(es_objects_plugin& _plugin)
         : _self( _plugin )
      {  curl = curl_easy_init(); }
      virtual ~es_objects_plugin_impl();

      bool index_database(const vector<object_id_type>& ids, std::string action);
      bool genesis();
      void remove_from_database(object_id_type id, std::string index);

      es_objects_plugin& _self;
      std::string _es_objects_elasticsearch_url = "http://localhost:9200/";
      std::string _es_objects_auth = "";
      uint32_t _es_objects_bulk_replay = 10000;
      uint32_t _es_objects_bulk_sync = 100;
      bool _es_objects_proposals = true;
      bool _es_objects_accounts = true;
      bool _es_objects_assets = true;
      bool _es_objects_balances = true;
      bool _es_objects_limit_orders = true;
      bool _es_objects_asset_bitasset = true;

      bool _es_objects_account_role = true;
      bool _es_objects_committee_member = true;
      bool _es_objects_nft = true;
      bool _es_objects_son = true;
      bool _es_objects_transaction = true;
      bool _es_objects_vesting_balance = true;
      bool _es_objects_witness = true;
      bool _es_objects_worker = true;

      std::string _es_objects_index_prefix = "ppobjects-";
      uint32_t _es_objects_start_es_after_block = 0;
      CURL *curl; // curl handler
      vector <std::string> bulk;
      vector<std::string> prepare;

      bool _es_objects_keep_only_current = true;

      uint32_t block_number;
      fc::time_point_sec block_time;

   private:
      template<typename T>
      void prepareTemplate(T blockchain_object, string index_name);
};

bool es_objects_plugin_impl::genesis()
{
   ilog("elasticsearch OBJECTS: inserting data from genesis");

   graphene::chain::database &db = _self.database();

   block_number = db.head_block_num();
   block_time = db.head_block_time();

   if (_es_objects_accounts) {
      auto &index_accounts = db.get_index(1, 2);
      index_accounts.inspect_all_objects([this, &db](const graphene::db::object &o) {
         auto obj = db.find_object(o.id);
         auto a = static_cast<const account_object *>(obj);
         prepareTemplate<account_object>(*a, "account");
      });
   }
   if (_es_objects_assets) {
      auto &index_assets = db.get_index(1, 3);
      index_assets.inspect_all_objects([this, &db](const graphene::db::object &o) {
         auto obj = db.find_object(o.id);
         auto a = static_cast<const asset_object *>(obj);
         prepareTemplate<asset_object>(*a, "asset");
      });
   }
   if (_es_objects_balances) {
      auto &index_balances = db.get_index(2, 5);
      index_balances.inspect_all_objects([this, &db](const graphene::db::object &o) {
         auto obj = db.find_object(o.id);
         auto b = static_cast<const account_balance_object *>(obj);
         prepareTemplate<account_balance_object>(*b, "balance");
      });
   }

   if (_es_objects_account_role) {
      auto &idx = db.get_index_type<graphene::chain::account_role_index>();
      idx.inspect_all_objects([this, &db](const graphene::db::object &o) {
         auto obj = db.find_object(o.id);
         auto b = static_cast<const account_role_object *>(obj);
         prepareTemplate<account_role_object>(*b, "account_role");
      });
   }
   if (_es_objects_committee_member) {
      auto &idx = db.get_index_type<graphene::chain::committee_member_index>();
      idx.inspect_all_objects([this, &db](const graphene::db::object &o) {
         auto obj = db.find_object(o.id);
         auto b = static_cast<const committee_member_object *>(obj);
         prepareTemplate<committee_member_object>(*b, "committee_member");
      });
   }
   if (_es_objects_nft) {
      auto &idx = db.get_index_type<graphene::chain::nft_index>();
      idx.inspect_all_objects([this, &db](const graphene::db::object &o) {
         auto obj = db.find_object(o.id);
         auto b = static_cast<const nft_object *>(obj);
         prepareTemplate<nft_object>(*b, "nft");
      });
   }
   if (_es_objects_nft) {
      auto &idx = db.get_index_type<graphene::chain::nft_metadata_index>();
      idx.inspect_all_objects([this, &db](const graphene::db::object &o) {
         auto obj = db.find_object(o.id);
         auto b = static_cast<const nft_metadata_object *>(obj);
         prepareTemplate<nft_metadata_object>(*b, "nft_metadata");
      });
   }
   if (_es_objects_nft) {
      auto &idx = db.get_index_type<graphene::chain::offer_index>();
      idx.inspect_all_objects([this, &db](const graphene::db::object &o) {
         auto obj = db.find_object(o.id);
         auto b = static_cast<const offer_object *>(obj);
         prepareTemplate<offer_object>(*b, "offer");
      });
   }
   if (_es_objects_son) {
      auto &idx = db.get_index_type<graphene::chain::sidechain_address_index>();
      idx.inspect_all_objects([this, &db](const graphene::db::object &o) {
         auto obj = db.find_object(o.id);
         auto b = static_cast<const sidechain_address_object *>(obj);
         prepareTemplate<sidechain_address_object>(*b, "sidechain_address");
      });
   }
   if (_es_objects_son) {
      auto &idx = db.get_index_type<graphene::chain::sidechain_transaction_index>();
      idx.inspect_all_objects([this, &db](const graphene::db::object &o) {
         auto obj = db.find_object(o.id);
         auto b = static_cast<const sidechain_transaction_object *>(obj);
         prepareTemplate<sidechain_transaction_object>(*b, "sidechain_transaction");
      });
   }
   if (_es_objects_son) {
      auto &idx = db.get_index_type<graphene::chain::son_index>();
      idx.inspect_all_objects([this, &db](const graphene::db::object &o) {
         auto obj = db.find_object(o.id);
         auto b = static_cast<const son_object *>(obj);
         prepareTemplate<son_object>(*b, "son");
      });
   }
   if (_es_objects_son) {
      auto &idx = db.get_index_type<graphene::chain::son_proposal_index>();
      idx.inspect_all_objects([this, &db](const graphene::db::object &o) {
         auto obj = db.find_object(o.id);
         auto b = static_cast<const son_proposal_object *>(obj);
         prepareTemplate<son_proposal_object>(*b, "son_proposal");
      });
   }
   if (_es_objects_son) {
      auto &idx = db.get_index_type<graphene::chain::son_wallet_index>();
      idx.inspect_all_objects([this, &db](const graphene::db::object &o) {
         auto obj = db.find_object(o.id);
         auto b = static_cast<const son_wallet_object *>(obj);
         prepareTemplate<son_wallet_object>(*b, "son_wallet");
      });
   }
   if (_es_objects_son) {
      auto &idx = db.get_index_type<graphene::chain::son_wallet_deposit_index>();
      idx.inspect_all_objects([this, &db](const graphene::db::object &o) {
         auto obj = db.find_object(o.id);
         auto b = static_cast<const son_wallet_deposit_object *>(obj);
         prepareTemplate<son_wallet_deposit_object>(*b, "son_wallet_deposit");
      });
   }
   if (_es_objects_son) {
      auto &idx = db.get_index_type<graphene::chain::son_wallet_withdraw_index>();
      idx.inspect_all_objects([this, &db](const graphene::db::object &o) {
         auto obj = db.find_object(o.id);
         auto b = static_cast<const son_wallet_withdraw_object *>(obj);
         prepareTemplate<son_wallet_withdraw_object>(*b, "son_wallet_withdraw");
      });
   }
   if (_es_objects_transaction) {
      auto &idx = db.get_index_type<graphene::chain::transaction_index>();
      idx.inspect_all_objects([this, &db](const graphene::db::object &o) {
         auto obj = db.find_object(o.id);
         auto b = static_cast<const transaction_object *>(obj);
         prepareTemplate<transaction_object>(*b, "transaction");
      });
   }
   if (_es_objects_vesting_balance) {
      auto &idx = db.get_index_type<graphene::chain::vesting_balance_index>();
      idx.inspect_all_objects([this, &db](const graphene::db::object &o) {
         auto obj = db.find_object(o.id);
         auto b = static_cast<const vesting_balance_object *>(obj);
         prepareTemplate<vesting_balance_object>(*b, "vesting_balance");
      });
   }
   if (_es_objects_witness) {
      auto &idx = db.get_index_type<graphene::chain::witness_index>();
      idx.inspect_all_objects([this, &db](const graphene::db::object &o) {
         auto obj = db.find_object(o.id);
         auto b = static_cast<const witness_object *>(obj);
         prepareTemplate<witness_object>(*b, "witness");
      });
   }
   if (_es_objects_worker) {
      auto &idx = db.get_index_type<graphene::chain::worker_index>();
      idx.inspect_all_objects([this, &db](const graphene::db::object &o) {
         auto obj = db.find_object(o.id);
         auto b = static_cast<const worker_object *>(obj);
         prepareTemplate<worker_object>(*b, "worker");
      });
   }

   graphene::utilities::ES es;
   es.curl = curl;
   es.bulk_lines = bulk;
   es.elasticsearch_url = _es_objects_elasticsearch_url;
   es.auth = _es_objects_auth;
   if (!graphene::utilities::SendBulk(es))
      FC_THROW_EXCEPTION(graphene::chain::plugin_exception, "Error inserting genesis data.");
   else
      bulk.clear();

   return true;
}

bool es_objects_plugin_impl::index_database(const vector<object_id_type>& ids, std::string action)
{
   graphene::chain::database &db = _self.database();

   block_time = db.head_block_time();
   block_number = db.head_block_num();

   if(block_number > _es_objects_start_es_after_block) {

      // check if we are in replay or in sync and change number of bulk documents accordingly
      uint32_t limit_documents = 0;
      if ((fc::time_point::now() - block_time) < fc::seconds(30))
         limit_documents = _es_objects_bulk_sync;
      else
         limit_documents = _es_objects_bulk_replay;


      for (auto const &value: ids) {
         if (value.is<proposal_object>() && _es_objects_proposals) {
            auto obj = db.find_object(value);
            auto p = static_cast<const proposal_object *>(obj);
            if (p != nullptr) {
               if (action == "delete")
                  remove_from_database(p->id, "proposal");
               else
                  prepareTemplate<proposal_object>(*p, "proposal");
            }
         } else if (value.is<account_object>() && _es_objects_accounts) {
            auto obj = db.find_object(value);
            auto a = static_cast<const account_object *>(obj);
            if (a != nullptr) {
               if (action == "delete")
                  remove_from_database(a->id, "account");
               else
                  prepareTemplate<account_object>(*a, "account");
            }
         } else if (value.is<asset_object>() && _es_objects_assets) {
            auto obj = db.find_object(value);
            auto a = static_cast<const asset_object *>(obj);
            if (a != nullptr) {
               if (action == "delete")
                  remove_from_database(a->id, "asset");
               else
                  prepareTemplate<asset_object>(*a, "asset");
            }
         } else if (value.is<account_balance_object>() && _es_objects_balances) {
            auto obj = db.find_object(value);
            auto b = static_cast<const account_balance_object *>(obj);
            if (b != nullptr) {
               if (action == "delete")
                  remove_from_database(b->id, "balance");
               else
                  prepareTemplate<account_balance_object>(*b, "balance");
            }
         } else if (value.is<limit_order_object>() && _es_objects_limit_orders) {
            auto obj = db.find_object(value);
            auto l = static_cast<const limit_order_object *>(obj);
            if (l != nullptr) {
               if (action == "delete")
                  remove_from_database(l->id, "limitorder");
               else
                  prepareTemplate<limit_order_object>(*l, "limitorder");
            }
         } else if (value.is<asset_bitasset_data_object>() && _es_objects_asset_bitasset) {
            auto obj = db.find_object(value);
            auto ba = static_cast<const asset_bitasset_data_object *>(obj);
            if (ba != nullptr) {
               if (action == "delete")
                  remove_from_database(ba->id, "bitasset");
               else
                  prepareTemplate<asset_bitasset_data_object>(*ba, "bitasset");
            }
         } else if (value.is<account_role_object>() && _es_objects_account_role) {
            auto obj = db.find_object(value);
            auto ba = static_cast<const account_role_object *>(obj);
            if (ba != nullptr) {
               if (action == "delete")
                  remove_from_database(ba->id, "account_role");
               else
                  prepareTemplate<account_role_object>(*ba, "account_role");
            }
         } else if (value.is<committee_member_object>() && _es_objects_committee_member) {
            auto obj = db.find_object(value);
            auto ba = static_cast<const committee_member_object *>(obj);
            if (ba != nullptr) {
               if (action == "delete")
                  remove_from_database(ba->id, "committee_member");
               else
                  prepareTemplate<committee_member_object>(*ba, "committee_member");
            }
         } else if (value.is<nft_object>() && _es_objects_nft) {
            auto obj = db.find_object(value);
            auto ba = static_cast<const nft_object *>(obj);
            if (ba != nullptr) {
               if (action == "delete")
                  remove_from_database(ba->id, "nft");
               else
                  prepareTemplate<nft_object>(*ba, "nft");
            }
         } else if (value.is<nft_metadata_object>() && _es_objects_nft) {
            auto obj = db.find_object(value);
            auto ba = static_cast<const nft_metadata_object *>(obj);
            if (ba != nullptr) {
               if (action == "delete")
                  remove_from_database(ba->id, "nft_metadata");
               else
                  prepareTemplate<nft_metadata_object>(*ba, "nft_metadata");
            }
         } else if (value.is<offer_object>() && _es_objects_nft) {
            auto obj = db.find_object(value);
            auto ba = static_cast<const offer_object *>(obj);
            if (ba != nullptr) {
               if (action == "delete")
                  remove_from_database(ba->id, "offer");
               else
                  prepareTemplate<offer_object>(*ba, "offer");
            }
         } else if (value.is<sidechain_address_object>() && _es_objects_son) {
            auto obj = db.find_object(value);
            auto ba = static_cast<const sidechain_address_object *>(obj);
            if (ba != nullptr) {
               if (action == "delete")
                  remove_from_database(ba->id, "sidechain_address");
               else
                  prepareTemplate<sidechain_address_object>(*ba, "sidechain_address");
            }
         } else if (value.is<sidechain_transaction_object>() && _es_objects_son) {
            auto obj = db.find_object(value);
            auto ba = static_cast<const sidechain_transaction_object *>(obj);
            if (ba != nullptr) {
               if (action == "delete")
                  remove_from_database(ba->id, "sidechain_transaction");
               else
                  prepareTemplate<sidechain_transaction_object>(*ba, "sidechain_transaction");
            }
         } else if (value.is<son_object>() && _es_objects_son) {
            auto obj = db.find_object(value);
            auto ba = static_cast<const son_object *>(obj);
            if (ba != nullptr) {
               if (action == "delete")
                  remove_from_database(ba->id, "son");
               else
                  prepareTemplate<son_object>(*ba, "son");
            }
         } else if (value.is<son_proposal_object>() && _es_objects_son) {
            auto obj = db.find_object(value);
            auto ba = static_cast<const son_proposal_object *>(obj);
            if (ba != nullptr) {
               if (action == "delete")
                  remove_from_database(ba->id, "son_proposal");
               else
                  prepareTemplate<son_proposal_object>(*ba, "son_proposal");
            }
         } else if (value.is<son_wallet_object>() && _es_objects_son) {
            auto obj = db.find_object(value);
            auto ba = static_cast<const son_wallet_object *>(obj);
            if (ba != nullptr) {
               if (action == "delete")
                  remove_from_database(ba->id, "son_wallet");
               else
                  prepareTemplate<son_wallet_object>(*ba, "son_wallet");
            }
         } else if (value.is<son_wallet_deposit_object>() && _es_objects_son) {
            auto obj = db.find_object(value);
            auto ba = static_cast<const son_wallet_deposit_object *>(obj);
            if (ba != nullptr) {
               if (action == "delete")
                  remove_from_database(ba->id, "son_wallet_deposit");
               else
                  prepareTemplate<son_wallet_deposit_object>(*ba, "son_wallet_deposit");
            }
         } else if (value.is<son_wallet_withdraw_object>() && _es_objects_son) {
            auto obj = db.find_object(value);
            auto ba = static_cast<const son_wallet_withdraw_object *>(obj);
            if (ba != nullptr) {
               if (action == "delete")
                  remove_from_database(ba->id, "son_wallet_withdraw");
               else
                  prepareTemplate<son_wallet_withdraw_object>(*ba, "son_wallet_withdraw");
            }
         } else if (value.is<transaction_object>() && _es_objects_transaction) {
            auto obj = db.find_object(value);
            auto ba = static_cast<const transaction_object *>(obj);
            if (ba != nullptr) {
               if (action == "delete")
                  remove_from_database(ba->id, "transaction");
               else
                  prepareTemplate<transaction_object>(*ba, "transaction");
            }
         } else if (value.is<vesting_balance_object>() && _es_objects_vesting_balance) {
            auto obj = db.find_object(value);
            auto ba = static_cast<const vesting_balance_object *>(obj);
            if (ba != nullptr) {
               if (action == "delete")
                  remove_from_database(ba->id, "vesting_balance");
               else
                  prepareTemplate<vesting_balance_object>(*ba, "vesting_balance");
            }
         } else if (value.is<witness_object>() && _es_objects_witness) {
            auto obj = db.find_object(value);
            auto ba = static_cast<const witness_object *>(obj);
            if (ba != nullptr) {
               if (action == "delete")
                  remove_from_database(ba->id, "witness");
               else
                  prepareTemplate<witness_object>(*ba, "witness");
            }
         } else if (value.is<worker_object>() && _es_objects_worker) {
            auto obj = db.find_object(value);
            auto ba = static_cast<const worker_object *>(obj);
            if (ba != nullptr) {
               if (action == "delete")
                  remove_from_database(ba->id, "worker");
               else
                  prepareTemplate<worker_object>(*ba, "worker");
            }
         }
      }

      if (curl && bulk.size() >= limit_documents) { // we are in bulk time, ready to add data to elasticsearech

         graphene::utilities::ES es;
         es.curl = curl;
         es.bulk_lines = bulk;
         es.elasticsearch_url = _es_objects_elasticsearch_url;
         es.auth = _es_objects_auth;

         if (!graphene::utilities::SendBulk(es))
            return false;
         else
            bulk.clear();
      }
   }

   return true;
}

void es_objects_plugin_impl::remove_from_database( object_id_type id, std::string index)
{
   if(_es_objects_keep_only_current)
   {
      fc::mutable_variant_object delete_line;
      delete_line["_id"] = string(id);
      delete_line["_index"] = _es_objects_index_prefix + index;
      delete_line["_type"] = "data";
      fc::mutable_variant_object final_delete_line;
      final_delete_line["delete"] = delete_line;
      prepare.push_back(fc::json::to_string(final_delete_line));
      std::move(prepare.begin(), prepare.end(), std::back_inserter(bulk));
      prepare.clear();
   }
}

template<typename T>
void es_objects_plugin_impl::prepareTemplate(T blockchain_object, string index_name)
{
   fc::mutable_variant_object bulk_header;
   bulk_header["_index"] = _es_objects_index_prefix + index_name;
   bulk_header["_type"] = "data";
   if(_es_objects_keep_only_current)
   {
      bulk_header["_id"] = string(blockchain_object.id);
   }

   adaptor_struct adaptor;
   fc::variant blockchain_object_variant;
   fc::to_variant( blockchain_object, blockchain_object_variant, GRAPHENE_NET_MAX_NESTED_OBJECTS );
   fc::mutable_variant_object o = adaptor.adapt(blockchain_object_variant.get_object());

   o["object_id"] = string(blockchain_object.id);
   o["block_time"] = block_time;
   o["block_number"] = block_number;

   string data = fc::json::to_string(o);

   prepare = graphene::utilities::createBulk(bulk_header, std::move(data));
   std::move(prepare.begin(), prepare.end(), std::back_inserter(bulk));
   prepare.clear();
}

es_objects_plugin_impl::~es_objects_plugin_impl()
{
   if (curl) {
      curl_easy_cleanup(curl);
      curl = nullptr;
   }
   return;
}

} // end namespace detail

es_objects_plugin::es_objects_plugin() :
   my( new detail::es_objects_plugin_impl(*this) )
{
}

es_objects_plugin::~es_objects_plugin()
{
}

std::string es_objects_plugin::plugin_name()const
{
   return "es_objects";
}
std::string es_objects_plugin::plugin_description()const
{
   return "Stores blockchain objects in ES database. Experimental.";
}

void es_objects_plugin::plugin_set_program_options(
   boost::program_options::options_description& cli,
   boost::program_options::options_description& cfg
   )
{
   cli.add_options()
         ("es-objects-elasticsearch-url", boost::program_options::value<std::string>(),
               "Elasticsearch node url(http://localhost:9200/)")
         ("es-objects-auth", boost::program_options::value<std::string>(), "Basic auth username:password('')")
         ("es-objects-bulk-replay", boost::program_options::value<uint32_t>(),
               "Number of bulk documents to index on replay(10000)")
         ("es-objects-bulk-sync", boost::program_options::value<uint32_t>(),
               "Number of bulk documents to index on a synchronized chain(100)")
         ("es-objects-proposals", boost::program_options::value<bool>(), "Store proposal objects(true)")
         ("es-objects-accounts", boost::program_options::value<bool>(), "Store account objects(true)")
         ("es-objects-assets", boost::program_options::value<bool>(), "Store asset objects(true)")
         ("es-objects-balances", boost::program_options::value<bool>(), "Store balances objects(true)")
         ("es-objects-limit-orders", boost::program_options::value<bool>(), "Store limit order objects(false)")
         ("es-objects-bitasset", boost::program_options::value<bool>(), "Store feed data(true)")
         ("es-objects-account-role", boost::program_options::value<bool>(), "Store account role objects (true)")
         ("es-objects-committee-member", boost::program_options::value<bool>(), "Store committee member objects(true)")
         ("es-objects-nft", boost::program_options::value<bool>(), "Store nft objects (true)")
         ("es-objects-son", boost::program_options::value<bool>(), "Store son objects (true)")
         ("es-objects-transaction", boost::program_options::value<bool>(), "Store transaction objects (true)")
         ("es-objects-vesting-balance", boost::program_options::value<bool>(), "Store vesting balance objects (true)")
         ("es-objects-witness", boost::program_options::value<bool>(), "Store witness objects (true)")
         ("es-objects-worker", boost::program_options::value<bool>(), "Store worker objects (true)")
         ("es-objects-index-prefix", boost::program_options::value<std::string>(),
               "Add a prefix to the index(ppobjects-)")
         ("es-objects-keep-only-current", boost::program_options::value<bool>(),
               "Keep only current state of the objects(true)")
         ("es-objects-start-es-after-block", boost::program_options::value<uint32_t>(),
               "Start doing ES job after block(0)")
         ;
   cfg.add(cli);
}

void es_objects_plugin::plugin_initialize(const boost::program_options::variables_map& options)
{
   if (options.count("es-objects-elasticsearch-url")) {
      my->_es_objects_elasticsearch_url = options["es-objects-elasticsearch-url"].as<std::string>();
   }
   if (options.count("es-objects-auth")) {
      my->_es_objects_auth = options["es-objects-auth"].as<std::string>();
   }
   if (options.count("es-objects-bulk-replay")) {
      my->_es_objects_bulk_replay = options["es-objects-bulk-replay"].as<uint32_t>();
   }
   if (options.count("es-objects-bulk-sync")) {
      my->_es_objects_bulk_sync = options["es-objects-bulk-sync"].as<uint32_t>();
   }
   if (options.count("es-objects-proposals")) {
      my->_es_objects_proposals = options["es-objects-proposals"].as<bool>();
   }
   if (options.count("es-objects-accounts")) {
      my->_es_objects_accounts = options["es-objects-accounts"].as<bool>();
   }
   if (options.count("es-objects-assets")) {
      my->_es_objects_assets = options["es-objects-assets"].as<bool>();
   }
   if (options.count("es-objects-balances")) {
      my->_es_objects_balances = options["es-objects-balances"].as<bool>();
   }
   if (options.count("es-objects-limit-orders")) {
      my->_es_objects_limit_orders = options["es-objects-limit-orders"].as<bool>();
   }
   if (options.count("es-objects-asset-bitasset")) {
      my->_es_objects_asset_bitasset = options["es-objects-asset-bitasset"].as<bool>();
   }
   if (options.count("es-objects-account-role")) {
      my->_es_objects_balances = options["es-objects-account-role"].as<bool>();
   }
   if (options.count("es-objects-committee-member")) {
      my->_es_objects_balances = options["es-objects-committee-member"].as<bool>();
   }
   if (options.count("es-objects-nft")) {
      my->_es_objects_balances = options["es-objects-nft"].as<bool>();
   }
   if (options.count("es-objects-son")) {
      my->_es_objects_balances = options["es-objects-son"].as<bool>();
   }
   if (options.count("es-objects-transaction")) {
      my->_es_objects_balances = options["es-objects-transaction"].as<bool>();
   }
   if (options.count("es-objects-vesting-balance")) {
      my->_es_objects_balances = options["es-objects-vesting-balance"].as<bool>();
   }
   if (options.count("es-objects-witness")) {
      my->_es_objects_balances = options["es-objects-witness"].as<bool>();
   }
   if (options.count("es-objects-worker")) {
      my->_es_objects_balances = options["es-objects-worker"].as<bool>();
   }
   if (options.count("es-objects-index-prefix")) {
      my->_es_objects_index_prefix = options["es-objects-index-prefix"].as<std::string>();
   }
   if (options.count("es-objects-keep-only-current")) {
      my->_es_objects_keep_only_current = options["es-objects-keep-only-current"].as<bool>();
   }
   if (options.count("es-objects-start-es-after-block")) {
      my->_es_objects_start_es_after_block = options["es-objects-start-es-after-block"].as<uint32_t>();
   }

   database().applied_block.connect([this](const signed_block &b) {
      if(b.block_num() == 1 && my->_es_objects_start_es_after_block == 0) {
         if (!my->genesis())
            FC_THROW_EXCEPTION(graphene::chain::plugin_exception, "Error populating genesis data.");
      }
   });
   database().new_objects.connect([this]( const vector<object_id_type>& ids,
         const flat_set<account_id_type>& impacted_accounts ) {
      if(!my->index_database(ids, "create"))
      {
         FC_THROW_EXCEPTION(graphene::chain::plugin_exception,
               "Error creating object from ES database, we are going to keep trying.");
      }
   });
   database().changed_objects.connect([this]( const vector<object_id_type>& ids,
         const flat_set<account_id_type>& impacted_accounts ) {
      if(!my->index_database(ids, "update"))
      {
         FC_THROW_EXCEPTION(graphene::chain::plugin_exception,
               "Error updating object from ES database, we are going to keep trying.");
      }
   });
   database().removed_objects.connect([this](const vector<object_id_type>& ids,
         const vector<const object*>& objs, const flat_set<account_id_type>& impacted_accounts) {
      if(!my->index_database(ids, "delete"))
      {
         FC_THROW_EXCEPTION(graphene::chain::plugin_exception,
               "Error deleting object from ES database, we are going to keep trying.");
      }
   });
}

void es_objects_plugin::plugin_startup()
{
   graphene::utilities::ES es;
   es.curl = my->curl;
   es.elasticsearch_url = my->_es_objects_elasticsearch_url;
   es.auth = my->_es_objects_auth;
   es.auth = my->_es_objects_index_prefix;

   if(!graphene::utilities::checkES(es))
      FC_THROW_EXCEPTION(fc::exception, "ES database is not up in url ${url}", ("url", my->_es_objects_elasticsearch_url));
   ilog("elasticsearch OBJECTS: plugin_startup() begin");
}

} }