#include <graphene/peerplays_sidechain/peerplays_sidechain_plugin.hpp>

#include <boost/property_tree/json_parser.hpp>
#include <boost/property_tree/ptree.hpp>
#include <boost/range/algorithm_ext/insert.hpp>

#include <fc/log/logger.hpp>
#include <fc/smart_ref_impl.hpp>

#include <graphene/chain/proposal_object.hpp>
#include <graphene/chain/protocol/transfer.hpp>
#include <graphene/chain/sidechain_address_object.hpp>
#include <graphene/chain/son_wallet_object.hpp>
#include <graphene/chain/son_wallet_withdraw_object.hpp>
#include <graphene/peerplays_sidechain/sidechain_net_manager.hpp>
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

   std::set<chain::son_id_type> &get_sons();
   son_id_type &get_current_son_id();
   son_object get_son_object(son_id_type son_id);
   bool is_active_son(son_id_type son_id);
   fc::ecc::private_key get_private_key(son_id_type son_id);
   fc::ecc::private_key get_private_key(chain::public_key_type public_key);

   void schedule_heartbeat_loop();
   void heartbeat_loop();
   void schedule_son_processing();
   void son_processing();
   void approve_proposals();
   void create_son_down_proposals();
   void create_son_deregister_proposals();
   void recreate_primary_wallet();
   void process_deposits();
   void process_withdrawals();

private:
   peerplays_sidechain_plugin &plugin;

   boost::program_options::variables_map options;

   bool config_ready_son;
   bool config_ready_bitcoin;
   bool config_ready_peerplays;

   son_id_type current_son_id;

   std::unique_ptr<peerplays_sidechain::sidechain_net_manager> net_manager;
   std::set<chain::son_id_type> sons;
   std::map<chain::public_key_type, fc::ecc::private_key> private_keys;
   fc::future<void> _heartbeat_task;
   fc::future<void> _son_processing_task;

   void on_applied_block(const signed_block &b);
};

peerplays_sidechain_plugin_impl::peerplays_sidechain_plugin_impl(peerplays_sidechain_plugin &_plugin) :
      plugin(_plugin),
      config_ready_son(false),
      config_ready_bitcoin(false),
      config_ready_peerplays(false),
      current_son_id(son_id_type(std::numeric_limits<uint32_t>().max())),
      net_manager(nullptr) {
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
      if (_son_processing_task.valid())
         _son_processing_task.cancel_and_wait(__FUNCTION__);
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
   cli.add_options()("bitcoin-node-ip", bpo::value<string>()->default_value("99.79.189.95"), "IP address of Bitcoin node");
   cli.add_options()("bitcoin-node-zmq-port", bpo::value<uint32_t>()->default_value(11111), "ZMQ port of Bitcoin node");
   cli.add_options()("bitcoin-node-rpc-port", bpo::value<uint32_t>()->default_value(22222), "RPC port of Bitcoin node");
   cli.add_options()("bitcoin-node-rpc-user", bpo::value<string>()->default_value("1"), "Bitcoin RPC user");
   cli.add_options()("bitcoin-node-rpc-password", bpo::value<string>()->default_value("1"), "Bitcoin RPC password");
   cli.add_options()("bitcoin-wallet", bpo::value<string>(), "Bitcoin wallet");
   cli.add_options()("bitcoin-wallet-password", bpo::value<string>(), "Bitcoin wallet password");
   cli.add_options()("bitcoin-private-key", bpo::value<vector<string>>()->composing()->multitoken()->DEFAULT_VALUE_VECTOR(std::make_pair("02d0f137e717fb3aab7aff99904001d49a0a636c5e1342f8927a4ba2eaee8e9772", "cVN31uC9sTEr392DLVUEjrtMgLA8Yb3fpYmTRj7bomTm6nn2ANPr")),
                     "Tuple of [Bitcoin public key, Bitcoin private key] (may specify multiple times)");
   cfg.add(cli);
}

