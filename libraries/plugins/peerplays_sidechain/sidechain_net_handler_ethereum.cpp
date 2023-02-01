#include <graphene/peerplays_sidechain/sidechain_net_handler_ethereum.hpp>

#include <algorithm>
#include <thread>

#include <boost/algorithm/string.hpp>
#include <boost/property_tree/json_parser.hpp>
#include <boost/property_tree/ptree.hpp>

#include <fc/crypto/base64.hpp>
#include <fc/log/logger.hpp>
#include <fc/network/ip.hpp>

#include <graphene/chain/account_object.hpp>
#include <graphene/chain/protocol/son_wallet.hpp>
#include <graphene/chain/sidechain_transaction_object.hpp>
#include <graphene/chain/son_sidechain_info.hpp>
#include <graphene/chain/son_wallet_object.hpp>
#include <graphene/peerplays_sidechain/ethereum/decoders.hpp>
#include <graphene/peerplays_sidechain/ethereum/encoders.hpp>
#include <graphene/peerplays_sidechain/ethereum/transaction.hpp>
#include <graphene/peerplays_sidechain/ethereum/utils.hpp>

#define SEND_RAW_TRANSACTION 1

namespace graphene { namespace peerplays_sidechain {

ethereum_rpc_client::ethereum_rpc_client(const std::string &url, const std::string &user_name, const std::string &password, bool debug_rpc_calls) :
      rpc_client(url, user_name, password, debug_rpc_calls) {
}

std::string ethereum_rpc_client::eth_blockNumber() {
   const std::string reply_str = send_post_request("eth_blockNumber", "", debug_rpc_calls);
   return retrieve_value_from_reply(reply_str, "");
}

std::string ethereum_rpc_client::eth_get_block_by_number(std::string block_number, bool full_block) {
   const std::string params = "[ \"" + block_number + "\", " + (full_block ? "true" : "false") + "]";
   return send_post_request("eth_getBlockByNumber", params, debug_rpc_calls);
}

std::string ethereum_rpc_client::eth_get_logs(std::string wallet_contract_address) {
   const std::string params = "[{\"address\": \"" + wallet_contract_address + "\"}]";
   const std::string reply_str = send_post_request("eth_getLogs", params, debug_rpc_calls);
   return retrieve_value_from_reply(reply_str, "");
}

std::string ethereum_rpc_client::eth_chainId() {
   return send_post_request("eth_chainId", "", debug_rpc_calls);
}

std::string ethereum_rpc_client::net_version() {
   return send_post_request("net_version", "", debug_rpc_calls);
}

std::string ethereum_rpc_client::eth_get_transaction_count(const std::string &params) {
   return send_post_request("eth_getTransactionCount", params, debug_rpc_calls);
}

std::string ethereum_rpc_client::eth_gas_price() {
   return send_post_request("eth_gasPrice", "", debug_rpc_calls);
}

std::string ethereum_rpc_client::eth_estimateGas(const std::string &params) {
   return send_post_request("eth_estimateGas", params, debug_rpc_calls);
}

std::string ethereum_rpc_client::get_chain_id() {
   const std::string reply_str = eth_chainId();
   const auto chain_id_string = retrieve_value_from_reply(reply_str, "");
   return chain_id_string.empty() ? "" : std::to_string(ethereum::from_hex<long long>(chain_id_string));
}

std::string ethereum_rpc_client::get_network_id() {
   const std::string reply_str = net_version();
   return retrieve_value_from_reply(reply_str, "");
}

std::string ethereum_rpc_client::get_nonce(const std::string &address) {
   const std::string reply_str = eth_get_transaction_count("[\"" + address + "\", \"pending\"]");
   const auto nonce_string = retrieve_value_from_reply(reply_str, "");
   if (!nonce_string.empty()) {
      const auto nonce_val = ethereum::from_hex<boost::multiprecision::uint256_t>(nonce_string);
      return nonce_val == 0 ? ethereum::add_0x("0") : ethereum::add_0x(ethereum::to_hex(nonce_val));
   }
   return "";
}

std::string ethereum_rpc_client::get_gas_price() {
   const std::string reply_str = eth_gas_price();
   return retrieve_value_from_reply(reply_str, "");
}

std::string ethereum_rpc_client::get_gas_limit() {
   const std::string reply_str = eth_get_block_by_number("latest", false);
   if (!reply_str.empty()) {
      std::stringstream ss(reply_str);
      boost::property_tree::ptree json;
      boost::property_tree::read_json(ss, json);
      if (json.count("result")) {
         std::string gas_limit_s = json.get<std::string>("result.gasLimit");
         return gas_limit_s;
      }
   }
   return std::string{};
}

std::string ethereum_rpc_client::get_estimate_gas(const std::string &params) {
   const std::string reply_str = eth_estimateGas(params);
   return retrieve_value_from_reply(reply_str, "");
}

std::string ethereum_rpc_client::eth_send_transaction(const std::string &params) {
   return send_post_request("eth_sendTransaction", "[" + params + "]", debug_rpc_calls);
}

std::string ethereum_rpc_client::eth_send_raw_transaction(const std::string &params) {
   return send_post_request("eth_sendRawTransaction", "[ \"" + params + "\" ]", debug_rpc_calls);
}

std::string ethereum_rpc_client::eth_get_transaction_receipt(const std::string &params) {
   return send_post_request("eth_getTransactionReceipt", "[\"" + params + "\"]", debug_rpc_calls);
}

std::string ethereum_rpc_client::eth_get_transaction_by_hash(const std::string &params) {
   return send_post_request("eth_getTransactionByHash", "[\"" + params + "\"]", debug_rpc_calls);
}

sidechain_net_handler_ethereum::sidechain_net_handler_ethereum(peerplays_sidechain_plugin &_plugin, const boost::program_options::variables_map &options) :
      sidechain_net_handler(_plugin, options) {
   sidechain = sidechain_type::ethereum;

   if (options.count("debug-rpc-calls")) {
      debug_rpc_calls = options.at("debug-rpc-calls").as<bool>();
   }

   rpc_url = options.at("ethereum-node-rpc-url").as<std::string>();
   if (options.count("ethereum-node-rpc-user")) {
      rpc_user = options.at("ethereum-node-rpc-user").as<std::string>();
   } else {
      rpc_user = "";
   }
   if (options.count("ethereum-node-rpc-password")) {
      rpc_password = options.at("ethereum-node-rpc-password").as<std::string>();
   } else {
      rpc_password = "";
   }

   wallet_contract_address = options.at("ethereum-wallet-contract-address").as<std::string>();

   if (options.count("erc-20-address")) {
      const std::vector<std::string> symbol_addresses = options["erc-20-address"].as<std::vector<std::string>>();
      for (const std::string &itr : symbol_addresses) {
         auto itr_pair = graphene::app::dejsonify<std::pair<std::string, std::string>>(itr, 5);
         ilog("ERC-20 symbol: ${symbol}, address: ${address}", ("symbol", itr_pair.first)("address", itr_pair.second));
         if (!itr_pair.first.length() || !itr_pair.second.length()) {
            FC_THROW("Invalid symbol address pair.");
         }

         auto address = itr_pair.second;
         std::transform(address.begin(), address.end(), address.begin(), ::tolower);
         erc20_addresses.insert(bimap_type::value_type{itr_pair.first, address});
      }
   }

   if (options.count("ethereum-private-key")) {
      const std::vector<std::string> pub_priv_keys = options["ethereum-private-key"].as<std::vector<std::string>>();
      for (const std::string &itr_key_pair : pub_priv_keys) {
         auto key_pair = graphene::app::dejsonify<std::pair<std::string, std::string>>(itr_key_pair, 5);
         ilog("Ethereum Public Key: ${public}", ("public", key_pair.first));
         if (!key_pair.first.length() || !key_pair.second.length()) {
            FC_THROW("Invalid public private key pair.");
         }
         private_keys[key_pair.first] = key_pair.second;
      }
   }

   rpc_client = new ethereum_rpc_client(rpc_url, rpc_user, rpc_password, debug_rpc_calls);

   const std::string chain_id_str = rpc_client->get_chain_id();
   if (chain_id_str.empty()) {
      elog("No Ethereum node running at ${url}", ("url", rpc_url));
      FC_ASSERT(false);
   }
   chain_id = std::stoll(chain_id_str);

   const std::string network_id_str = rpc_client->get_network_id();
   if (network_id_str.empty()) {
      elog("No Ethereum node running at ${url}", ("url", rpc_url));
      FC_ASSERT(false);
   }
   network_id = std::stoll(network_id_str);

   ilog("Running on Ethereum network, chain id ${chain_id_str}, network id ${network_id_str}", ("chain_id_str", chain_id_str)("network_id_str", network_id_str));

   const auto block_number = rpc_client->eth_blockNumber();
   last_block_received = !block_number.empty() ? ethereum::from_hex<uint64_t>(block_number) : 0;
   schedule_ethereum_listener();
   event_received.connect([this](const std::string &event_data) {
      std::thread(&sidechain_net_handler_ethereum::handle_event, this, event_data).detach();
   });
}

sidechain_net_handler_ethereum::~sidechain_net_handler_ethereum() {
}

bool sidechain_net_handler_ethereum::process_proposal(const proposal_object &po) {
   ilog("Proposal to process: ${po}, SON id ${son_id}", ("po", po.id)("son_id", plugin.get_current_son_id(sidechain)));

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
      const son_wallet_id_type swo_id = op_obj_idx_0.get<son_wallet_update_operation>().son_wallet_id;
      const auto id = (swo_id.instance.value - std::distance(active_sidechain_types.begin(), active_sidechain_types.find(sidechain))) / active_sidechain_types.size();
      const son_wallet_id_type op_id{ id };
      const auto &idx = database.get_index_type<son_wallet_index>().indices().get<by_id>();
      const auto swo = idx.find(op_id);
      if (swo != idx.end()) {

         const auto active_sons = gpo.active_sons.at(sidechain);
         const vector<son_sidechain_info> wallet_sons = swo->sons.at(sidechain);

         bool son_sets_equal = (active_sons.size() == wallet_sons.size());

         if (son_sets_equal) {
            for (size_t i = 0; i < active_sons.size(); i++) {
               son_sets_equal = son_sets_equal && active_sons.at(i) == wallet_sons.at(i);
            }
         }

         if (son_sets_equal) {
            address_ok = (op_obj_idx_0.get<son_wallet_update_operation>().address == wallet_contract_address);
         }

         if (po.proposed_transaction.operations.size() >= 2) {
            const object_id_type object_id = op_obj_idx_1.get<sidechain_transaction_create_operation>().object_id;
            const auto id = (object_id.instance() - std::distance(active_sidechain_types.begin(), active_sidechain_types.find(sidechain))) / active_sidechain_types.size();
            const object_id_type obj_id{ object_id.space(), object_id.type(), id };
            const std::string op_tx_str = op_obj_idx_1.get<sidechain_transaction_create_operation>().transaction;

            const auto &st_idx = database.get_index_type<sidechain_transaction_index>().indices().get<by_object_id>();
            const auto st = st_idx.find(obj_id);
            if (st == st_idx.end()) {

               std::string tx_str = "";

               if (obj_id.is<son_wallet_id_type>()) {
                  const auto &idx = database.get_index_type<son_wallet_index>().indices().get<by_id>();
                  const auto swo = idx.find(obj_id);
                  if (swo != idx.end()) {
                     tx_str = create_primary_wallet_transaction(gpo.active_sons.at(sidechain), object_id.operator std::string());
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
      const son_wallet_deposit_id_type swdo_id = op_obj_idx_0.get<son_wallet_deposit_process_operation>().son_wallet_deposit_id;
      const auto &idx = database.get_index_type<son_wallet_deposit_index>().indices().get<by_id>();
      const auto swdo = idx.find(swdo_id);
      if (swdo != idx.end()) {

         const std::string swdo_txid = swdo->sidechain_transaction_id;
         const std::string swdo_sidechain_from = swdo->sidechain_from;
         const std::string swdo_sidechain_currency = swdo->sidechain_currency;
         const uint64_t swdo_sidechain_amount = swdo->sidechain_amount.value;

         const std::string tx_str = rpc_client->eth_get_transaction_by_hash(swdo_txid);
         if (tx_str != "") {

            std::stringstream ss_tx(tx_str);
            boost::property_tree::ptree tx;
            boost::property_tree::read_json(ss_tx, tx);

            if (tx.get<std::string>("result") != "null") {

               const std::string sidechain_from = tx.get<std::string>("result.from");
               const std::string sidechain_to = tx.get<std::string>("result.to");

               std::string cmp_sidechain_to = sidechain_to;
               std::transform(cmp_sidechain_to.begin(), cmp_sidechain_to.end(), cmp_sidechain_to.begin(), ::toupper);
               std::string cmp_wallet_contract_address = wallet_contract_address;
               std::transform(cmp_wallet_contract_address.begin(), cmp_wallet_contract_address.end(), cmp_wallet_contract_address.begin(), ::toupper);

               //! Check whether it is ERC-20 token deposit
               std::string symbol;
               boost::multiprecision::uint256_t amount;
               bool error_in_deposit = false;
               const auto deposit_erc_20 = ethereum::deposit_erc20_decoder::decode(tx.get<std::string>("result.input"));
               if (deposit_erc_20.valid()) {
                  std::string cmp_token = deposit_erc_20->token;
                  std::transform(cmp_token.begin(), cmp_token.end(), cmp_token.begin(), ::tolower);

                  const auto it = erc20_addresses.right.find(cmp_token);
                  if (it == erc20_addresses.right.end()) {
                     wlog("No erc-20 token with address: ${address}", ("address", cmp_token));
                     error_in_deposit = true;
                  }
                  symbol = it->second;
                  amount = deposit_erc_20->amount;
               } else {
                  symbol = "ETH";
                  const std::string value_s = tx.get<std::string>("result.value");
                  amount = boost::multiprecision::uint256_t{value_s};
                  amount = amount / 100000;
                  amount = amount / 100000;
               }

               process_ok = (!error_in_deposit) &&
                            (swdo_sidechain_from == sidechain_from) &&
                            (cmp_sidechain_to == cmp_wallet_contract_address) &&
                            (swdo_sidechain_currency == symbol) &&
                            (swdo_sidechain_amount == fc::safe<uint64_t>{amount}.value);
            }
         }
      }

      should_approve = process_ok;
      break;
   }

   case chain::operation::tag<chain::son_wallet_withdraw_process_operation>::value: {
      bool process_ok = false;
      bool transaction_ok = false;
      const son_wallet_withdraw_id_type swwo_id = op_obj_idx_0.get<son_wallet_withdraw_process_operation>().son_wallet_withdraw_id;
      const auto &idx = database.get_index_type<son_wallet_withdraw_index>().indices().get<by_id>();
      const auto swwo = idx.find(swwo_id);
      if (swwo != idx.end()) {
         const uint32_t swwo_block_num = swwo->block_num;
         const std::string swwo_peerplays_transaction_id = swwo->peerplays_transaction_id;
         const uint32_t swwo_op_idx = std::stoll(swwo->peerplays_uid.substr(swwo->peerplays_uid.find_last_of("-") + 1));

         const auto &block = database.fetch_block_by_number(swwo_block_num);

         for (const auto &tx : block->transactions) {
            if (tx.id().str() == swwo_peerplays_transaction_id) {
               const operation op = tx.operations[swwo_op_idx];
               const transfer_operation t_op = op.get<transfer_operation>();

               const price asset_price = database.get<asset_object>(t_op.amount.asset_id).options.core_exchange_rate;
               const asset peerplays_asset = asset(t_op.amount.amount * asset_price.base.amount / asset_price.quote.amount);

               process_ok = (t_op.to == gpo.parameters.son_account()) &&
                            (swwo->peerplays_from == t_op.from) &&
                            (swwo->peerplays_asset == peerplays_asset);
               break;
            }
         }

         const object_id_type object_id = op_obj_idx_1.get<sidechain_transaction_create_operation>().object_id;
         const std::string op_tx_str = op_obj_idx_1.get<sidechain_transaction_create_operation>().transaction;

         const auto &st_idx = database.get_index_type<sidechain_transaction_index>().indices().get<by_object_id>();
         const auto st = st_idx.find(object_id);
         if (st == st_idx.end()) {

            std::string tx_str = "";

            if (object_id.is<son_wallet_withdraw_id_type>()) {
               const auto &idx = database.get_index_type<son_wallet_withdraw_index>().indices().get<by_id>();
               const auto swwo = idx.find(object_id);
               if (swwo != idx.end()) {
                  tx_str = create_withdrawal_transaction(*swwo);
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
      const son_id_type signer = op_obj_idx_0.get<sidechain_transaction_sign_operation>().signer;
      const std::string signature = op_obj_idx_0.get<sidechain_transaction_sign_operation>().signature;
      const sidechain_transaction_id_type sidechain_transaction_id = op_obj_idx_0.get<sidechain_transaction_sign_operation>().sidechain_transaction_id;
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

void sidechain_net_handler_ethereum::process_primary_wallet() {
   const auto &swi = database.get_index_type<son_wallet_index>().indices().get<by_id>();
   const auto &active_sw = swi.rbegin();
   if (active_sw != swi.rend()) {

      const auto &prev_sw = std::next(active_sw);
      if (prev_sw != swi.rend() && active_sw->sons.at(sidechain) == prev_sw->sons.at(sidechain))
         return;

      if ((active_sw->addresses.find(sidechain) == active_sw->addresses.end()) ||
          (active_sw->addresses.at(sidechain).empty())) {

         const auto id = active_sw->id.instance() * active_sidechain_types.size() + std::distance(active_sidechain_types.begin(), active_sidechain_types.find(sidechain));
         const object_id_type op_id{ active_sw->id.space(), active_sw->id.type(), id };

         if (proposal_exists(chain::operation::tag<chain::son_wallet_update_operation>::value, op_id)) {
            return;
         }

         if (!plugin.can_son_participate(sidechain, chain::operation::tag<chain::son_wallet_update_operation>::value, op_id)) {
            return;
         }

         const chain::global_property_object &gpo = database.get_global_properties();
         proposal_create_operation proposal_op;
         proposal_op.fee_paying_account = plugin.get_current_son_object(sidechain).son_account;
         const uint32_t lifetime = (gpo.parameters.block_interval * gpo.active_witnesses.size()) * 3;
         proposal_op.expiration_time = time_point_sec(database.head_block_time().sec_since_epoch() + lifetime);

         son_wallet_update_operation swu_op;
         swu_op.payer = gpo.parameters.son_account();
         swu_op.son_wallet_id = op_id;
         swu_op.sidechain = sidechain;
         swu_op.address = wallet_contract_address;
         proposal_op.proposed_ops.emplace_back(swu_op);

         const auto signers = [this, &prev_sw, &active_sw, &swi] {
            std::vector<son_sidechain_info> signers;
            //! Check if we don't have any previous set of active SONs use the current one
            if (prev_sw != swi.rend()) {
               if (!prev_sw->sons.at(sidechain).empty())
                  signers = prev_sw->sons.at(sidechain);
               else
                  signers = active_sw->sons.at(sidechain);
            } else {
               signers = active_sw->sons.at(sidechain);
            }

            return signers;
         }();

         std::string tx_str = create_primary_wallet_transaction(gpo.active_sons.at(sidechain), op_id.operator std::string());
         if (!tx_str.empty()) {
            sidechain_transaction_create_operation stc_op;
            stc_op.payer = gpo.parameters.son_account();
            stc_op.object_id = op_id;
            stc_op.sidechain = sidechain;
            stc_op.transaction = tx_str;
            for (const auto &signer : signers) {
               son_info si;
               si.son_id = signer.son_id;
               si.weight = signer.weight;
               si.signing_key = signer.signing_key;
               si.sidechain_public_keys[sidechain] = signer.public_key;
               stc_op.signers.emplace_back(std::move(si));
            }
            proposal_op.proposed_ops.emplace_back(stc_op);
         }

         signed_transaction trx = database.create_signed_transaction(plugin.get_private_key(plugin.get_current_son_id(sidechain)), proposal_op);
         try {
            trx.validate();
            database.push_transaction(trx, database::validation_steps::skip_block_size_check);
            if (plugin.app().p2p_node())
               plugin.app().p2p_node()->broadcast(net::trx_message(trx));
            plugin.log_son_proposal_retry(sidechain, chain::operation::tag<chain::son_wallet_update_operation>::value, op_id);
         } catch (fc::exception &e) {
            elog("Sending proposal for son wallet update operation failed with exception ${e}", ("e", e.what()));
            return;
         }
      }
   }
}

void sidechain_net_handler_ethereum::process_sidechain_addresses() {
}

bool sidechain_net_handler_ethereum::process_deposit(const son_wallet_deposit_object &swdo) {

   if (proposal_exists(chain::operation::tag<chain::son_wallet_deposit_process_operation>::value, swdo.id)) {
      return false;
   }

   const chain::global_property_object &gpo = database.get_global_properties();

   const auto &assets_by_symbol = database.get_index_type<asset_index>().indices().get<by_symbol>();
   const auto asset_itr = assets_by_symbol.find(swdo.sidechain_currency);
   if (asset_itr == assets_by_symbol.end()) {
      wlog("Could not find asset: ${symbol}", ("symbol", swdo.sidechain_currency));
      return false;
   }

   const price asset_price = asset_itr->options.core_exchange_rate;
   const asset asset_to_issue = asset(swdo.peerplays_asset.amount * asset_price.quote.amount / asset_price.base.amount, asset_itr->get_id());

   proposal_create_operation proposal_op;
   proposal_op.fee_paying_account = plugin.get_current_son_object(sidechain).son_account;
   const uint32_t lifetime = (gpo.parameters.block_interval * gpo.active_witnesses.size()) * 3;
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

   signed_transaction trx = database.create_signed_transaction(plugin.get_private_key(plugin.get_current_son_id(sidechain)), proposal_op);
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

bool sidechain_net_handler_ethereum::process_withdrawal(const son_wallet_withdraw_object &swwo) {

   if (proposal_exists(chain::operation::tag<chain::son_wallet_withdraw_process_operation>::value, swwo.id)) {
      return false;
   }

   std::string tx_str = create_withdrawal_transaction(swwo);

   if (!tx_str.empty()) {
      const chain::global_property_object &gpo = database.get_global_properties();

      proposal_create_operation proposal_op;
      proposal_op.fee_paying_account = plugin.get_current_son_object(sidechain).son_account;
      const uint32_t lifetime = (gpo.parameters.block_interval * gpo.active_witnesses.size()) * 3;
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
      for (const auto &signer : gpo.active_sons.at(sidechain)) {
         son_info si;
         si.son_id = signer.son_id;
         si.weight = signer.weight;
         si.signing_key = signer.signing_key;
         si.sidechain_public_keys[sidechain] = signer.public_key;
         stc_op.signers.emplace_back(std::move(si));
      }
      proposal_op.proposed_ops.emplace_back(stc_op);

      signed_transaction trx = database.create_signed_transaction(plugin.get_private_key(plugin.get_current_son_id(sidechain)), proposal_op);
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
   }
   return false;
}

std::string sidechain_net_handler_ethereum::process_sidechain_transaction(const sidechain_transaction_object &sto) {
   return sign_transaction(sto);
}

std::string sidechain_net_handler_ethereum::send_sidechain_transaction(const sidechain_transaction_object &sto) {
   boost::property_tree::ptree pt;
   boost::property_tree::ptree pt_array;

   std::vector<ethereum::encoded_sign_transaction> transactions;
   for (const auto &signature : sto.signatures) {

      //! Check if we have this signed transaction, if not, don't send it
      if (signature.second.empty())
         continue;

      ethereum::encoded_sign_transaction transaction{sto.transaction, ethereum::signature{signature.second}};
      transactions.emplace_back(transaction);
   }

   const auto &current_son = plugin.get_current_son_object(sidechain);
   FC_ASSERT(current_son.sidechain_public_keys.contains(sidechain), "No public keys for current son: ${account_id}", ("account_id", current_son.son_account));
   const auto &public_key = current_son.sidechain_public_keys.at(sidechain);

   const auto function_signature = ethereum::signature_encoder::get_function_signature_from_transaction(sto.transaction);
   if (function_signature.empty()) {
      elog("Function signature is empty for transaction id ${id}, transaction ${transaction}", ("id", sto.id)("transaction", sto.transaction));
      return std::string{}; //! Return empty string, as we have error in sending
   }

   const ethereum::signature_encoder encoder{function_signature};
#ifdef SEND_RAW_TRANSACTION
   ethereum::raw_transaction raw_tr;
   raw_tr.nonce = rpc_client->get_nonce(ethereum::add_0x(public_key));
   raw_tr.gas_price = rpc_client->get_gas_price();
   raw_tr.gas_limit = rpc_client->get_gas_limit();
   raw_tr.to = wallet_contract_address;
   raw_tr.value = "";
   raw_tr.data = encoder.encode(transactions);
   raw_tr.chain_id = ethereum::add_0x(ethereum::to_hex(chain_id));

   const auto sign_tr = raw_tr.sign(get_private_key(public_key));
   const std::string sidechain_transaction = rpc_client->eth_send_raw_transaction(sign_tr.serialize());
#else
   ethereum::transaction raw_tr;
   raw_tr.data = encoder.encode(transactions);
   raw_tr.to = wallet_contract_address;
   raw_tr.from = ethereum::add_0x(public_key);

   const auto sign_tr = raw_tr.sign(get_private_key(public_key));
   const std::string sidechain_transaction = rpc_client->eth_send_transaction(sign_tr.serialize());
#endif

   std::stringstream ss_tx(sidechain_transaction);
   boost::property_tree::ptree tx_json;
   boost::property_tree::read_json(ss_tx, tx_json);
   if (tx_json.count("result") && !tx_json.count("error")) {
      boost::property_tree::ptree node;
      node.put("transaction", sto.transaction);
      node.put("sidechain_transaction", sidechain_transaction);
      node.put("transaction_receipt", tx_json.get<std::string>("result"));
      pt_array.push_back(std::make_pair("", node));
   } else {
      //! Fixme
      //! How should we proceed with error in eth_send_transaction
      elog("Error in eth send transaction for transaction id ${id}, transaction ${transaction}, sidechain_transaction ${sidechain_transaction}", ("id", sto.id)("transaction", sto.transaction)("sidechain_transaction", sidechain_transaction));
      return std::string{}; //! Return empty string, as we have error in sending
   }
   pt.add_child("result_array", pt_array);

   std::stringstream ss;
   boost::property_tree::json_parser::write_json(ss, pt);
   return ss.str();
}

bool sidechain_net_handler_ethereum::settle_sidechain_transaction(const sidechain_transaction_object &sto, asset &settle_amount) {
   std::stringstream ss(sto.sidechain_transaction);
   boost::property_tree::ptree json;
   boost::property_tree::read_json(ss, json);

   if (!json.count("result_array")) {
      return false;
   }

   size_t count = 0;
   for (const auto &entry : json.get_child("result_array")) {
      const std::string receipt = rpc_client->eth_get_transaction_receipt(entry.second.get<std::string>("transaction_receipt"));

      std::stringstream ss_receipt(receipt);
      boost::property_tree::ptree json_receipt;
      boost::property_tree::read_json(ss_receipt, json_receipt);

      if (json_receipt.get<std::string>("result") == "null") {
         wlog("Block is not minted yet for transaction ${id}", ("id", sto.id));
         return false;
      }

      if ("0x1" == json_receipt.get<std::string>("result.status")) {
         count += 1;
         //! Fixme - compare data somehow?
         //if( sto.transaction == entry_receipt.second.get<std::string>("data") ) {
         //}
      }
   }

   //! Check that we have all transactions
   if (count != json.get_child("result_array").size()) {
      wlog("Not all receipts received for transaction ${id}", ("id", sto.id));
      return false;
   } else {
      if (sto.object_id.is<son_wallet_id_type>()) {
         settle_amount = asset(0, database.get_global_properties().parameters.eth_asset());
      }

      if (sto.object_id.is<son_wallet_withdraw_id_type>()) {
         auto swwo = database.get<son_wallet_withdraw_object>(sto.object_id);
         const auto &assets_by_symbol = database.get_index_type<asset_index>().indices().get<by_symbol>();
         const auto asset_itr = assets_by_symbol.find(swwo.withdraw_currency);
         if (asset_itr == assets_by_symbol.end()) {
            wlog("Could not find asset: ${symbol}", ("symbol", swwo.withdraw_currency));
            return false;
         }
         settle_amount = asset(swwo.withdraw_amount, asset_itr->get_id());
      }

      return true;
   }

   return false;
}

optional<asset> sidechain_net_handler_ethereum::estimate_withdrawal_transaction_fee() const {
   const auto &gpo = database.get_global_properties();
   if (gpo.active_sons.at(sidechain).empty()) {
      wlog("No active sons for sidechain: ${sidechain}", ("sidechain", sidechain));
      return optional<asset>{};
   }

   const auto &active_son = gpo.active_sons.at(sidechain).at(0);
   const auto &s_idx = database.get_index_type<son_index>().indices().get<by_id>();
   const auto son = s_idx.find(active_son.son_id);
   if (son == s_idx.end()) {
      wlog("Can't find son for id: ${son_id}", ("son_id", active_son.son_id));
      return optional<asset>{};
   }

   if (!son->sidechain_public_keys.contains(sidechain)) {
      wlog("No public keys for current son: ${account_id}", ("account_id", son->son_account));
      return optional<asset>{};
   }

   const auto &public_key = son->sidechain_public_keys.at(sidechain);
   const auto data = ethereum::withdrawal_encoder::encode(public_key, 1 * 10000000000, son_wallet_withdraw_id_type{0}.operator object_id_type().operator std::string());
   const std::string params = "[{\"from\":\"" + ethereum::add_0x(public_key) + "\", \"to\":\"" + wallet_contract_address + "\", \"data\":\"" + data + "\"}]";

   const auto estimate_gas = ethereum::from_hex<int64_t>(rpc_client->get_estimate_gas(params));
   const auto gas_price = ethereum::from_hex<int64_t>(rpc_client->get_gas_price());
   const auto eth_gas_fee = double(estimate_gas * gas_price) / double{1000000000000000000};

   const auto asset = database.get<asset_object>(database.get_global_properties().parameters.eth_asset());
   return asset.amount_from_string(std::to_string(eth_gas_fee));
}

std::string sidechain_net_handler_ethereum::create_primary_wallet_transaction(const std::vector<son_sidechain_info> &son_pubkeys, const std::string &object_id) {
   std::vector<std::pair<std::string, uint16_t>> owners_weights;
   for (auto &son : son_pubkeys) {
      const std::string pub_key_str = son.public_key;
      owners_weights.emplace_back(std::make_pair(pub_key_str, son.weight));
   }

   return ethereum::update_owners_encoder::encode(owners_weights, object_id);
}

std::string sidechain_net_handler_ethereum::create_withdrawal_transaction(const son_wallet_withdraw_object &swwo) {
   if (swwo.withdraw_currency == "ETH") {
      return ethereum::withdrawal_encoder::encode(ethereum::remove_0x(swwo.withdraw_address), swwo.withdraw_amount.value * 10000000000, swwo.id.operator std::string());
   } else {
      const auto it = erc20_addresses.left.find(swwo.withdraw_currency);
      if (it == erc20_addresses.left.end()) {
         elog("No erc-20 token: ${symbol}", ("symbol", swwo.withdraw_currency));
         return "";
      }
      return ethereum::withdrawal_erc20_encoder::encode(ethereum::remove_0x(it->second), ethereum::remove_0x(swwo.withdraw_address), swwo.withdraw_amount.value, swwo.id.operator std::string());
   }

   return "";
}

std::string sidechain_net_handler_ethereum::sign_transaction(const sidechain_transaction_object &sto) {
   const auto &current_son = plugin.get_current_son_object(sidechain);
   FC_ASSERT(current_son.sidechain_public_keys.contains(sidechain), "No public keys for current son: ${account_id}", ("account_id", current_son.son_account));

   const auto &public_key = current_son.sidechain_public_keys.at(sidechain);

   //! We need to change v value according to chain_id
   auto signature = ethereum::sign_hash(ethereum::keccak_hash(sto.transaction), ethereum::add_0x(ethereum::to_hex(chain_id)), get_private_key(public_key));
   signature.v = ethereum::to_hex(ethereum::from_hex<unsigned int>(signature.v) - 2 * chain_id - 35 + 27);

   return signature.serialize();
}

void sidechain_net_handler_ethereum::schedule_ethereum_listener() {
   const fc::time_point now = fc::time_point::now();
   const int64_t time_to_next = 5000;

   const fc::time_point next_wakeup(now + fc::milliseconds(time_to_next));

   _listener_task = fc::schedule([this] {
      ethereum_listener_loop();
   },
                                 next_wakeup, "SON Ethereum listener task");
}

void sidechain_net_handler_ethereum::ethereum_listener_loop() {
   schedule_ethereum_listener();

   const auto reply = rpc_client->eth_blockNumber();

   if (!reply.empty()) {
      const uint64_t head_block_number = ethereum::from_hex<uint64_t>(reply);

      if (head_block_number != last_block_received) {
         //! Check that current block number is greater than last one
         if (head_block_number < last_block_received) {
            wlog("Head block ${head_block_number} is greater than last received block ${last_block_received}", ("head_block_number", head_block_number)("last_block_received", last_block_received));
            return;
         }

         //! Send event data for all blocks that passed
         for (uint64_t i = last_block_received + 1; i <= head_block_number; ++i) {
            const std::string block_number = ethereum::add_0x(ethereum::to_hex(i, false));
            handle_event(block_number);
         }

         last_block_received = head_block_number;
      }
   }
}

void sidechain_net_handler_ethereum::handle_event(const std::string &block_number) {
   const std::string block = rpc_client->eth_get_block_by_number(block_number, true);
   if (block != "") {
      add_to_son_listener_log("BLOCK   : " + block_number);
      std::stringstream ss(block);
      boost::property_tree::ptree block_json;
      boost::property_tree::read_json(ss, block_json);

      if (block_json.get<string>("result") == "null") {
         wlog("No data for block ${block_number}", ("block_number", block_number));
         return;
      }

      size_t tx_idx = -1;
      for (const auto &tx_child : block_json.get_child("result.transactions")) {
         const boost::property_tree::ptree tx = tx_child.second;
         tx_idx = tx_idx + 1;

         const std::string from = tx.get<std::string>("from");
         const std::string to = tx.get<std::string>("to");

         std::string cmp_to = to;
         std::transform(cmp_to.begin(), cmp_to.end(), cmp_to.begin(), ::toupper);
         std::string cmp_wallet_contract_address = wallet_contract_address;
         std::transform(cmp_wallet_contract_address.begin(), cmp_wallet_contract_address.end(), cmp_wallet_contract_address.begin(), ::toupper);

         if (cmp_to == cmp_wallet_contract_address) {

            //! Check whether it is ERC-20 token deposit
            std::string symbol;
            boost::multiprecision::uint256_t amount;
            const auto deposit_erc_20 = ethereum::deposit_erc20_decoder::decode(tx.get<std::string>("input"));
            if (deposit_erc_20.valid()) {
               std::string cmp_token = deposit_erc_20->token;
               std::transform(cmp_token.begin(), cmp_token.end(), cmp_token.begin(), ::tolower);

               const auto it = erc20_addresses.right.find(cmp_token);
               if (it == erc20_addresses.right.end()) {
                  wlog("No erc-20 token with address: ${address}", ("address", cmp_token));
                  continue;
               }
               symbol = it->second;
               amount = deposit_erc_20->amount;
            } else {
               symbol = "ETH";
               const std::string value_s = tx.get<std::string>("value");
               amount = boost::multiprecision::uint256_t{value_s};
               amount = amount / 100000;
               amount = amount / 100000;
            }

            const auto &assets_by_symbol = database.get_index_type<asset_index>().indices().get<by_symbol>();
            const auto asset_itr = assets_by_symbol.find(symbol);
            if (asset_itr == assets_by_symbol.end()) {
               wlog("Could not find asset: ${symbol}", ("symbol", symbol));
               continue;
            }

            const auto &sidechain_addresses_idx = database.get_index_type<sidechain_address_index>().indices().get<by_sidechain_and_deposit_address_and_expires>();
            const auto &addr_itr = sidechain_addresses_idx.find(std::make_tuple(sidechain, from, time_point_sec::maximum()));
            if (addr_itr == sidechain_addresses_idx.end()) {
               continue;
            }

            std::stringstream ss;
            ss << "ethereum"
               << "-" << tx.get<std::string>("hash") << "-" << tx_idx;

            sidechain_event_data sed;
            sed.timestamp = database.head_block_time();
            sed.block_num = database.head_block_num();
            sed.sidechain = sidechain;
            sed.type = sidechain_event_type::deposit;
            sed.sidechain_uid = ss.str();
            sed.sidechain_transaction_id = tx.get<std::string>("hash");
            sed.sidechain_from = from;
            sed.sidechain_to = to;
            sed.sidechain_currency = symbol;
            sed.sidechain_amount = amount;
            sed.peerplays_from = addr_itr->sidechain_address_account;
            sed.peerplays_to = database.get_global_properties().parameters.son_account();
            const price price = asset_itr->options.core_exchange_rate;
            sed.peerplays_asset = asset(sed.sidechain_amount * price.base.amount / price.quote.amount);

            add_to_son_listener_log("TRX     : " + sed.sidechain_transaction_id);

            sidechain_event_data_received(sed);
         }
      }
   }
}

}} // namespace graphene::peerplays_sidechain
