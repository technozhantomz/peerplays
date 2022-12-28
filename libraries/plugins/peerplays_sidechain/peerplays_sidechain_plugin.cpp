#include <graphene/peerplays_sidechain/peerplays_sidechain_plugin.hpp>

#include <boost/property_tree/json_parser.hpp>
#include <boost/property_tree/ptree.hpp>
#include <boost/range/algorithm_ext/insert.hpp>
#include <future>
#include <thread>

#include <fc/log/logger.hpp>
#include <graphene/chain/proposal_object.hpp>
#include <graphene/chain/protocol/transfer.hpp>
#include <graphene/chain/sidechain_address_object.hpp>
#include <graphene/chain/son_wallet_object.hpp>
#include <graphene/chain/son_wallet_withdraw_object.hpp>
#include <graphene/peerplays_sidechain/sidechain_api.hpp>
#include <graphene/peerplays_sidechain/sidechain_net_handler_factory.hpp>
#include <graphene/utilities/key_conversion.hpp>

namespace bpo = boost::program_options;

namespace graphene { namespace peerplays_sidechain {

namespace detail {

class peerplays_sidechain_plugin_impl {
public:
   peerplays_sidechain_plugin_impl(peerplays_sidechain_plugin &_plugin);
   virtual ~peerplays_sidechain_plugin_impl();

   void plugin_set_program_options(
         boost::program_options::options_description &cli,
         boost::program_options::options_description &cfg);
   void plugin_initialize(const boost::program_options::variables_map &opt);
   void plugin_startup();
   void plugin_shutdown();

   std::set<chain::son_id_type> &get_sons();
   const son_id_type get_current_son_id(sidechain_type sidechain);
   const son_object get_current_son_object(sidechain_type sidechain);
   const son_object get_son_object(son_id_type son_id);
   bool is_active_son(sidechain_type sidechain, son_id_type son_id);
   bool is_son_deregistered(son_id_type son_id);
   bool is_son_deregister_op_valid(const chain::operation &op);
   bool is_son_down_op_valid(const chain::operation &op);
   bool is_valid_son_proposal(const chain::proposal_object &proposal);
   fc::ecc::private_key get_private_key(son_id_type son_id);
   fc::ecc::private_key get_private_key(chain::public_key_type public_key);
   void log_son_proposal_retry(sidechain_type sidechain, int op_type, object_id_type object_id);
   bool can_son_participate(sidechain_type sidechain, int op_type, object_id_type object_id);
   std::map<sidechain_type, std::vector<std::string>> get_son_listener_log();
   optional<asset> estimate_withdrawal_transaction_fee(sidechain_type sidechain);

   void schedule_heartbeat_loop();
   void heartbeat_loop();
   void schedule_son_processing();
   void son_processing(sidechain_type sidechain);
   void approve_proposals(sidechain_type sidechain);
   void create_son_down_proposals(sidechain_type sidechain);
   void create_son_deregister_proposals(sidechain_type sidechain);

   void process_proposals(sidechain_type sidechain);
   void process_active_sons_change(sidechain_type sidechain);
   void create_deposit_addresses(sidechain_type sidechain);
   void process_deposits(sidechain_type sidechain);
   void process_withdrawals(sidechain_type sidechain);
   void process_sidechain_transactions(sidechain_type sidechain);
   void send_sidechain_transactions(sidechain_type sidechain);
   void settle_sidechain_transactions(sidechain_type sidechain);

private:
   peerplays_sidechain_plugin &plugin;

   boost::program_options::variables_map options;

   bool config_ready_son;
   bool config_ready_bitcoin;
   bool config_ready_ethereum;
   bool config_ready_hive;
   bool config_ready_peerplays;

   bool sidechain_enabled_bitcoin;
   bool sidechain_enabled_ethereum;
   bool sidechain_enabled_hive;
   bool sidechain_enabled_peerplays;

   std::map<sidechain_type, son_id_type> current_son_id;
   std::mutex current_son_id_mutex;
   std::mutex access_db_mutex;
   std::mutex access_approve_prop_mutex;
   std::mutex access_son_down_prop_mutex;
   std::mutex access_son_deregister_prop_mutex;

   std::map<sidechain_type, bool> sidechain_enabled;
   std::map<sidechain_type, std::unique_ptr<sidechain_net_handler>> net_handlers;
   std::set<chain::son_id_type> sons;
   std::map<chain::public_key_type, fc::ecc::private_key> private_keys;
   fc::future<void> _heartbeat_task;
   std::map<sidechain_type, std::future<void>> _son_processing_task;
   std::map<son_proposal_type, uint16_t> son_retry_count;
   uint16_t retries_threshold = 150;