void peerplays_sidechain_plugin_impl::plugin_initialize(const boost::program_options::variables_map &opt) {
   options = opt;
   config_ready_son = (options.count("son-id") || options.count("son-ids")) && options.count("peerplays-private-key");
   if (config_ready_son) {
      LOAD_VALUE_SET(options, "son-id", sons, chain::son_id_type)
      if (options.count("son-ids"))
         boost::insert(sons, fc::json::from_string(options.at("son-ids").as<string>()).as<vector<chain::son_id_type>>(5));
      config_ready_son = config_ready_son && !sons.empty();

#ifndef SUPPORT_MULTIPLE_SONS
      FC_ASSERT(sons.size() == 1, "Multiple SONs not supported");
#endif

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
   }
   if (!config_ready_son) {
      wlog("Haven't set up SON parameters");
      throw;
   }

   config_ready_bitcoin = options.count("bitcoin-node-ip") &&
                          options.count("bitcoin-node-zmq-port") && options.count("bitcoin-node-rpc-port") &&
                          options.count("bitcoin-node-rpc-user") && options.count("bitcoin-node-rpc-password") &&
                          /*options.count( "bitcoin-wallet" ) && options.count( "bitcoin-wallet-password" ) &&*/
                          options.count("bitcoin-private-key");
   if (!config_ready_bitcoin) {
      wlog("Haven't set up Bitcoin sidechain parameters");
   }

   //config_ready_ethereum = options.count( "ethereum-node-ip" ) &&
   //        options.count( "ethereum-address" ) && options.count( "ethereum-public-key" ) && options.count( "ethereum-private-key" );
   //if (!config_ready_ethereum) {
   //   wlog("Haven't set up Ethereum sidechain parameters");
   //}

   config_ready_peerplays = true;
   if (!config_ready_peerplays) {
      wlog("Haven't set up Peerplays sidechain parameters");
   }

   if (!(config_ready_bitcoin /*&& config_ready_ethereum*/ && config_ready_peerplays)) {
      wlog("Haven't set up any sidechain parameters");
      throw;
   }
}

void peerplays_sidechain_plugin_impl::plugin_startup() {

   if (config_ready_son) {
      ilog("Starting ${n} SON instances", ("n", sons.size()));

      schedule_heartbeat_loop();
   } else {
      elog("No sons configured! Please add SON IDs and private keys to configuration.");
   }

   net_manager = std::unique_ptr<sidechain_net_manager>(new sidechain_net_manager(plugin));

   if (config_ready_bitcoin) {
      net_manager->create_handler(sidechain_type::bitcoin, options);
      ilog("Bitcoin sidechain handler running");
   }

   //if (config_ready_ethereum) {
   //   net_manager->create_handler(sidechain_type::ethereum, options);
   //   ilog("Ethereum sidechain handler running");
   //}

   if (config_ready_peerplays) {
      net_manager->create_handler(sidechain_type::peerplays, options);
      ilog("Peerplays sidechain handler running");
   }

   plugin.database().applied_block.connect([&](const signed_block &b) {
      on_applied_block(b);
   });
}

std::set<chain::son_id_type> &peerplays_sidechain_plugin_impl::get_sons() {
   return sons;
}

son_id_type &peerplays_sidechain_plugin_impl::get_current_son_id() {
   return current_son_id;
}

son_object peerplays_sidechain_plugin_impl::get_son_object(son_id_type son_id) {
   const auto &idx = plugin.database().get_index_type<chain::son_index>().indices().get<by_id>();
   auto son_obj = idx.find(son_id);
   if (son_obj == idx.end())
      return {};
   return *son_obj;
}

