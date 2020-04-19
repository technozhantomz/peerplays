#include <graphene/peerplays_sidechain/sidechain_net_handler_peerplays.hpp>

#include <algorithm>
#include <thread>

#include <boost/algorithm/hex.hpp>
#include <boost/property_tree/json_parser.hpp>
#include <boost/property_tree/ptree.hpp>

#include <fc/crypto/base64.hpp>
#include <fc/crypto/hex.hpp>
#include <fc/log/logger.hpp>
#include <fc/network/ip.hpp>

#include <graphene/chain/account_object.hpp>
#include <graphene/chain/protocol/son_wallet.hpp>
#include <graphene/chain/son_info.hpp>
#include <graphene/chain/son_wallet_object.hpp>
#include <graphene/utilities/key_conversion.hpp>

namespace graphene { namespace peerplays_sidechain {

sidechain_net_handler_peerplays::sidechain_net_handler_peerplays(peerplays_sidechain_plugin &_plugin, const boost::program_options::variables_map &options) :
      sidechain_net_handler(_plugin, options) {
   sidechain = sidechain_type::peerplays;

   if (options.count("peerplays-private-key")) {
      const std::vector<std::string> pub_priv_keys = options["peerplays-private-key"].as<std::vector<std::string>>();
      for (const std::string &itr_key_pair : pub_priv_keys) {
         auto key_pair = graphene::app::dejsonify<std::pair<std::string, std::string>>(itr_key_pair, 5);
         ilog("Peerplays Public Key: ${public}", ("public", key_pair.first));
         if (!key_pair.first.length() || !key_pair.second.length()) {
            FC_THROW("Invalid public private key pair.");
         }
         private_keys[key_pair.first] = key_pair.second;
      }
   }

   database.applied_block.connect([&](const signed_block &b) {
      on_applied_block(b);
   });
}

sidechain_net_handler_peerplays::~sidechain_net_handler_peerplays() {
}

bool sidechain_net_handler_peerplays::process_proposal(const proposal_object &po) {

   ilog("Proposal to process: ${po}, SON id ${son_id}", ("po", po.id)("son_id", plugin.get_current_son_id()));

   bool should_approve = false;

   const chain::global_property_object &gpo = database.get_global_properties();

   int32_t op_idx_0 = -1;
   chain::operation op_obj_idx_0;

   if (po.proposed_transaction.operations.size() >= 1) {
      op_idx_0 = po.proposed_transaction.operations[0].which();
      op_obj_idx_0 = po.proposed_transaction.operations[0];
   }

   switch (op_idx_0) {

   case chain::operation::tag<chain::son_wallet_update_operation>::value: {
      should_approve = false;
      break;
   }

   case chain::operation::tag<chain::son_wallet_deposit_process_operation>::value: {
      son_wallet_deposit_id_type swdo_id = op_obj_idx_0.get<son_wallet_deposit_process_operation>().son_wallet_deposit_id;
      const auto &idx = database.get_index_type<son_wallet_deposit_index>().indices().get<by_id>();
      const auto swdo = idx.find(swdo_id);
      if (swdo != idx.end()) {

         uint32_t swdo_block_num = swdo->block_num;
         std::string swdo_sidechain_transaction_id = swdo->sidechain_transaction_id;
         uint32_t swdo_op_idx = std::stoll(swdo->sidechain_uid.substr(swdo->sidechain_uid.find_last_of("-") + 1));

         const auto &block = database.fetch_block_by_number(swdo_block_num);

         for (const auto &tx : block->transactions) {
            if (tx.id().str() == swdo_sidechain_transaction_id) {
               operation op = tx.operations[swdo_op_idx];
               transfer_operation t_op = op.get<transfer_operation>();

               asset sidechain_asset = asset(swdo->sidechain_amount, fc::variant(swdo->sidechain_currency, 1).as<asset_id_type>(1));
               price sidechain_asset_price = database.get<asset_object>(sidechain_asset.asset_id).options.core_exchange_rate;
               asset peerplays_asset = asset(sidechain_asset.amount * sidechain_asset_price.base.amount / sidechain_asset_price.quote.amount);

               should_approve = (gpo.parameters.son_account() == t_op.to) &&
                                (swdo->peerplays_from == t_op.from) &&
                                (sidechain_asset == t_op.amount) &&
                                (swdo->peerplays_asset == peerplays_asset);
               break;
            }
         }
      }
      break;
   }

   case chain::operation::tag<chain::son_wallet_withdraw_process_operation>::value: {
      should_approve = false;
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

void sidechain_net_handler_peerplays::process_primary_wallet() {
   return;
}

void sidechain_net_handler_peerplays::process_sidechain_addresses() {
   return;
}

bool sidechain_net_handler_peerplays::process_deposit(const son_wallet_deposit_object &swdo) {

   const chain::global_property_object &gpo = database.get_global_properties();

   asset_issue_operation ai_op;
   ai_op.issuer = gpo.parameters.son_account();
   price btc_price = database.get<asset_object>(database.get_global_properties().parameters.btc_asset()).options.core_exchange_rate;
   ai_op.asset_to_issue = asset(swdo.peerplays_asset.amount * btc_price.quote.amount / btc_price.base.amount, database.get_global_properties().parameters.btc_asset());
   ai_op.issue_to_account = swdo.peerplays_from;

   signed_transaction tx;
   auto dyn_props = database.get_dynamic_global_properties();
   tx.set_reference_block(dyn_props.head_block_id);
   tx.set_expiration(database.head_block_time() + gpo.parameters.maximum_time_until_expiration);
   tx.operations.push_back(ai_op);
   database.current_fee_schedule().set_fee(tx.operations.back());

   std::stringstream ss;
   fc::raw::pack(ss, tx, 1000);
   std::string tx_str = boost::algorithm::hex(ss.str());

   if (!tx_str.empty()) {
      const chain::global_property_object &gpo = database.get_global_properties();

      sidechain_transaction_create_operation stc_op;
      stc_op.payer = gpo.parameters.son_account();
      stc_op.object_id = swdo.id;
      stc_op.sidechain = sidechain;
      stc_op.transaction = tx_str;
      stc_op.signers = gpo.active_sons;

      proposal_create_operation proposal_op;
      proposal_op.fee_paying_account = plugin.get_current_son_object().son_account;
      proposal_op.proposed_ops.emplace_back(stc_op);
      uint32_t lifetime = (gpo.parameters.block_interval * gpo.active_witnesses.size()) * 3;
      proposal_op.expiration_time = time_point_sec(database.head_block_time().sec_since_epoch() + lifetime);

      signed_transaction trx = database.create_signed_transaction(plugin.get_private_key(plugin.get_current_son_id()), proposal_op);
      try {
         trx.validate();
         database.push_transaction(trx, database::validation_steps::skip_block_size_check);
         if (plugin.app().p2p_node())
            plugin.app().p2p_node()->broadcast(net::trx_message(trx));
         return true;
      } catch (fc::exception e) {
         elog("Sending proposal for deposit sidechain transaction create operation failed with exception ${e}", ("e", e.what()));
         return false;
      }
   }
   return false;
}

bool sidechain_net_handler_peerplays::process_withdrawal(const son_wallet_withdraw_object &swwo) {
   return true;
}

std::string sidechain_net_handler_peerplays::process_sidechain_transaction(const sidechain_transaction_object &sto) {

   std::stringstream ss_trx(boost::algorithm::unhex(sto.transaction));
   signed_transaction trx;
   fc::raw::unpack(ss_trx, trx, 1000);

   fc::optional<fc::ecc::private_key> privkey = graphene::utilities::wif_to_key(get_private_key(plugin.get_current_son_object().sidechain_public_keys.at(sidechain)));
   signature_type st = trx.sign(*privkey, database.get_chain_id());

   std::stringstream ss_st;
   fc::raw::pack(ss_st, st, 1000);
   std::string st_str = boost::algorithm::hex(ss_st.str());

   return st_str;
}

std::string sidechain_net_handler_peerplays::send_sidechain_transaction(const sidechain_transaction_object &sto) {

   std::stringstream ss_trx(boost::algorithm::unhex(sto.transaction));
   signed_transaction trx;
   fc::raw::unpack(ss_trx, trx, 1000);

   for (auto signature : sto.signatures) {
      if (!signature.second.empty()) {
         std::stringstream ss_st(boost::algorithm::unhex(signature.second));
         signature_type st;
         fc::raw::unpack(ss_st, st, 1000);

         trx.signatures.push_back(st);
         trx.signees.clear();
      }
   }

   try {
      trx.validate();
      database.push_transaction(trx, database::validation_steps::skip_block_size_check);
      if (plugin.app().p2p_node())
         plugin.app().p2p_node()->broadcast(net::trx_message(trx));
      return trx.id().str();
   } catch (fc::exception e) {
      elog("Sidechain transaction failed with exception ${e}", ("e", e.what()));
      return "";
   }

   return "";
}

int64_t sidechain_net_handler_peerplays::settle_sidechain_transaction(const sidechain_transaction_object &sto) {
   int64_t settle_amount = 0;
   return settle_amount;
}

void sidechain_net_handler_peerplays::on_applied_block(const signed_block &b) {
   for (const auto &trx : b.transactions) {
      size_t operation_index = -1;
      for (auto op : trx.operations) {
         operation_index = operation_index + 1;
         if (op.which() == operation::tag<transfer_operation>::value) {
            transfer_operation transfer_op = op.get<transfer_operation>();
            if (transfer_op.to != plugin.database().get_global_properties().parameters.son_account()) {
               continue;
            }

            std::stringstream ss;
            ss << "peerplays"
               << "-" << trx.id().str() << "-" << operation_index;
            std::string sidechain_uid = ss.str();

            sidechain_event_data sed;
            sed.timestamp = database.head_block_time();
            sed.block_num = database.head_block_num();
            sed.sidechain = sidechain_type::peerplays;
            sed.sidechain_uid = sidechain_uid;
            sed.sidechain_transaction_id = trx.id().str();
            sed.sidechain_from = fc::to_string(transfer_op.from.space_id) + "." + fc::to_string(transfer_op.from.type_id) + "." + fc::to_string((uint64_t)transfer_op.from.instance);
            sed.sidechain_to = fc::to_string(transfer_op.to.space_id) + "." + fc::to_string(transfer_op.to.type_id) + "." + fc::to_string((uint64_t)transfer_op.to.instance);
            sed.sidechain_currency = fc::to_string(transfer_op.amount.asset_id.space_id) + "." + fc::to_string(transfer_op.amount.asset_id.type_id) + "." + fc::to_string((uint64_t)transfer_op.amount.asset_id.instance);
            sed.sidechain_amount = transfer_op.amount.amount;
            sed.peerplays_from = transfer_op.from;
            sed.peerplays_to = transfer_op.to;
            price asset_price = database.get<asset_object>(transfer_op.amount.asset_id).options.core_exchange_rate;
            sed.peerplays_asset = asset(transfer_op.amount.amount * asset_price.base.amount / asset_price.quote.amount);
            sidechain_event_data_received(sed);
         }
      }
   }
}

}} // namespace graphene::peerplays_sidechain