   bool first_block_skipped;
   void on_applied_block(const signed_block &b);
};

peerplays_sidechain_plugin_impl::peerplays_sidechain_plugin_impl(peerplays_sidechain_plugin &_plugin) :
      plugin(_plugin),
      config_ready_son(false),
      config_ready_bitcoin(false),
      config_ready_ethereum(false),
      config_ready_hive(false),
      config_ready_peerplays(false),
      sidechain_enabled_bitcoin(false),
      sidechain_enabled_ethereum(false),
      sidechain_enabled_hive(false),
      sidechain_enabled_peerplays(false),
      current_son_id([] {
         std::map<sidechain_type, son_id_type> current_son_id;
         for (const auto &active_sidechain_type : active_sidechain_types) {
            current_son_id.emplace(active_sidechain_type, son_id_type(std::numeric_limits<uint32_t>().max()));
         }
         return current_son_id;
      }()),
      sidechain_enabled([] {
         std::map<sidechain_type, bool> sidechain_enabled;
         for (const auto &active_sidechain_type : active_sidechain_types) {
            sidechain_enabled.emplace(active_sidechain_type, false);
         }
         return sidechain_enabled;
      }()),
      net_handlers([] {
         std::map<sidechain_type, std::unique_ptr<sidechain_net_handler>> net_handlers;
         for (const auto &active_sidechain_type : active_sidechain_types) {
            net_handlers.emplace(active_sidechain_type, nullptr);
         }
         return net_handlers;
      }()),
      first_block_skipped(false) {
}

peerplays_sidechain_plugin_impl::~peerplays_sidechain_plugin_impl() {
   try {
      if (_heartbeat_task.valid())
         _heartbeat_task.cancel_and_wait(__FUNCTION__);
   } catch (fc::canceled_exception &) {
      //Expected exception. Move along.
   } catch (fc::exception &e) {
      edump((e.to_detail_string()));
   }

   try {
      for (const auto &active_sidechain_type : active_sidechain_types) {
         if (_son_processing_task.count(active_sidechain_type) != 0 && _son_processing_task.at(active_sidechain_type).valid())
            _son_processing_task.at(active_sidechain_type).wait();
      }
   } catch (fc::canceled_exception &) {
      //Expected exception. Move along.
   } catch (fc::exception &e) {
      edump((e.to_detail_string()));
   }
}

void peerplays_sidechain_plugin_impl::plugin_set_program_options(
      boost::program_options::options_description &cli,
      boost::program_options::options_description &cfg) {
   auto default_priv_key = fc::ecc::private_key::regenerate(fc::sha256::hash(std::string("nathan")));
   string son_id_example = fc::json::to_string(chain::son_id_type(5));
   string son_id_example2 = fc::json::to_string(chain::son_id_type(6));

   cli.add_options()("son-id", bpo::value<vector<string>>(), ("ID of SON controlled by this node (e.g. " + son_id_example + ", quotes are required)").c_str());
   cli.add_options()("son-ids", bpo::value<string>(), ("IDs of multiple SONs controlled by this node (e.g. [" + son_id_example + ", " + son_id_example2 + "], quotes are required)").c_str());
   cli.add_options()("peerplays-private-key", bpo::value<vector<string>>()->composing()->multitoken()->DEFAULT_VALUE_VECTOR(std::make_pair(chain::public_key_type(default_priv_key.get_public_key()), graphene::utilities::key_to_wif(default_priv_key))),
                     "Tuple of [PublicKey, WIF private key] (may specify multiple times)");

   cli.add_options()("sidechain-retry-threshold", bpo::value<uint16_t>()->default_value(150), "Sidechain retry throttling threshold");

   cli.add_options()("debug-rpc-calls", bpo::value<bool>()->default_value(false), "Outputs RPC calls to console");

   cli.add_options()("bitcoin-sidechain-enabled", bpo::value<bool>()->default_value(false), "Bitcoin sidechain handler enabled");
   cli.add_options()("bitcoin-node-ip", bpo::value<string>()->default_value("127.0.0.1"), "IP address of Bitcoin node");
   cli.add_options()("bitcoin-node-zmq-port", bpo::value<uint32_t>()->default_value(11111), "ZMQ port of Bitcoin node");
   cli.add_options()("bitcoin-node-rpc-port", bpo::value<uint32_t>()->default_value(8332), "RPC port of Bitcoin node");
   cli.add_options()("bitcoin-node-rpc-user", bpo::value<string>()->default_value("1"), "Bitcoin RPC user");
   cli.add_options()("bitcoin-node-rpc-password", bpo::value<string>()->default_value("1"), "Bitcoin RPC password");
   cli.add_options()("bitcoin-wallet-name", bpo::value<string>(), "Bitcoin wallet name");
   cli.add_options()("bitcoin-wallet-password", bpo::value<string>(), "Bitcoin wallet password");
   cli.add_options()("bitcoin-private-key", bpo::value<vector<string>>()->composing()->multitoken()->DEFAULT_VALUE_VECTOR(std::make_pair("02d0f137e717fb3aab7aff99904001d49a0a636c5e1342f8927a4ba2eaee8e9772", "cVN31uC9sTEr392DLVUEjrtMgLA8Yb3fpYmTRj7bomTm6nn2ANPr")),
                     "Tuple of [Bitcoin public key, Bitcoin private key] (may specify multiple times)");

   cli.add_options()("ethereum-sidechain-enabled", bpo::value<bool>()->default_value(false), "Ethereum sidechain handler enabled");
   cli.add_options()("ethereum-node-rpc-url", bpo::value<string>()->default_value("127.0.0.1:8545"), "Ethereum node RPC URL [http[s]://]host[:port]");
   cli.add_options()("ethereum-node-rpc-user", bpo::value<string>(), "Ethereum RPC user");
   cli.add_options()("ethereum-node-rpc-password", bpo::value<string>(), "Ethereum RPC password");
   cli.add_options()("ethereum-wallet-contract-address", bpo::value<string>(), "Ethereum wallet contract address");
   cli.add_options()("ethereum-private-key", bpo::value<vector<string>>()->composing()->multitoken()->DEFAULT_VALUE_VECTOR(std::make_pair("5fbbb31be52608d2f52247e8400b7fcaa9e0bc12", "9bedac2bd8fe2a6f6528e066c67fc8ac0622e96828d40c0e820d83c5bd2b0589")),
                     "Tuple of [Ethereum public key, Ethereum private key] (may specify multiple times)");

   cli.add_options()("hive-sidechain-enabled", bpo::value<bool>()->default_value(false), "Hive sidechain handler enabled");
   cli.add_options()("hive-node-rpc-url", bpo::value<string>()->default_value("127.0.0.1:28090"), "Hive node RPC URL [http[s]://]host[:port]");
   cli.add_options()("hive-node-rpc-user", bpo::value<string>(), "Hive node RPC user");
   cli.add_options()("hive-node-rpc-password", bpo::value<string>(), "Hive node RPC password");
   cli.add_options()("hive-wallet-account-name", bpo::value<string>(), "Hive wallet account name");
   cli.add_options()("hive-private-key", bpo::value<vector<string>>()->composing()->multitoken()->DEFAULT_VALUE_VECTOR(std::make_pair("TST6LLegbAgLAy28EHrffBVuANFWcFgmqRMW13wBmTExqFE9SCkg4", "5JNHfZYKGaomSFvd4NUdQ9qMcEAC43kujbfjueTHpVapX1Kzq2n")),
                     "Tuple of [Hive public key, Hive private key] (may specify multiple times)");

   cfg.add(cli);
}

void peerplays_sidechain_plugin_impl::plugin_initialize(const boost::program_options::variables_map &opt) {
   options = opt;
   config_ready_son = (options.count("son-id") || options.count("son-ids")) && options.count("peerplays-private-key");
   if (config_ready_son) {
      LOAD_VALUE_SET(options, "son-id", sons, chain::son_id_type)
      if (options.count("son-ids")) {
         vector<chain::son_id_type> v = fc::json::from_string(options.at("son-ids").as<string>()).as<vector<chain::son_id_type>>(5);
         sons.insert(v.begin(), v.end());
      }
      config_ready_son = config_ready_son && !sons.empty();

      if (options.count("peerplays-private-key")) {
         const std::vector<std::string> key_id_to_wif_pair_strings = options["peerplays-private-key"].as<std::vector<std::string>>();
         for (const std::string &key_id_to_wif_pair_string : key_id_to_wif_pair_strings) {
            auto key_id_to_wif_pair = graphene::app::dejsonify<std::pair<chain::public_key_type, std::string>>(key_id_to_wif_pair_string, 5);
            ilog("Public Key: ${public}", ("public", key_id_to_wif_pair.first));
            fc::optional<fc::ecc::private_key> private_key = graphene::utilities::wif_to_key(key_id_to_wif_pair.second);
            if (!private_key) {
               // the key isn't in WIF format; see if they are still passing the old native private key format.  This is
               // just here to ease the transition, can be removed soon
               try {
                  private_key = fc::variant(key_id_to_wif_pair.second, 2).as<fc::ecc::private_key>(1);
               } catch (const fc::exception &) {
                  FC_THROW("Invalid WIF-format private key ${key_string}", ("key_string", key_id_to_wif_pair.second));
               }
            }
            private_keys[key_id_to_wif_pair.first] = *private_key;
         }
         config_ready_son = config_ready_son && !private_keys.empty();
      }
      if (options.count("sidechain-retry-threshold")) {
         retries_threshold = options.at("sidechain-retry-threshold").as<uint16_t>();
      }
      ilog("sidechain-retry-threshold: ${sidechain-retry-threshold}", ("sidechain-retry-threshold", retries_threshold));
   }
   if (!config_ready_son) {
      wlog("Haven't set up SON parameters");
      throw;
   }

   sidechain_enabled_bitcoin = options.at("bitcoin-sidechain-enabled").as<bool>();
   config_ready_bitcoin = options.count("bitcoin-node-ip") &&
                          options.count("bitcoin-node-zmq-port") && options.count("bitcoin-node-rpc-port") &&
                          options.count("bitcoin-node-rpc-user") && options.count("bitcoin-node-rpc-password") &&
                          options.count("bitcoin-wallet-name") && options.count("bitcoin-wallet-password") &&
                          options.count("bitcoin-private-key");
   if (sidechain_enabled_bitcoin && !config_ready_bitcoin) {
      wlog("Haven't set up Bitcoin sidechain parameters");
   }

   sidechain_enabled_ethereum = options.at("ethereum-sidechain-enabled").as<bool>();
   config_ready_ethereum = options.count("ethereum-node-rpc-url") &&
                           /*options.count("ethereum-node-rpc-user") && options.count("ethereum-node-rpc-password") &&*/
                           options.count("ethereum-wallet-contract-address") &&
                           options.count("ethereum-private-key");
   if (sidechain_enabled_ethereum && !config_ready_ethereum) {
      wlog("Haven't set up Ethereum sidechain parameters");
   }

   sidechain_enabled_hive = options.at("hive-sidechain-enabled").as<bool>();
   config_ready_hive = options.count("hive-node-rpc-url") &&
                       /*options.count("hive-node-rpc-user") && options.count("hive-node-rpc-password") &&*/
                       options.count("hive-wallet-account-name") &&
                       options.count("hive-private-key");
   if (sidechain_enabled_hive && !config_ready_hive) {
      wlog("Haven't set up Hive sidechain parameters");
   }

#ifdef ENABLE_PEERPLAYS_ASSET_DEPOSITS
   sidechain_enabled_peerplays = true;
#else
   sidechain_enabled_peerplays = false;
#endif
   config_ready_peerplays = true;
   if (sidechain_enabled_peerplays && !config_ready_peerplays) {
      wlog("Haven't set up Peerplays sidechain parameters");
   }
}

void peerplays_sidechain_plugin_impl::plugin_startup() {

   if (config_ready_son) {
      ilog("Starting ${n} SON instances", ("n", sons.size()));

      schedule_heartbeat_loop();
   } else {
      elog("No sons configured! Please add SON IDs and private keys to configuration.");
   }

   sidechain_net_handler_factory net_handler_factory(plugin);

   if (sidechain_enabled_bitcoin && config_ready_bitcoin) {
      sidechain_enabled.at(sidechain_type::bitcoin) = true;
      net_handlers.at(sidechain_type::bitcoin) = net_handler_factory.create_handler(sidechain_type::bitcoin, options);
      ilog("Bitcoin sidechain handler running");
   }

   if (sidechain_enabled_ethereum && config_ready_ethereum) {
      sidechain_enabled.at(sidechain_type::ethereum) = true;
      net_handlers.at(sidechain_type::ethereum) = net_handler_factory.create_handler(sidechain_type::ethereum, options);
      ilog("Ethereum sidechain handler running");
   }

   if (sidechain_enabled_hive && config_ready_hive) {
      sidechain_enabled.at(sidechain_type::hive) = true;
      net_handlers.at(sidechain_type::hive) = net_handler_factory.create_handler(sidechain_type::hive, options);
      ilog("Hive sidechain handler running");
   }

   if (sidechain_enabled_peerplays && config_ready_peerplays) {
      sidechain_enabled.at(sidechain_type::peerplays) = true;
      net_handlers.at(sidechain_type::peerplays) = net_handler_factory.create_handler(sidechain_type::peerplays, options);
      ilog("Peerplays sidechain handler running");
   }

   plugin.database().applied_block.connect([&](const signed_block &b) {
      on_applied_block(b);
   });
}

void peerplays_sidechain_plugin_impl::plugin_shutdown() {
}

std::set<chain::son_id_type> &peerplays_sidechain_plugin_impl::get_sons() {
   return sons;
}

const son_id_type peerplays_sidechain_plugin_impl::get_current_son_id(sidechain_type sidechain) {
   const std::lock_guard<std::mutex> lock(current_son_id_mutex);
   return current_son_id.at(sidechain);
}

const son_object peerplays_sidechain_plugin_impl::get_current_son_object(sidechain_type sidechain) {
   return get_son_object(get_current_son_id(sidechain));
}

const son_object peerplays_sidechain_plugin_impl::get_son_object(son_id_type son_id) {
   const auto &idx = plugin.database().get_index_type<chain::son_index>().indices().get<by_id>();
   auto son_obj = idx.find(son_id);
   if (son_obj == idx.end())
      return {};
   return *son_obj;
}

bool peerplays_sidechain_plugin_impl::is_active_son(sidechain_type sidechain, son_id_type son_id) {
   const auto &idx = plugin.database().get_index_type<chain::son_index>().indices().get<by_id>();
   auto son_obj = idx.find(son_id);
   if (son_obj == idx.end())
      return false;

   const chain::global_property_object &gpo = plugin.database().get_global_properties();
   set<son_id_type> active_son_ids;
   std::transform(gpo.active_sons.at(sidechain).cbegin(), gpo.active_sons.at(sidechain).cend(),
                  std::inserter(active_son_ids, active_son_ids.end()),
                  [](const son_info &swi) {
                     return swi.son_id;
                  });

   auto it = std::find(active_son_ids.begin(), active_son_ids.end(), son_id);

   return (it != active_son_ids.end());
}

bool peerplays_sidechain_plugin_impl::is_son_deregistered(son_id_type son_id) {
   const auto &idx = plugin.database().get_index_type<chain::son_index>().indices().get<by_id>();
   auto son_obj = idx.find(son_id);
   if (son_obj == idx.end())
      return true;

   bool status_deregistered = true;
   for (const auto &status : son_obj->statuses) {
      if ((status.second != son_status::deregistered))
         status_deregistered = false;
   }

   if (status_deregistered) {
      return true;
   }

   return false;
}

bool peerplays_sidechain_plugin_impl::is_son_deregister_op_valid(const chain::operation &op) {
   son_deregister_operation deregister_op = op.get<son_deregister_operation>();
   return plugin.database().is_son_dereg_valid(deregister_op.son_id);
}

bool peerplays_sidechain_plugin_impl::is_son_down_op_valid(const chain::operation &op) {
   chain::database &d = plugin.database();
   const chain::global_property_object &gpo = d.get_global_properties();
   const chain::dynamic_global_property_object &dgpo = d.get_dynamic_global_properties();
   const auto &idx = d.get_index_type<chain::son_index>().indices().get<by_id>();
   const son_report_down_operation down_op = op.get<son_report_down_operation>();
   const auto son_obj = idx.find(down_op.son_id);
   if (son_obj == idx.end()) {
      return false;
   }
   const auto stats = son_obj->statistics(d);
   const fc::time_point_sec last_maintenance_time = dgpo.next_maintenance_time - gpo.parameters.maintenance_interval;
   const int64_t down_threshold = gpo.parameters.son_down_time();

   bool status_son_down_op_valid = true;
   for (const auto &status : son_obj->statuses) {
      if ((status.second != son_status::active) && (status.second != son_status::request_maintenance))
         status_son_down_op_valid = false;
   }
   if (status_son_down_op_valid) {
      for (const auto &active_sidechain_type : active_sidechain_types) {
         if (stats.last_active_timestamp.contains(active_sidechain_type)) {
            const fc::time_point_sec last_active_ts = ((stats.last_active_timestamp.at(active_sidechain_type) > last_maintenance_time) ? stats.last_active_timestamp.at(active_sidechain_type) : last_maintenance_time);
            if (((fc::time_point::now() - last_active_ts) <= fc::seconds(down_threshold))) {
               status_son_down_op_valid = false;
            }
         }
      }
   }

   return status_son_down_op_valid;
}

fc::ecc::private_key peerplays_sidechain_plugin_impl::get_private_key(son_id_type son_id) {
   return get_private_key(get_son_object(son_id).signing_key);
}

fc::ecc::private_key peerplays_sidechain_plugin_impl::get_private_key(chain::public_key_type public_key) {
   auto private_key_itr = private_keys.find(public_key);
   if (private_key_itr != private_keys.end()) {
      return private_key_itr->second;
   }
   return {};
}

void peerplays_sidechain_plugin_impl::schedule_heartbeat_loop() {
   fc::time_point now = fc::time_point::now();
   int64_t time_to_next_heartbeat = plugin.database().get_global_properties().parameters.son_heartbeat_frequency();

   fc::time_point next_wakeup(now + fc::seconds(time_to_next_heartbeat));

   _heartbeat_task = fc::schedule([this] {
      heartbeat_loop();
   },
                                  next_wakeup, "SON Heartbeat Production");
}

void peerplays_sidechain_plugin_impl::heartbeat_loop() {
   schedule_heartbeat_loop();
   chain::database &d = plugin.database();

   for (son_id_type son_id : sons) {
      const auto &son_obj = get_son_object(son_id);

      //! Check that son is in_maintenance
      bool status_in_maintenance = false;
      for (const auto &status : son_obj.statuses) {
         if ((status.second == son_status::in_maintenance))
            status_in_maintenance = true;
      }

      //! Check that son is active (at least for one sidechain_type)
      bool is_son_active = false;
      for (const auto &active_sidechain_type : active_sidechain_types) {
         if (sidechain_enabled.at(active_sidechain_type)) {
            if (is_active_son(active_sidechain_type, son_id))
               is_son_active = true;
         }
      }

      if (is_son_active || status_in_maintenance) {

         ilog("Sending heartbeat for SON ${son}", ("son", son_id));
         chain::son_heartbeat_operation op;
         op.owner_account = get_son_object(son_id).son_account;
         op.son_id = son_id;
         op.ts = fc::time_point::now() + fc::seconds(0);
         chain::signed_transaction trx = d.create_signed_transaction(plugin.get_private_key(son_id), op);
         fc::future<bool> fut = fc::async([&]() {
            try {
               trx.validate();
               d.push_transaction(trx, database::validation_steps::skip_block_size_check);
               if (plugin.app().p2p_node())
                  plugin.app().p2p_node()->broadcast(net::trx_message(trx));
               return true;
            } catch (fc::exception &e) {
               elog("Sending heartbeat failed with exception ${e}", ("e", e.what()));
               return false;
            }
         });
         fut.wait(fc::seconds(10));
      }
   }
}

void peerplays_sidechain_plugin_impl::schedule_son_processing() {
   const auto now = std::chrono::steady_clock::now();
   static const int64_t time_to_next_son_processing = 500000;

   const auto next_wakeup = now + std::chrono::microseconds(time_to_next_son_processing);

   for (const auto &active_sidechain_type : active_sidechain_types) {
      if (_son_processing_task.count(active_sidechain_type) != 0 && _son_processing_task.at(active_sidechain_type).wait_for(std::chrono::seconds{0}) != std::future_status::ready) {
         wlog("Son doesn't process in time for sidechain: ${active_sidechain_type}", ("active_sidechain_type", active_sidechain_type));
         _son_processing_task.at(active_sidechain_type).wait();
      }

      _son_processing_task[active_sidechain_type] = std::async(std::launch::async, [this, next_wakeup, active_sidechain_type] {
         if (sidechain_enabled.at(active_sidechain_type)) {
            std::this_thread::sleep_until(next_wakeup);
            son_processing(active_sidechain_type);
         }
      });
   }
}

void peerplays_sidechain_plugin_impl::son_processing(sidechain_type sidechain) {
   //! Check whether we have active SONs
   if (plugin.database().get_global_properties().active_sons.count(sidechain) == 0 ||
       plugin.database().get_global_properties().active_sons.at(sidechain).empty()) {
      return;
   }

   //fc::time_point now_fine = fc::time_point::now();
   //fc::time_point_sec now = now_fine + fc::microseconds(500000);
   //if (plugin.database().get_slot_time(1) < now) {
   //   return; // Not synced
   //}

   const fc::time_point now_fine = fc::time_point::now();
   const fc::time_point_sec now = now_fine - fc::milliseconds(3000);
   if (plugin.database().head_block_time() < now) {
      return; // Not synced
   }

   //! Get scheduled_son_id according to sidechain_type
   const chain::son_id_type scheduled_son_id = plugin.database().get_scheduled_son(sidechain, 1);
   ilog("Scheduled SON: ${scheduled_son_id} Sidechain: ${sidechain} Now: ${now}",
        ("scheduled_son_id", scheduled_son_id)("sidechain", sidechain)("now", now));

   for (son_id_type son_id : plugin.get_sons()) {
      if (plugin.is_son_deregistered(son_id)) {
         continue;
      }

      {
         const std::lock_guard<std::mutex> lock(current_son_id_mutex);
         current_son_id.at(sidechain) = son_id;
      }

      // These tasks are executed by
      // - All active SONs, no matter if scheduled
      // - All previously active SONs
      approve_proposals(sidechain);
      process_proposals(sidechain);
      process_sidechain_transactions(sidechain);

      if (plugin.is_active_son(sidechain, son_id)) {
         // Tasks that are executed by scheduled and active SON only
         if (get_current_son_id(sidechain) == scheduled_son_id) {

            create_son_down_proposals(sidechain);

            create_son_deregister_proposals(sidechain);

            process_active_sons_change(sidechain);

            create_deposit_addresses(sidechain);

            process_deposits(sidechain);

            process_withdrawals(sidechain);

            process_sidechain_transactions(sidechain);

            send_sidechain_transactions(sidechain);

            settle_sidechain_transactions(sidechain);
         }
      }
   }
}

bool peerplays_sidechain_plugin_impl::is_valid_son_proposal(const chain::proposal_object &proposal) {
   if (proposal.proposed_transaction.operations.size() == 1) {
      int32_t op_idx_0 = proposal.proposed_transaction.operations[0].which();
      chain::operation op = proposal.proposed_transaction.operations[0];

      if (op_idx_0 == chain::operation::tag<chain::son_report_down_operation>::value) {
         return is_son_down_op_valid(op);
      }

      if (op_idx_0 == chain::operation::tag<chain::son_deregister_operation>::value) {
         return is_son_deregister_op_valid(op);
      }
   }

   return false;
}

void peerplays_sidechain_plugin_impl::log_son_proposal_retry(sidechain_type sidechain, int op_type, object_id_type object_id) {
   son_proposal_type prop_type(op_type, sidechain, get_current_son_id(sidechain), object_id);
   auto itr = son_retry_count.find(prop_type);
   if (itr != son_retry_count.end()) {
      itr->second++;
   } else {
      son_retry_count[prop_type] = 1;
   }
}

bool peerplays_sidechain_plugin_impl::can_son_participate(sidechain_type sidechain, int op_type, object_id_type object_id) {
   son_proposal_type prop_type(op_type, sidechain, get_current_son_id(sidechain), object_id);
   auto itr = son_retry_count.find(prop_type);
   return (itr == son_retry_count.end() || itr->second < retries_threshold);
}

std::map<sidechain_type, std::vector<std::string>> peerplays_sidechain_plugin_impl::get_son_listener_log() {
   std::map<sidechain_type, std::vector<std::string>> result;
   for (const auto &active_sidechain_type : active_sidechain_types) {
      if (net_handlers.at(active_sidechain_type)) {
         result.emplace(active_sidechain_type, net_handlers.at(active_sidechain_type)->get_son_listener_log());
      }
   }
   return result;
}

optional<asset> peerplays_sidechain_plugin_impl::estimate_withdrawal_transaction_fee(sidechain_type sidechain) {
   if (!net_handlers.at(sidechain)) {
      wlog("Net handler is null for sidechain: ${sidechain}", ("sidechain", sidechain));
      return optional<asset>();
   }

   return net_handlers.at(sidechain)->estimate_withdrawal_transaction_fee();
}

void peerplays_sidechain_plugin_impl::approve_proposals(sidechain_type sidechain) {
   // prevent approving duplicate proposals with lock for parallel execution.
   // We can have the same propsals, but in the case of parallel execution we can run
   // into problem of approving the same propsal since it might happens that previous
   // approved proposal didn't have time or chance to populate the list of available
   // active proposals which is consulted here in the code.
   const std::lock_guard<std::mutex> lck{access_approve_prop_mutex};
   auto check_approve_proposal = [&](const chain::son_id_type &son_id, const chain::proposal_object &proposal) {
      if (!is_valid_son_proposal(proposal)) {
         return;
      }
      ilog("Sending approval for ${p} from ${s}", ("p", proposal.id)("s", son_id));
      chain::proposal_update_operation puo;
      puo.fee_paying_account = get_son_object(son_id).son_account;
      puo.proposal = proposal.id;
      puo.active_approvals_to_add = {get_son_object(son_id).son_account};
      chain::signed_transaction trx = plugin.database().create_signed_transaction(plugin.get_private_key(son_id), puo);
      fc::future<bool> fut = fc::async([&]() {
         try {
            trx.validate();
            std::lock_guard<std::mutex> lck(access_db_mutex);
            plugin.database().push_transaction(trx, database::validation_steps::skip_block_size_check);
            if (plugin.app().p2p_node())
               plugin.app().p2p_node()->broadcast(net::trx_message(trx));
            return true;
         } catch (fc::exception &e) {
            elog("Sending approval failed with exception ${e}", ("e", e.what()));
            return false;
         }
      });
      fut.wait(fc::seconds(10));
   };

   const auto &idx = plugin.database().get_index_type<proposal_index>().indices().get<by_id>();
   vector<proposal_id_type> proposals;
   for (const auto &proposal : idx) {
      proposals.push_back(proposal.id);
   }

   for (const auto proposal_id : proposals) {
      const object *obj = plugin.database().find_object(proposal_id);
      const chain::proposal_object *proposal_ptr = dynamic_cast<const chain::proposal_object *>(obj);
      if (proposal_ptr == nullptr) {
         continue;
      }
      const proposal_object proposal = *proposal_ptr;

      if (proposal.available_active_approvals.find(get_current_son_object(sidechain).son_account) != proposal.available_active_approvals.end()) {
         continue;
      }

      check_approve_proposal(get_current_son_id(sidechain), proposal);
   }
}

void peerplays_sidechain_plugin_impl::create_son_down_proposals(sidechain_type sidechain) {
   const std::lock_guard<std::mutex> lck{access_son_down_prop_mutex};
   auto create_son_down_proposal = [&](chain::son_id_type son_id, fc::time_point_sec last_active_ts) {
      chain::database &d = plugin.database();
      const chain::global_property_object &gpo = d.get_global_properties();

      chain::son_report_down_operation son_down_op;
      son_down_op.payer = d.get_global_properties().parameters.son_account();
      son_down_op.son_id = son_id;
      son_down_op.down_ts = last_active_ts;

      proposal_create_operation proposal_op;
      proposal_op.fee_paying_account = get_current_son_object(sidechain).son_account;
      proposal_op.proposed_ops.emplace_back(op_wrapper(son_down_op));
      const uint32_t lifetime = (gpo.parameters.block_interval * gpo.active_witnesses.size()) * 3;
      proposal_op.expiration_time = time_point_sec(d.head_block_time().sec_since_epoch() + lifetime);
      return proposal_op;
   };

   chain::database &d = plugin.database();
   const chain::global_property_object &gpo = d.get_global_properties();
   const chain::dynamic_global_property_object &dgpo = d.get_dynamic_global_properties();
   const auto &idx = d.get_index_type<chain::son_index>().indices().get<by_id>();
   const std::set<son_id_type> sons_being_reported_down = d.get_sons_being_reported_down();
   const chain::son_id_type my_son_id = get_current_son_id(sidechain);

   //! Fixme - check this part of the code
   for (auto son_inf : gpo.active_sons.at(sidechain)) {
      if (my_son_id == son_inf.son_id || (sons_being_reported_down.find(son_inf.son_id) != sons_being_reported_down.end())) {
         continue;
      }

      const auto son_obj = idx.find(son_inf.son_id);
      const auto stats = son_obj->statistics(d);
      const fc::time_point_sec last_maintenance_time = dgpo.next_maintenance_time - gpo.parameters.maintenance_interval;
      const fc::time_point_sec last_active_ts = [&stats, &sidechain, &last_maintenance_time] {
         fc::time_point_sec last_active_ts;
         if (stats.last_active_timestamp.contains(sidechain)) {
            last_active_ts = (stats.last_active_timestamp.at(sidechain) > last_maintenance_time) ? stats.last_active_timestamp.at(sidechain) : last_maintenance_time;
         } else
            last_active_ts = last_maintenance_time;
         return last_active_ts;
      }();
      const int64_t down_threshold = gpo.parameters.son_down_time();

      bool status_son_down_valid = true;
      for (const auto &status : son_obj->statuses) {
         if ((status.second != son_status::active) && (status.second != son_status::request_maintenance))
            status_son_down_valid = false;
      }
      if ((status_son_down_valid) && ((fc::time_point::now() - last_active_ts) > fc::seconds(down_threshold))) {
         ilog("Sending son down proposal for ${t} from ${s}", ("t", std::string(object_id_type(son_obj->id)))("s", std::string(object_id_type(my_son_id))));
         chain::proposal_create_operation op = create_son_down_proposal(son_inf.son_id, last_active_ts);
         chain::signed_transaction trx = d.create_signed_transaction(plugin.get_private_key(get_son_object(my_son_id).signing_key), op);
         fc::future<bool> fut = fc::async([&]() {
            try {
               trx.validate();
               std::lock_guard<std::mutex> lck(access_db_mutex);
               d.push_transaction(trx, database::validation_steps::skip_block_size_check);
               if (plugin.app().p2p_node())
                  plugin.app().p2p_node()->broadcast(net::trx_message(trx));
               return true;
            } catch (fc::exception &e) {
               elog("Sending son down proposal failed with exception ${e}", ("e", e.what()));
               return false;
            }
         });
         fut.wait(fc::seconds(10));
      }
   }
}

void peerplays_sidechain_plugin_impl::create_son_deregister_proposals(sidechain_type sidechain) {
   const std::lock_guard<std::mutex> lck{access_son_deregister_prop_mutex};
   chain::database &d = plugin.database();
   std::set<son_id_type> sons_to_be_dereg = d.get_sons_to_be_deregistered();
   chain::son_id_type my_son_id = get_current_son_id(sidechain);

   if (sons_to_be_dereg.size() > 0) {
      // We shouldn't raise proposals for the SONs for which a de-reg
      // proposal is already raised.
      std::set<son_id_type> sons_being_dereg = d.get_sons_being_deregistered();
      for (auto &son : sons_to_be_dereg) {
         // New SON to be deregistered
         if (sons_being_dereg.find(son) == sons_being_dereg.end() && my_son_id != son) {
            // Creating the de-reg proposal
            auto op = d.create_son_deregister_proposal(son, get_son_object(my_son_id).son_account);
            if (op.valid()) {
               // Signing and pushing into the txs to be included in the block
               ilog("Sending son deregister proposal for ${p} from ${s}", ("p", son)("s", my_son_id));
               chain::signed_transaction trx = d.create_signed_transaction(plugin.get_private_key(get_son_object(my_son_id).signing_key), *op);
               fc::future<bool> fut = fc::async([&]() {
                  try {
                     trx.validate();
                     std::lock_guard<std::mutex> lck(access_db_mutex);
                     d.push_transaction(trx, database::validation_steps::skip_block_size_check);
                     if (plugin.app().p2p_node())
                        plugin.app().p2p_node()->broadcast(net::trx_message(trx));
                     return true;
                  } catch (fc::exception &e) {
                     elog("Sending son deregister proposal failed with exception ${e}", ("e", e.what()));
                     return false;
                  }
               });
               fut.wait(fc::seconds(10));
            }
         }
      }
   }
}

void peerplays_sidechain_plugin_impl::process_proposals(sidechain_type sidechain) {
   if (net_handlers.at(sidechain)) {
      net_handlers.at(sidechain)->process_proposals();
   }
}

void peerplays_sidechain_plugin_impl::process_active_sons_change(sidechain_type sidechain) {
   if (net_handlers.at(sidechain)) {
      net_handlers.at(sidechain)->process_active_sons_change();
   }
}

void peerplays_sidechain_plugin_impl::create_deposit_addresses(sidechain_type sidechain) {
   if (net_handlers.at(sidechain)) {
      net_handlers.at(sidechain)->create_deposit_addresses();
   }
}

void peerplays_sidechain_plugin_impl::process_deposits(sidechain_type sidechain) {
   if (net_handlers.at(sidechain)) {
      net_handlers.at(sidechain)->process_deposits();
   }
}

void peerplays_sidechain_plugin_impl::process_withdrawals(sidechain_type sidechain) {
   if (net_handlers.at(sidechain)) {
      net_handlers.at(sidechain)->process_withdrawals();
   }
}

void peerplays_sidechain_plugin_impl::process_sidechain_transactions(sidechain_type sidechain) {
   if (net_handlers.at(sidechain)) {
      net_handlers.at(sidechain)->process_sidechain_transactions();
   }
}

void peerplays_sidechain_plugin_impl::send_sidechain_transactions(sidechain_type sidechain) {
   if (net_handlers.at(sidechain)) {
      net_handlers.at(sidechain)->send_sidechain_transactions();
   }
}

void peerplays_sidechain_plugin_impl::settle_sidechain_transactions(sidechain_type sidechain) {
   if (net_handlers.at(sidechain)) {
      net_handlers.at(sidechain)->settle_sidechain_transactions();
   }
}

void peerplays_sidechain_plugin_impl::on_applied_block(const signed_block &b) {
   if (first_block_skipped) {
      schedule_son_processing();
   } else {
      first_block_skipped = true;
   }
}

} // namespace detail

peerplays_sidechain_plugin::peerplays_sidechain_plugin() :
      my(new detail::peerplays_sidechain_plugin_impl(*this)) {
}

peerplays_sidechain_plugin::~peerplays_sidechain_plugin() {
   return;
}

std::string peerplays_sidechain_plugin::plugin_name() const {
   return "peerplays_sidechain";
}

void peerplays_sidechain_plugin::plugin_set_program_options(
      boost::program_options::options_description &cli,
      boost::program_options::options_description &cfg) {
   my->plugin_set_program_options(cli, cfg);
}

void peerplays_sidechain_plugin::plugin_initialize(const boost::program_options::variables_map &options) {
   ilog("peerplays sidechain plugin:  plugin_initialize() begin");
   my->plugin_initialize(options);
   ilog("peerplays sidechain plugin:  plugin_initialize() end");
}

void peerplays_sidechain_plugin::plugin_startup() {
   ilog("peerplays sidechain plugin:  plugin_startup() begin");
   my->plugin_startup();
   ilog("peerplays sidechain plugin:  plugin_startup() end");
}

void peerplays_sidechain_plugin::plugin_shutdown() {
   ilog("peerplays sidechain plugin:  plugin_shutdown() begin");
   my->plugin_shutdown();
   ilog("peerplays sidechain plugin:  plugin_shutdown() end");
}

std::set<chain::son_id_type> &peerplays_sidechain_plugin::get_sons() {
   return my->get_sons();
}

const son_id_type peerplays_sidechain_plugin::get_current_son_id(sidechain_type sidechain) {
   return my->get_current_son_id(sidechain);
}

const son_object peerplays_sidechain_plugin::get_current_son_object(sidechain_type sidechain) {
   return my->get_current_son_object(sidechain);
}

const son_object peerplays_sidechain_plugin::get_son_object(son_id_type son_id) {
   return my->get_son_object(son_id);
}

bool peerplays_sidechain_plugin::is_active_son(sidechain_type sidechain, son_id_type son_id) {
   return my->is_active_son(sidechain, son_id);
}

bool peerplays_sidechain_plugin::is_son_deregistered(son_id_type son_id) {
   return my->is_son_deregistered(son_id);
}

fc::ecc::private_key peerplays_sidechain_plugin::get_private_key(son_id_type son_id) {
   return my->get_private_key(son_id);
}

fc::ecc::private_key peerplays_sidechain_plugin::get_private_key(chain::public_key_type public_key) {
   return my->get_private_key(public_key);
}

void peerplays_sidechain_plugin::log_son_proposal_retry(sidechain_type sidechain, int op_type, object_id_type object_id) {
   my->log_son_proposal_retry(sidechain, op_type, object_id);
}

bool peerplays_sidechain_plugin::can_son_participate(sidechain_type sidechain, int op_type, object_id_type object_id) {
   return my->can_son_participate(sidechain, op_type, object_id);
}

std::map<sidechain_type, std::vector<std::string>> peerplays_sidechain_plugin::get_son_listener_log() {
   return my->get_son_listener_log();
}

optional<asset> peerplays_sidechain_plugin::estimate_withdrawal_transaction_fee(sidechain_type sidechain) {
   return my->estimate_withdrawal_transaction_fee(sidechain);
}

}} // namespace graphene::peerplays_sidechain