bool peerplays_sidechain_plugin_impl::is_active_son(son_id_type son_id) {
   const auto &idx = plugin.database().get_index_type<chain::son_index>().indices().get<by_id>();
   auto son_obj = idx.find(son_id);
   if (son_obj == idx.end())
      return false;

   const chain::global_property_object &gpo = plugin.database().get_global_properties();
   vector<son_id_type> active_son_ids;
   active_son_ids.reserve(gpo.active_sons.size());
   std::transform(gpo.active_sons.begin(), gpo.active_sons.end(),
                  std::inserter(active_son_ids, active_son_ids.end()),
                  [](const son_info &swi) {
                     return swi.son_id;
                  });

   auto it = std::find(active_son_ids.begin(), active_son_ids.end(), son_id);

   return (it != active_son_ids.end());
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
      if (is_active_son(son_id) || get_son_object(son_id).status == chain::son_status::in_maintenance) {

         ilog("peerplays_sidechain_plugin:  sending heartbeat for SON ${son}", ("son", son_id));
         chain::son_heartbeat_operation op;
         op.owner_account = get_son_object(son_id).son_account;
         op.son_id = son_id;
         op.ts = fc::time_point::now() + fc::seconds(0);
         chain::signed_transaction trx = d.create_signed_transaction(plugin.get_private_key(son_id), op);
         fc::future<bool> fut = fc::async([&]() {
            try {
               d.push_transaction(trx, database::validation_steps::skip_block_size_check);
               if (plugin.app().p2p_node())
                  plugin.app().p2p_node()->broadcast(net::trx_message(trx));
               return true;
            } catch (fc::exception e) {
               ilog("peerplays_sidechain_plugin_impl:  sending heartbeat failed with exception ${e}", ("e", e.what()));
               return false;
            }
         });
         fut.wait(fc::seconds(10));
      }
   }
}

void peerplays_sidechain_plugin_impl::schedule_son_processing() {
   fc::time_point now = fc::time_point::now();
   int64_t time_to_next_son_processing = 500000;

   fc::time_point next_wakeup(now + fc::microseconds(time_to_next_son_processing));

   _son_processing_task = fc::schedule([this] {
      son_processing();
   },
                                       next_wakeup, "SON Processing");
}

void peerplays_sidechain_plugin_impl::son_processing() {
   if (plugin.database().get_global_properties().active_sons.size() <= 0) {
      return;
   }

   chain::son_id_type next_son_id = plugin.database().get_scheduled_son(1);
   ilog("peerplays_sidechain_plugin_impl:  Scheduled SON ${son}", ("son", next_son_id));

   // Tasks that are executed by all active SONs, no matter if scheduled
   // E.g. sending approvals and signing
   approve_proposals();

   // Tasks that are executed by scheduled and active SON
   if (sons.find(next_son_id) != sons.end()) {

      current_son_id = next_son_id;

      create_son_down_proposals();

      create_son_deregister_proposals();

      recreate_primary_wallet();

      process_deposits();

      process_withdrawals();
   }
}

