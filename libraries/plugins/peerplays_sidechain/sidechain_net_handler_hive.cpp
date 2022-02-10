#include <graphene/peerplays_sidechain/sidechain_net_handler_hive.hpp>

#include <algorithm>
#include <iomanip>
#include <thread>

#include <boost/algorithm/hex.hpp>
#include <boost/algorithm/string.hpp>
#include <boost/property_tree/json_parser.hpp>
#include <boost/property_tree/ptree.hpp>

#include <fc/crypto/base64.hpp>
#include <fc/crypto/hex.hpp>
#include <fc/io/json.hpp>
#include <fc/log/logger.hpp>
#include <fc/network/ip.hpp>
#include <fc/smart_ref_impl.hpp>
#include <fc/time.hpp>

#include <graphene/chain/account_object.hpp>
#include <graphene/chain/protocol/fee_schedule.hpp>
#include <graphene/chain/protocol/son_wallet.hpp>
#include <graphene/chain/son_info.hpp>
#include <graphene/chain/son_wallet_object.hpp>
#include <graphene/peerplays_sidechain/common/utils.hpp>
#include <graphene/peerplays_sidechain/hive/asset.hpp>
#include <graphene/peerplays_sidechain/hive/operations.hpp>
#include <graphene/peerplays_sidechain/hive/transaction.hpp>
#include <graphene/utilities/key_conversion.hpp>

#include <boost/asio.hpp>