void peerplays_sidechain_plugin_impl::approve_proposals() {

   auto approve_proposal = [&](const chain::son_id_type &son_id, const chain::proposal_id_type &proposal_id) {
      ilog("peerplays_sidechain_plugin:  sending approval for ${p} from ${s}", ("p", proposal_id)("s", son_id));
      chain::proposal_update_operation puo;
      puo.fee_paying_account = get_son_object(son_id).son_account;
      puo.proposal = proposal_id;
      puo.active_approvals_to_add = {get_son_object(son_id).son_account};
      chain::signed_transaction trx = plugin.database().create_signed_transaction(plugin.get_private_key(son_id), puo);
      fc::future<bool> fut = fc::async([&]() {
         try {
            plugin.database().push_transaction(trx, database::validation_steps::skip_block_size_check);
            if (plugin.app().p2p_node())
               plugin.app().p2p_node()->broadcast(net::trx_message(trx));
            return true;
         } catch (fc::exception e) {
            ilog("peerplays_sidechain_plugin_impl:  sending approval failed with exception ${e}", ("e", e.what()));
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
      for (son_id_type son_id : sons) {
         if (!is_active_son(son_id)) {
            continue;
         }

         const object *obj = plugin.database().find_object(proposal_id);
         const chain::proposal_object *proposal_ptr = dynamic_cast<const chain::proposal_object *>(obj);
         if (proposal_ptr == nullptr) {
            continue;
         }
         const proposal_object proposal = *proposal_ptr;

         if (proposal.available_active_approvals.find(get_son_object(son_id).son_account) != proposal.available_active_approvals.end()) {
            continue;
         }

         if (proposal.proposed_transaction.operations.size() == 1 && proposal.proposed_transaction.operations[0].which() == chain::operation::tag<chain::son_report_down_operation>::value) {
            approve_proposal(son_id, proposal.id);
            continue;
         }

         if (proposal.proposed_transaction.operations.size() == 1 && proposal.proposed_transaction.operations[0].which() == chain::operation::tag<chain::son_delete_operation>::value) {
            approve_proposal(son_id, proposal.id);
            continue;
         }

         if (proposal.proposed_transaction.operations.size() == 1 && proposal.proposed_transaction.operations[0].which() == chain::operation::tag<chain::son_wallet_update_operation>::value) {
            approve_proposal(son_id, proposal.id);
            continue;
         }

         if (proposal.proposed_transaction.operations.size() == 1 && proposal.proposed_transaction.operations[0].which() == chain::operation::tag<chain::son_wallet_deposit_create_operation>::value) {
            approve_proposal(son_id, proposal.id);
            continue;
         }

         if (proposal.proposed_transaction.operations.size() == 1 && proposal.proposed_transaction.operations[0].which() == chain::operation::tag<chain::son_wallet_deposit_process_operation>::value) {
            approve_proposal(son_id, proposal.id);
            continue;
         }

         if (proposal.proposed_transaction.operations.size() == 1 && proposal.proposed_transaction.operations[0].which() == chain::operation::tag<chain::son_wallet_withdraw_create_operation>::value) {
            approve_proposal(son_id, proposal.id);
            continue;
         }

         if (proposal.proposed_transaction.operations.size() == 1 && proposal.proposed_transaction.operations[0].which() == chain::operation::tag<chain::son_wallet_withdraw_process_operation>::value) {
            approve_proposal(son_id, proposal.id);
            continue;
         }
      }
   }
}

void peerplays_sidechain_plugin_impl::create_son_down_proposals() {
   auto create_son_down_proposal = [&](chain::son_id_type son_id, fc::time_point_sec last_active_ts) {
      chain::database &d = plugin.database();
      const chain::global_property_object &gpo = d.get_global_properties();

      chain::son_report_down_operation son_down_op;
      son_down_op.payer = GRAPHENE_SON_ACCOUNT;
      son_down_op.son_id = son_id;
      son_down_op.down_ts = last_active_ts;

      proposal_create_operation proposal_op;
      proposal_op.fee_paying_account = get_son_object(plugin.get_current_son_id()).son_account;
      proposal_op.proposed_ops.push_back(op_wrapper(son_down_op));
      uint32_t lifetime = (gpo.parameters.block_interval * gpo.active_witnesses.size()) * 3;
      proposal_op.expiration_time = time_point_sec(d.head_block_time().sec_since_epoch() + lifetime);
      return proposal_op;
   };

   chain::database &d = plugin.database();
   const chain::global_property_object &gpo = d.get_global_properties();
   const chain::dynamic_global_property_object &dgpo = d.get_dynamic_global_properties();
   const auto &idx = d.get_index_type<chain::son_index>().indices().get<by_id>();
   std::set<son_id_type> sons_being_reported_down = d.get_sons_being_reported_down();
   chain::son_id_type my_son_id = get_current_son_id();
   for (auto son_inf : gpo.active_sons) {
      if (my_son_id == son_inf.son_id || (sons_being_reported_down.find(son_inf.son_id) != sons_being_reported_down.end())) {
         continue;
      }
      auto son_obj = idx.find(son_inf.son_id);
      auto stats = son_obj->statistics(d);
      fc::time_point_sec last_maintenance_time = dgpo.next_maintenance_time - gpo.parameters.maintenance_interval;
      fc::time_point_sec last_active_ts = ((stats.last_active_timestamp > last_maintenance_time) ? stats.last_active_timestamp : last_maintenance_time);
      int64_t down_threshold = gpo.parameters.son_down_time();
      if (((son_obj->status == chain::son_status::active) || (son_obj->status == chain::son_status::request_maintenance)) &&
          ((fc::time_point::now() - last_active_ts) > fc::seconds(down_threshold))) {
         ilog("peerplays_sidechain_plugin:  sending son down proposal for ${t} from ${s}", ("t", std::string(object_id_type(son_obj->id)))("s", std::string(object_id_type(my_son_id))));
         chain::proposal_create_operation op = create_son_down_proposal(son_inf.son_id, last_active_ts);
         chain::signed_transaction trx = d.create_signed_transaction(plugin.get_private_key(get_son_object(my_son_id).signing_key), op);
         fc::future<bool> fut = fc::async([&]() {
            try {
               d.push_transaction(trx, database::validation_steps::skip_block_size_check);
               if (plugin.app().p2p_node())
                  plugin.app().p2p_node()->broadcast(net::trx_message(trx));
               return true;
            } catch (fc::exception e) {
               ilog("peerplays_sidechain_plugin_impl:  sending son down proposal failed with exception ${e}", ("e", e.what()));
               return false;
            }
         });
         fut.wait(fc::seconds(10));
      }
   }
}

void peerplays_sidechain_plugin_impl::create_son_deregister_proposals() {
   chain::database &d = plugin.database();
   std::set<son_id_type> sons_to_be_dereg = d.get_sons_to_be_deregistered();
   chain::son_id_type my_son_id = get_current_son_id();

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
               ilog("peerplays_sidechain_plugin:  sending son deregister proposal for ${p} from ${s}", ("p", son)("s", my_son_id));
               chain::signed_transaction trx = d.create_signed_transaction(plugin.get_private_key(get_son_object(my_son_id).signing_key), *op);
               fc::future<bool> fut = fc::async([&]() {
                  try {
                     d.push_transaction(trx, database::validation_steps::skip_block_size_check);
                     if (plugin.app().p2p_node())
                        plugin.app().p2p_node()->broadcast(net::trx_message(trx));
                     return true;
                  } catch (fc::exception e) {
                     ilog("peerplays_sidechain_plugin_impl:  sending son dereg proposal failed with exception ${e}", ("e", e.what()));
                     return false;
                  }
               });
               fut.wait(fc::seconds(10));
            }
         }
      }
   }
}

void peerplays_sidechain_plugin_impl::recreate_primary_wallet() {
   net_manager->recreate_primary_wallet();
}

void peerplays_sidechain_plugin_impl::process_deposits() {
   net_manager->process_deposits();
}

void peerplays_sidechain_plugin_impl::process_withdrawals() {
   net_manager->process_withdrawals();
}

void peerplays_sidechain_plugin_impl::on_applied_block(const signed_block &b) {
   schedule_son_processing();
}

} // end namespace detail

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
   ilog("peerplays sidechain plugin:  plugin_initialize()");
   my->plugin_initialize(options);
}

void peerplays_sidechain_plugin::plugin_startup() {
   ilog("peerplays sidechain plugin:  plugin_startup()");
   my->plugin_startup();
}

std::set<chain::son_id_type> &peerplays_sidechain_plugin::get_sons() {
   return my->get_sons();
}

son_id_type &peerplays_sidechain_plugin::get_current_son_id() {
   return my->get_current_son_id();
}

son_object peerplays_sidechain_plugin::get_son_object(son_id_type son_id) {
   return my->get_son_object(son_id);
}

bool peerplays_sidechain_plugin::is_active_son(son_id_type son_id) {
   return my->is_active_son(son_id);
}

fc::ecc::private_key peerplays_sidechain_plugin::get_private_key(son_id_type son_id) {
   return my->get_private_key(son_id);
}

fc::ecc::private_key peerplays_sidechain_plugin::get_private_key(chain::public_key_type public_key) {
   return my->get_private_key(public_key);
}

}} // namespace graphene::peerplays_sidechain