namespace graphene { namespace peerplays_sidechain {

hive_node_rpc_client::hive_node_rpc_client(const std::string &url, const std::string &user_name, const std::string &password, bool debug_rpc_calls) :
      rpc_client(url, user_name, password, debug_rpc_calls) {
}

std::string hive_node_rpc_client::account_history_api_get_transaction(std::string transaction_id) {
   std::string params = "{ \"id\": \"" + transaction_id + "\" }";
   return send_post_request("account_history_api.get_transaction", params, debug_rpc_calls);
}

std::string hive_node_rpc_client::block_api_get_block(uint32_t block_number) {
   std::string params = "{ \"block_num\": " + std::to_string(block_number) + " }";
   return send_post_request("block_api.get_block", params, debug_rpc_calls);
}

std::string hive_node_rpc_client::condenser_api_get_accounts(std::vector<std::string> accounts) {
   std::string params = "";
   for (auto account : accounts) {
      if (!params.empty()) {
         params = params + ",";
      }
      params = "\"" + account + "\"";
   }
   params = "[[" + params + "]]";
   return send_post_request("condenser_api.get_accounts", params, debug_rpc_calls);
}

std::string hive_node_rpc_client::condenser_api_get_config() {
   std::string params = "[]";
   return send_post_request("condenser_api.get_config", params, debug_rpc_calls);
}

std::string hive_node_rpc_client::database_api_get_dynamic_global_properties() {
   return send_post_request("database_api.get_dynamic_global_properties", "", debug_rpc_calls);
}

std::string hive_node_rpc_client::database_api_get_version() {
   return send_post_request("database_api.get_version", "", debug_rpc_calls);
}

std::string hive_node_rpc_client::network_broadcast_api_broadcast_transaction(std::string htrx) {
   std::string params = "{ \"trx\": " + htrx + ", \"max_block_age\": -1 }";
   return send_post_request("network_broadcast_api.broadcast_transaction", params, debug_rpc_calls);
}

std::string hive_node_rpc_client::get_account(std::string account) {
   std::vector<std::string> accounts;
   accounts.push_back(account);
   std::string reply_str = condenser_api_get_accounts(accounts);
   return retrieve_array_value_from_reply(reply_str, "", 0);
}

std::string hive_node_rpc_client::get_account_memo_key(std::string account) {
   std::string reply_str = get_account(account);
   reply_str = "{\"result\":" + reply_str + "}";
   return retrieve_value_from_reply(reply_str, "memo_key");
}

std::string hive_node_rpc_client::get_chain_id() {
   std::string reply_str = database_api_get_version();
   return retrieve_value_from_reply(reply_str, "chain_id");
}

std::string hive_node_rpc_client::get_head_block_id() {
   std::string reply_str = database_api_get_dynamic_global_properties();
   return retrieve_value_from_reply(reply_str, "head_block_id");
}

std::string hive_node_rpc_client::get_head_block_time() {
   std::string reply_str = database_api_get_dynamic_global_properties();
   return retrieve_value_from_reply(reply_str, "time");
}

std::string hive_node_rpc_client::get_is_test_net() {
   std::string reply_str = condenser_api_get_config();
   return retrieve_value_from_reply(reply_str, "IS_TEST_NET");
}

std::string hive_node_rpc_client::get_last_irreversible_block_num() {
   std::string reply_str = database_api_get_dynamic_global_properties();
   return retrieve_value_from_reply(reply_str, "last_irreversible_block_num");
}

sidechain_net_handler_hive::sidechain_net_handler_hive(peerplays_sidechain_plugin &_plugin, const boost::program_options::variables_map &options) :
      sidechain_net_handler(_plugin, options) {
   sidechain = sidechain_type::hive;

   if (options.count("debug-rpc-calls")) {
      debug_rpc_calls = options.at("debug-rpc-calls").as<bool>();
   }

   node_rpc_url = options.at("hive-node-rpc-url").as<std::string>();
   if (options.count("hive-node-rpc-user")) {
      node_rpc_user = options.at("hive-node-rpc-user").as<std::string>();
   } else {
      node_rpc_user = "";
   }
   if (options.count("hive-node-rpc-password")) {
      node_rpc_password = options.at("hive-node-rpc-password").as<std::string>();
   } else {
      node_rpc_password = "";
   }

   if (options.count("hive-private-key")) {
      const std::vector<std::string> pub_priv_keys = options["hive-private-key"].as<std::vector<std::string>>();
      for (const std::string &itr_key_pair : pub_priv_keys) {
         auto key_pair = graphene::app::dejsonify<std::pair<std::string, std::string>>(itr_key_pair, 5);
         ilog("Hive Public Key: ${public}", ("public", key_pair.first));
         if (!key_pair.first.length() || !key_pair.second.length()) {
            FC_THROW("Invalid public private key pair.");
         }
         private_keys[key_pair.first] = key_pair.second;
      }
   }

   node_rpc_client = new hive_node_rpc_client(node_rpc_url, node_rpc_user, node_rpc_password, debug_rpc_calls);

   std::string chain_id_str = node_rpc_client->get_chain_id();
   if (chain_id_str.empty()) {
      elog("No Hive node running at ${url}", ("url", node_rpc_url));
      FC_ASSERT(false);
   }
   chain_id = chain_id_type(chain_id_str);

   std::string is_test_net = node_rpc_client->get_is_test_net();
   network_type = is_test_net.compare("true") == 0 ? hive::network::testnet : hive::network::mainnet;
   if (network_type == hive::network::mainnet) {
      ilog("Running on Hive mainnet, chain id ${chain_id_str}", ("chain_id_str", chain_id_str));
      hive::asset::hbd_symbol_ser = HBD_SYMBOL_SER;
      hive::asset::hive_symbol_ser = HIVE_SYMBOL_SER;
      hive::public_key_type::prefix = KEY_PREFIX_STM;
   } else {
      ilog("Running on Hive testnet, chain id ${chain_id_str}", ("chain_id_str", chain_id_str));
      hive::asset::hbd_symbol_ser = TBD_SYMBOL_SER;
      hive::asset::hive_symbol_ser = TESTS_SYMBOL_SER;
      hive::public_key_type::prefix = KEY_PREFIX_TST;
   }

   last_block_received = 0;
   schedule_hive_listener();
   event_received.connect([this](const std::string &event_data) {
      std::thread(&sidechain_net_handler_hive::handle_event, this, event_data).detach();
   });
}

sidechain_net_handler_hive::~sidechain_net_handler_hive() {
}

bool sidechain_net_handler_hive::process_proposal(const proposal_object &po) {
   //ilog("Proposal to process: ${po}, SON id ${son_id}", ("po", po.id)("son_id", plugin.get_current_son_id()));

   bool should_approve = false;

   const chain::global_property_object &gpo = database.get_global_properties();

   int32_t op_idx_0 = -1;
   chain::operation op_obj_idx_0;

   if (po.proposed_transaction.operations.size() >= 1) {
      op_idx_0 = po.proposed_transaction.operations[0].which();
      op_obj_idx_0 = po.proposed_transaction.operations[0];
   }

   int32_t op_idx_1 = -1;
   chain::operation op_obj_idx_1;
   (void)op_idx_1;

   if (po.proposed_transaction.operations.size() >= 2) {
      op_idx_1 = po.proposed_transaction.operations[1].which();
      op_obj_idx_1 = po.proposed_transaction.operations[1];
   }

   switch (op_idx_0) {

   case chain::operation::tag<chain::son_wallet_update_operation>::value: {
      bool address_ok = false;
      bool transaction_ok = false;
      son_wallet_id_type swo_id = op_obj_idx_0.get<son_wallet_update_operation>().son_wallet_id;
      const auto &idx = database.get_index_type<son_wallet_index>().indices().get<by_id>();
      const auto swo = idx.find(swo_id);
      if (swo != idx.end()) {

         auto active_sons = gpo.active_sons;
         vector<son_info> wallet_sons = swo->sons;

         bool son_sets_equal = (active_sons.size() == wallet_sons.size());

         if (son_sets_equal) {
            for (size_t i = 0; i < active_sons.size(); i++) {
               son_sets_equal = son_sets_equal && active_sons.at(i) == wallet_sons.at(i);
            }
         }

         if (son_sets_equal) {
            address_ok = (op_obj_idx_0.get<son_wallet_update_operation>().address == "son-account");
         }

         if (po.proposed_transaction.operations.size() >= 2) {
            object_id_type object_id = op_obj_idx_1.get<sidechain_transaction_create_operation>().object_id;
            std::string op_tx_str = op_obj_idx_1.get<sidechain_transaction_create_operation>().transaction;

            const auto &st_idx = database.get_index_type<sidechain_transaction_index>().indices().get<by_object_id>();
            const auto st = st_idx.find(object_id);
            if (st == st_idx.end()) {

               std::string tx_str = "";

               if (object_id.is<son_wallet_id_type>()) {
                  const auto &idx = database.get_index_type<son_wallet_index>().indices().get<by_id>();
                  const auto swo = idx.find(object_id);
                  if (swo != idx.end()) {

                     std::stringstream ss_trx(boost::algorithm::unhex(op_tx_str));
                     hive::signed_transaction op_trx;
                     fc::raw::unpack(ss_trx, op_trx, 1000);

                     fc::flat_map<std::string, uint16_t> account_auths;
                     uint32_t total_weight = 0;
                     for (const auto &wallet_son : wallet_sons) {
                        total_weight = total_weight + wallet_son.weight;
                        account_auths[wallet_son.sidechain_public_keys.at(sidechain)] = wallet_son.weight;
                     }

                     std::string memo_key = node_rpc_client->get_account_memo_key("son-account");

                     hive::authority active;
                     active.weight_threshold = total_weight * 2 / 3 + 1;
                     active.account_auths = account_auths;

                     hive::account_update_operation auo;
                     auo.account = "son-account";
                     auo.active = active;
                     auo.memo_key = op_trx.operations[0].get<hive::account_update_operation>().memo_key;

                     hive::signed_transaction htrx;
                     htrx.ref_block_num = op_trx.ref_block_num;
                     htrx.ref_block_prefix = op_trx.ref_block_prefix;
                     htrx.set_expiration(op_trx.expiration);

                     htrx.operations.push_back(auo);

                     std::stringstream ss;
                     fc::raw::pack(ss, htrx, 1000);
                     tx_str = boost::algorithm::hex(ss.str());
                  }
               }

               transaction_ok = (op_tx_str == tx_str);
            }
         } else {
            transaction_ok = true;
         }
      }

      should_approve = address_ok &&
                       transaction_ok;
      break;
   }

   case chain::operation::tag<chain::son_wallet_deposit_process_operation>::value: {
      bool process_ok = false;
      son_wallet_deposit_id_type swdo_id = op_obj_idx_0.get<son_wallet_deposit_process_operation>().son_wallet_deposit_id;
      const auto &idx = database.get_index_type<son_wallet_deposit_index>().indices().get<by_id>();
      const auto swdo = idx.find(swdo_id);
      if (swdo != idx.end()) {

         std::string swdo_txid = swdo->sidechain_transaction_id;
         std::string swdo_sidechain_from = swdo->sidechain_from;
         std::string swdo_sidechain_currency = swdo->sidechain_currency;
         uint64_t swdo_sidechain_amount = swdo->sidechain_amount.value;
         uint64_t swdo_op_idx = std::stoll(swdo->sidechain_uid.substr(swdo->sidechain_uid.find_last_of("-")));

         std::string tx_str = node_rpc_client->account_history_api_get_transaction(swdo_txid);
         if (tx_str != "") {

            std::stringstream ss_tx(tx_str);
            boost::property_tree::ptree tx;
            boost::property_tree::read_json(ss_tx, tx);

            uint64_t op_idx = -1;
            for (const auto &ops : tx.get_child("result.operations")) {
               const auto &op = ops.second;
               op_idx = op_idx + 1;
               if (op_idx == swdo_op_idx) {
                  std::string operation_type = op.get<std::string>("type");

                  if (operation_type == "transfer_operation") {
                     const auto &op_value = op.get_child("value");

                     std::string sidechain_from = op_value.get<std::string>("from");

                     const auto &amount_child = op_value.get_child("amount");

                     uint64_t amount = amount_child.get<uint64_t>("amount");
                     std::string nai = amount_child.get<std::string>("nai");
                     std::string sidechain_currency = "";
                     if ((nai == "@@000000013" /*?? HBD*/) || (nai == "@@000000013" /*TBD*/)) {
                        sidechain_currency = "HBD";
                     }
                     if ((nai == "@@000000021") /*?? HIVE*/ || (nai == "@@000000021" /*TESTS*/)) {
                        sidechain_currency = "HIVE";
                     }

                     std::string memo = op_value.get<std::string>("memo");
                     boost::trim(memo);
                     if (!memo.empty()) {
                        sidechain_from = memo;
                     }

                     process_ok = (swdo_sidechain_from == sidechain_from) &&
                                  (swdo_sidechain_currency == sidechain_currency) &&
                                  (swdo_sidechain_amount == amount);
                  }
               }
            }
         }
      }
      should_approve = process_ok;
      break;
   }

   case chain::operation::tag<chain::son_wallet_withdraw_process_operation>::value: {
      bool process_ok = false;
      bool transaction_ok = false;
      son_wallet_withdraw_id_type swwo_id = op_obj_idx_0.get<son_wallet_withdraw_process_operation>().son_wallet_withdraw_id;
      const auto &idx = database.get_index_type<son_wallet_withdraw_index>().indices().get<by_id>();
      const auto swwo = idx.find(swwo_id);
      if (swwo != idx.end()) {

         uint32_t swwo_block_num = swwo->block_num;
         std::string swwo_peerplays_transaction_id = swwo->peerplays_transaction_id;
         uint32_t swwo_op_idx = std::stoll(swwo->peerplays_uid.substr(swwo->peerplays_uid.find_last_of("-") + 1));

         const auto &block = database.fetch_block_by_number(swwo_block_num);

         for (const auto &tx : block->transactions) {
            if (tx.id().str() == swwo_peerplays_transaction_id) {
               operation op = tx.operations[swwo_op_idx];
               transfer_operation t_op = op.get<transfer_operation>();

               price asset_price = database.get<asset_object>(t_op.amount.asset_id).options.core_exchange_rate;
               asset peerplays_asset = asset(t_op.amount.amount * asset_price.base.amount / asset_price.quote.amount);

               process_ok = (t_op.to == gpo.parameters.son_account()) &&
                            (swwo->peerplays_from == t_op.from) &&
                            (swwo->peerplays_asset == peerplays_asset);
               break;
            }
         }

         object_id_type object_id = op_obj_idx_1.get<sidechain_transaction_create_operation>().object_id;
         std::string op_tx_str = op_obj_idx_1.get<sidechain_transaction_create_operation>().transaction;

         const auto &st_idx = database.get_index_type<sidechain_transaction_index>().indices().get<by_object_id>();
         const auto st = st_idx.find(object_id);
         if (st == st_idx.end()) {

            std::string tx_str = "";

            if (object_id.is<son_wallet_withdraw_id_type>()) {
               const auto &idx = database.get_index_type<son_wallet_withdraw_index>().indices().get<by_id>();
               const auto swwo = idx.find(object_id);
               if (swwo != idx.end()) {

                  std::stringstream ss_trx(boost::algorithm::unhex(op_tx_str));
                  hive::signed_transaction op_trx;
                  fc::raw::unpack(ss_trx, op_trx, 1000);

                  uint64_t symbol = 0;
                  if (swwo->withdraw_currency == "HBD") {
                     symbol = hive::asset::hbd_symbol_ser;
                  }
                  if (swwo->withdraw_currency == "HIVE") {
                     symbol = hive::asset::hive_symbol_ser;
                  }

                  hive::transfer_operation t_op;
                  t_op.from = "son-account";
                  t_op.to = swwo->withdraw_address;
                  t_op.amount.amount = swwo->withdraw_amount;
                  t_op.amount.symbol = symbol;
                  t_op.memo = "";

                  hive::signed_transaction htrx;
                  htrx.ref_block_num = op_trx.ref_block_num;
                  htrx.ref_block_prefix = op_trx.ref_block_prefix;
                  htrx.set_expiration(op_trx.expiration);

                  htrx.operations.push_back(t_op);

                  std::stringstream ss;
                  fc::raw::pack(ss, htrx, 1000);
                  tx_str = boost::algorithm::hex(ss.str());
               }
            }

            transaction_ok = (op_tx_str == tx_str);
         }
      }

      should_approve = process_ok &&
                       transaction_ok;
      break;
   }

   case chain::operation::tag<chain::sidechain_transaction_sign_operation>::value: {
      should_approve = true;
      son_id_type signer = op_obj_idx_0.get<sidechain_transaction_sign_operation>().signer;
      std::string signature = op_obj_idx_0.get<sidechain_transaction_sign_operation>().signature;
      sidechain_transaction_id_type sidechain_transaction_id = op_obj_idx_0.get<sidechain_transaction_sign_operation>().sidechain_transaction_id;
      const auto &st_idx = database.get_index_type<sidechain_transaction_index>().indices().get<by_id>();
      const auto sto = st_idx.find(sidechain_transaction_id);
      if (sto == st_idx.end()) {
         should_approve = false;
         break;
      }

      const auto &s_idx = database.get_index_type<son_index>().indices().get<by_id>();
      const auto son = s_idx.find(signer);
      if (son == s_idx.end()) {
         should_approve = false;
         break;
      }

      break;
   }

   case chain::operation::tag<chain::sidechain_transaction_settle_operation>::value: {
      should_approve = true;
      break;
   }

   default:
      should_approve = false;
      elog("==================================================");
      elog("Proposal not considered for approval ${po}", ("po", po));
      elog("==================================================");
   }

   return should_approve;
}

void sidechain_net_handler_hive::process_primary_wallet() {
   const auto &swi = database.get_index_type<son_wallet_index>().indices().get<by_id>();
   const auto &active_sw = swi.rbegin();
   if (active_sw != swi.rend()) {

      if ((active_sw->addresses.find(sidechain) == active_sw->addresses.end()) ||
          (active_sw->addresses.at(sidechain).empty())) {

         if (proposal_exists(chain::operation::tag<chain::son_wallet_update_operation>::value, active_sw->id)) {
            return;
         }

         const chain::global_property_object &gpo = database.get_global_properties();

         auto active_sons = gpo.active_sons;
         fc::flat_map<std::string, uint16_t> account_auths;
         uint32_t total_weight = 0;
         for (const auto &active_son : active_sons) {
            total_weight = total_weight + active_son.weight;
            account_auths[active_son.sidechain_public_keys.at(sidechain)] = active_son.weight;
         }

         std::string memo_key = node_rpc_client->get_account_memo_key("son-account");

         if (memo_key.empty()) {
            return;
         }

         hive::authority active;
         active.weight_threshold = total_weight * 2 / 3 + 1;
         active.account_auths = account_auths;

         hive::account_update_operation auo;
         auo.account = "son-account";
         auo.active = active;
         auo.memo_key = hive::public_key_type(memo_key);

         std::string block_id_str = node_rpc_client->get_head_block_id();
         hive::block_id_type head_block_id(block_id_str);

         std::string head_block_time_str = node_rpc_client->get_head_block_time();
         time_point head_block_time = fc::time_point_sec::from_iso_string(head_block_time_str);

         hive::signed_transaction htrx;
         htrx.set_reference_block(head_block_id);
         htrx.set_expiration(head_block_time + fc::seconds(180));

         htrx.operations.push_back(auo);

         std::stringstream ss;
         fc::raw::pack(ss, htrx, 1000);
         std::string tx_str = boost::algorithm::hex(ss.str());
         if (tx_str.empty()) {
            return;
         }

         proposal_create_operation proposal_op;
         proposal_op.fee_paying_account = plugin.get_current_son_object().son_account;
         uint32_t lifetime = (gpo.parameters.block_interval * gpo.active_witnesses.size()) * 3;
         proposal_op.expiration_time = time_point_sec(database.head_block_time().sec_since_epoch() + lifetime);

         son_wallet_update_operation swu_op;
         swu_op.payer = gpo.parameters.son_account();
         swu_op.son_wallet_id = active_sw->id;
         swu_op.sidechain = sidechain;
         swu_op.address = "son-account";

         proposal_op.proposed_ops.emplace_back(swu_op);

         sidechain_transaction_create_operation stc_op;
         stc_op.payer = gpo.parameters.son_account();
         stc_op.object_id = active_sw->id;
         stc_op.sidechain = sidechain;
         stc_op.transaction = tx_str;
         stc_op.signers = gpo.active_sons;

         proposal_op.proposed_ops.emplace_back(stc_op);

         signed_transaction trx = database.create_signed_transaction(plugin.get_private_key(plugin.get_current_son_id()), proposal_op);
         try {
            trx.validate();
            database.push_transaction(trx, database::validation_steps::skip_block_size_check);
            if (plugin.app().p2p_node())
               plugin.app().p2p_node()->broadcast(net::trx_message(trx));
         } catch (fc::exception &e) {
            elog("Sending proposal for son wallet update operation failed with exception ${e}", ("e", e.what()));
            return;
         }
      }
   }
}

void sidechain_net_handler_hive::process_sidechain_addresses() {
   const auto &sidechain_addresses_idx = database.get_index_type<sidechain_address_index>();
   const auto &sidechain_addresses_by_sidechain_idx = sidechain_addresses_idx.indices().get<by_sidechain>();
   const auto &sidechain_addresses_by_sidechain_range = sidechain_addresses_by_sidechain_idx.equal_range(sidechain);
   std::for_each(sidechain_addresses_by_sidechain_range.first, sidechain_addresses_by_sidechain_range.second,
                 [&](const sidechain_address_object &sao) {
                    bool retval = true;
                    if (sao.expires == time_point_sec::maximum()) {
                       if (sao.deposit_address == "") {
                          sidechain_address_update_operation op;
                          op.payer = plugin.get_current_son_object().son_account;
                          op.sidechain_address_id = sao.id;
                          op.sidechain_address_account = sao.sidechain_address_account;
                          op.sidechain = sao.sidechain;
                          op.deposit_public_key = sao.deposit_public_key;
                          op.deposit_address = sao.withdraw_address;
                          op.deposit_address_data = sao.withdraw_address;
                          op.withdraw_public_key = sao.withdraw_public_key;
                          op.withdraw_address = sao.withdraw_address;

                          signed_transaction trx = database.create_signed_transaction(plugin.get_private_key(plugin.get_current_son_id()), op);
                          try {
                             trx.validate();
                             database.push_transaction(trx, database::validation_steps::skip_block_size_check);
                             if (plugin.app().p2p_node())
                                plugin.app().p2p_node()->broadcast(net::trx_message(trx));
                             retval = true;
                          } catch (fc::exception &e) {
                             elog("Sending transaction for sidechain address update operation failed with exception ${e}", ("e", e.what()));
                             retval = false;
                          }
                       }
                    }
                    return retval;
                 });
}

bool sidechain_net_handler_hive::process_deposit(const son_wallet_deposit_object &swdo) {
   const chain::global_property_object &gpo = database.get_global_properties();

   price asset_price;
   asset asset_to_issue;
   if (swdo.sidechain_currency == "HBD") {
      asset_price = database.get<asset_object>(database.get_global_properties().parameters.hbd_asset()).options.core_exchange_rate;
      asset_to_issue = asset(swdo.peerplays_asset.amount * asset_price.quote.amount / asset_price.base.amount, database.get_global_properties().parameters.hbd_asset());
   }
   if (swdo.sidechain_currency == "HIVE") {
      asset_price = database.get<asset_object>(database.get_global_properties().parameters.hive_asset()).options.core_exchange_rate;
      asset_to_issue = asset(swdo.peerplays_asset.amount * asset_price.quote.amount / asset_price.base.amount, database.get_global_properties().parameters.hive_asset());
   }

   proposal_create_operation proposal_op;
   proposal_op.fee_paying_account = plugin.get_current_son_object().son_account;
   uint32_t lifetime = (gpo.parameters.block_interval * gpo.active_witnesses.size()) * 3;
   proposal_op.expiration_time = time_point_sec(database.head_block_time().sec_since_epoch() + lifetime);

   son_wallet_deposit_process_operation swdp_op;
   swdp_op.payer = gpo.parameters.son_account();
   swdp_op.son_wallet_deposit_id = swdo.id;
   proposal_op.proposed_ops.emplace_back(swdp_op);

   asset_issue_operation ai_op;
   ai_op.fee = database.current_fee_schedule().calculate_fee(ai_op);
   ai_op.issuer = gpo.parameters.son_account();
   ai_op.asset_to_issue = asset_to_issue;
   ai_op.issue_to_account = swdo.peerplays_from;
   proposal_op.proposed_ops.emplace_back(ai_op);

   signed_transaction trx = database.create_signed_transaction(plugin.get_private_key(plugin.get_current_son_id()), proposal_op);
   try {
      trx.validate();
      database.push_transaction(trx, database::validation_steps::skip_block_size_check);
      if (plugin.app().p2p_node())
         plugin.app().p2p_node()->broadcast(net::trx_message(trx));
      return true;
   } catch (fc::exception &e) {
      elog("Sending proposal for deposit sidechain transaction create operation failed with exception ${e}", ("e", e.what()));
      return false;
   }

   return false;
}

bool sidechain_net_handler_hive::process_withdrawal(const son_wallet_withdraw_object &swwo) {
   const chain::global_property_object &gpo = database.get_global_properties();

   //=====

   uint64_t symbol = 0;
   if (swwo.withdraw_currency == "HBD") {
      symbol = hive::asset::hbd_symbol_ser;
   }
   if (swwo.withdraw_currency == "HIVE") {
      symbol = hive::asset::hive_symbol_ser;
   }

   hive::transfer_operation t_op;
   t_op.from = "son-account";
   t_op.to = swwo.withdraw_address;
   t_op.amount.amount = swwo.withdraw_amount;
   t_op.amount.symbol = symbol;
   t_op.memo = "";

   std::string block_id_str = node_rpc_client->get_head_block_id();
   hive::block_id_type head_block_id(block_id_str);

   std::string head_block_time_str = node_rpc_client->get_head_block_time();
   time_point head_block_time = fc::time_point_sec::from_iso_string(head_block_time_str);

   hive::signed_transaction htrx;
   htrx.set_reference_block(head_block_id);
   htrx.set_expiration(head_block_time + fc::seconds(180));

   htrx.operations.push_back(t_op);

   std::stringstream ss;
   fc::raw::pack(ss, htrx, 1000);
   std::string tx_str = boost::algorithm::hex(ss.str());
   if (tx_str.empty()) {
      return false;
   }

   //=====

   proposal_create_operation proposal_op;
   proposal_op.fee_paying_account = plugin.get_current_son_object().son_account;
   uint32_t lifetime = (gpo.parameters.block_interval * gpo.active_witnesses.size()) * 3;
   proposal_op.expiration_time = time_point_sec(database.head_block_time().sec_since_epoch() + lifetime);

   son_wallet_withdraw_process_operation swwp_op;
   swwp_op.payer = gpo.parameters.son_account();
   swwp_op.son_wallet_withdraw_id = swwo.id;
   proposal_op.proposed_ops.emplace_back(swwp_op);

   sidechain_transaction_create_operation stc_op;
   stc_op.payer = gpo.parameters.son_account();
   stc_op.object_id = swwo.id;
   stc_op.sidechain = sidechain;
   stc_op.transaction = tx_str;
   stc_op.signers = gpo.active_sons;
   proposal_op.proposed_ops.emplace_back(stc_op);

   signed_transaction trx = database.create_signed_transaction(plugin.get_private_key(plugin.get_current_son_id()), proposal_op);
   try {
      trx.validate();
      database.push_transaction(trx, database::validation_steps::skip_block_size_check);
      if (plugin.app().p2p_node())
         plugin.app().p2p_node()->broadcast(net::trx_message(trx));
      return true;
   } catch (fc::exception &e) {
      elog("Sending proposal for withdraw sidechain transaction create operation failed with exception ${e}", ("e", e.what()));
      return false;
   }

   return false;
}

std::string sidechain_net_handler_hive::process_sidechain_transaction(const sidechain_transaction_object &sto) {
   std::stringstream ss_trx(boost::algorithm::unhex(sto.transaction));
   hive::signed_transaction htrx;
   fc::raw::unpack(ss_trx, htrx, 1000);

   std::string chain_id_str = node_rpc_client->get_chain_id();
   const hive::chain_id_type chain_id(chain_id_str);

   fc::optional<fc::ecc::private_key> privkey = graphene::utilities::wif_to_key(get_private_key(plugin.get_current_son_object().sidechain_public_keys.at(sidechain)));
   signature_type st = htrx.sign(*privkey, chain_id);

   std::stringstream ss_st;
   fc::raw::pack(ss_st, st, 1000);
   std::string st_str = boost::algorithm::hex(ss_st.str());

   return st_str;
}

std::string sidechain_net_handler_hive::send_sidechain_transaction(const sidechain_transaction_object &sto) {
   std::stringstream ss_trx(boost::algorithm::unhex(sto.transaction));
   hive::signed_transaction htrx;
   fc::raw::unpack(ss_trx, htrx, 1000);

   for (auto signature : sto.signatures) {
      if (!signature.second.empty()) {
         std::stringstream ss_st(boost::algorithm::unhex(signature.second));
         signature_type st;
         fc::raw::unpack(ss_st, st, 1000);
         htrx.signatures.push_back(st);
      }
   }

   std::string params = fc::json::to_string(htrx);
   node_rpc_client->network_broadcast_api_broadcast_transaction(params);

   return htrx.id().str();
}

bool sidechain_net_handler_hive::settle_sidechain_transaction(const sidechain_transaction_object &sto, asset &settle_amount) {
   if (sto.object_id.is<son_wallet_id_type>()) {
      settle_amount.amount = 0;
      return true;
   }

   if (sto.sidechain_transaction.empty()) {
      return false;
   }

   std::string tx_str = node_rpc_client->account_history_api_get_transaction(sto.sidechain_transaction);
   if (tx_str != "") {

      std::stringstream ss_tx(tx_str);
      boost::property_tree::ptree tx_json;
      boost::property_tree::read_json(ss_tx, tx_json);

      //const chain::global_property_object &gpo = database.get_global_properties();

      std::string tx_txid = tx_json.get<std::string>("result.transaction_id");
      uint32_t tx_block_num = tx_json.get<uint32_t>("result.block_num");
      uint32_t last_irreversible_block = std::stoul(node_rpc_client->get_last_irreversible_block_num());

      //std::string tx_address = addr.get_address();
      //int64_t tx_amount = -1;

      if (tx_block_num <= last_irreversible_block) {
         if (sto.object_id.is<son_wallet_withdraw_id_type>()) {
            auto swwo = database.get<son_wallet_withdraw_object>(sto.object_id);
            if (swwo.withdraw_currency == "HBD") {
               settle_amount = asset(swwo.withdraw_amount, database.get_global_properties().parameters.hbd_asset());
            }
            if (swwo.withdraw_currency == "HIVE") {
               settle_amount = asset(swwo.withdraw_amount, database.get_global_properties().parameters.hive_asset());
            }
            return true;
         }
      }
   }
   return false;
}

void sidechain_net_handler_hive::schedule_hive_listener() {
   fc::time_point now = fc::time_point::now();
   int64_t time_to_next = 1000;

   fc::time_point next_wakeup(now + fc::milliseconds(time_to_next));

   _listener_task = fc::schedule([this] {
      hive_listener_loop();
   },
                                 next_wakeup, "SON Hive listener task");
}

void sidechain_net_handler_hive::hive_listener_loop() {
   schedule_hive_listener();

   std::string reply = node_rpc_client->database_api_get_dynamic_global_properties();
   if (!reply.empty()) {
      std::stringstream ss(reply);
      boost::property_tree::ptree json;
      boost::property_tree::read_json(ss, json);
      if (json.count("result")) {
         uint64_t head_block_number = json.get<uint64_t>("result.head_block_number");
         if (head_block_number != last_block_received) {
            std::string event_data = std::to_string(head_block_number);
            handle_event(event_data);
            last_block_received = head_block_number;
         }
      }
   }

   //std::string reply = node_rpc_client->get_last_irreversible_block_num();
   //if (!reply.empty()) {
   //   uint64_t last_irreversible_block = std::stoul(reply);
   //   if (last_irreversible_block != last_block_received) {
   //      std::string event_data = std::to_string(last_irreversible_block);
   //      handle_event(event_data);
   //      last_block_received = last_irreversible_block;
   //   }
   //}
}

void sidechain_net_handler_hive::handle_event(const std::string &event_data) {
   std::string block = node_rpc_client->block_api_get_block(std::atoll(event_data.c_str()));
   if (block != "") {
      std::stringstream ss(block);
      boost::property_tree::ptree block_json;
      boost::property_tree::read_json(ss, block_json);

      size_t tx_idx = -1;
      for (const auto &tx_ids_child : block_json.get_child("result.block.transactions")) {
         boost::property_tree::ptree tx = tx_ids_child.second;
         tx_idx = tx_idx + 1;

         size_t op_idx = -1;
         for (const auto &ops : tx.get_child("operations")) {
            const auto &op = ops.second;
            op_idx = op_idx + 1;

            std::string operation_type = op.get<std::string>("type");

            if (operation_type == "transfer_operation") {
               const auto &op_value = op.get_child("value");

               std::string from = op_value.get<std::string>("from");
               std::string to = op_value.get<std::string>("to");

               if (to == "son-account") {

                  const auto &amount_child = op_value.get_child("amount");

                  uint64_t amount = amount_child.get<uint64_t>("amount");
                  //uint64_t precision = amount_child.get<uint64_t>("precision");
                  std::string nai = amount_child.get<std::string>("nai");
                  std::string sidechain_currency = "";
                  price sidechain_currency_price = {};
                  if ((nai == "@@000000013" /*?? HBD*/) || (nai == "@@000000013" /*TBD*/)) {
                     sidechain_currency = "HBD";
                     sidechain_currency_price = database.get<asset_object>(database.get_global_properties().parameters.hbd_asset()).options.core_exchange_rate;
                  }
                  if ((nai == "@@000000021") /*?? HIVE*/ || (nai == "@@000000021" /*TESTS*/)) {
                     sidechain_currency = "HIVE";
                     sidechain_currency_price = database.get<asset_object>(database.get_global_properties().parameters.hive_asset()).options.core_exchange_rate;
                  }

                  std::string memo = op_value.get<std::string>("memo");
                  boost::trim(memo);
                  if (!memo.empty()) {
                     from = memo;
                  }

                  const auto &sidechain_addresses_idx = database.get_index_type<sidechain_address_index>().indices().get<by_sidechain_and_deposit_address_and_expires>();
                  const auto &addr_itr = sidechain_addresses_idx.find(std::make_tuple(sidechain, from, time_point_sec::maximum()));
                  account_id_type accn = account_id_type();
                  if (addr_itr == sidechain_addresses_idx.end()) {
                     const auto &account_idx = database.get_index_type<account_index>().indices().get<by_name>();
                     const auto &account_itr = account_idx.find(from);
                     if (account_itr == account_idx.end()) {
                        continue;
                     } else {
                        accn = account_itr->id;
                     }
                  } else {
                     accn = addr_itr->sidechain_address_account;
                  }

                  std::vector<std::string> transaction_ids;
                  for (const auto &tx_ids_child : block_json.get_child("result.block.transaction_ids")) {
                     const auto &transaction_id = tx_ids_child.second.get_value<std::string>();
                     transaction_ids.push_back(transaction_id);
                  }
                  std::string transaction_id = transaction_ids.at(tx_idx);

                  std::stringstream ss;
                  ss << "hive"
                     << "-" << transaction_id << "-" << op_idx;
                  std::string sidechain_uid = ss.str();

                  sidechain_event_data sed;
                  sed.timestamp = database.head_block_time();
                  sed.block_num = database.head_block_num();
                  sed.sidechain = sidechain;
                  sed.sidechain_uid = sidechain_uid;
                  sed.sidechain_transaction_id = transaction_id;
                  sed.sidechain_from = from;
                  sed.sidechain_to = to;
                  sed.sidechain_currency = sidechain_currency;
                  sed.sidechain_amount = amount;
                  sed.peerplays_from = accn;
                  sed.peerplays_to = database.get_global_properties().parameters.son_account();
                  sed.peerplays_asset = asset(sed.sidechain_amount * sidechain_currency_price.base.amount / sidechain_currency_price.quote.amount);
                  sidechain_event_data_received(sed);
               }
            }
         }
      }
   }
}

}} // namespace graphene::peerplays_sidechain
