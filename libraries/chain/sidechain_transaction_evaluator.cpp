#include <graphene/chain/sidechain_transaction_evaluator.hpp>

#include <graphene/chain/hardfork.hpp>
#include <graphene/chain/database.hpp>
#include <graphene/chain/proposal_object.hpp>
#include <graphene/chain/sidechain_transaction_object.hpp>
#include <graphene/chain/son_object.hpp>

namespace graphene { namespace chain {

void_result sidechain_transaction_create_evaluator::do_evaluate(const sidechain_transaction_create_operation &op)
{ try {
   FC_ASSERT(db().head_block_time() >= HARDFORK_SON_TIME, "Not allowed until SON HARDFORK");
   FC_ASSERT( op.payer == db().get_global_properties().parameters.son_account(), "SON paying account must be set as payer." );

   FC_ASSERT((op.object_id.is<son_wallet_id_type>() || op.object_id.is<son_wallet_deposit_id_type>() || op.object_id.is<son_wallet_withdraw_id_type>()), "Invalid object id");

   const auto &sto_idx = db().get_index_type<sidechain_transaction_index>().indices().get<by_object_id>();
   const auto &sto_obj = sto_idx.find(op.object_id);
   FC_ASSERT(sto_obj == sto_idx.end(), "Sidechain transaction for a given object is already created");

   FC_ASSERT(!op.transaction.empty(), "Sidechain transaction data not set");

   return void_result();
} FC_CAPTURE_AND_RETHROW( ( op ) ) }

object_id_type sidechain_transaction_create_evaluator::do_apply(const sidechain_transaction_create_operation &op)
{ try {
   const auto &new_sidechain_transaction_object = db().create<sidechain_transaction_object>([&](sidechain_transaction_object &sto) {
      sto.sidechain = op.sidechain;
      sto.object_id = op.object_id;
      sto.transaction = op.transaction;
      std::transform(op.signers.begin(), op.signers.end(), std::inserter(sto.signers, sto.signers.end()), [](const son_id_type son_id) {
         return std::make_pair(son_id, false);
      });
      sto.block = db().head_block_id();
      sto.valid = true;
      sto.complete = false;
      sto.sent = false;
   });
   return new_sidechain_transaction_object.id;
} FC_CAPTURE_AND_RETHROW( ( op ) ) }

void_result sidechain_transaction_sign_evaluator::do_evaluate(const sidechain_transaction_sign_operation &op)
{ try {
   FC_ASSERT(db().head_block_time() >= HARDFORK_SON_TIME, "Not allowed until SON HARDFORK"); // can be removed after HF date pass

   const auto &sto_idx = db().get_index_type<sidechain_transaction_index>().indices().get<by_id>();
   const auto &sto_obj = sto_idx.find(op.sidechain_transaction_id);
   FC_ASSERT(sto_obj != sto_idx.end(), "Sidechain transaction object not found");

   const auto &son_idx = db().get_index_type<son_index>().indices().get<by_account>();
   const auto &son_obj = son_idx.find(op.payer);
   FC_ASSERT(son_obj != son_idx.end(), "SON object not found");

   bool expected = false;
   for (auto signer : sto_obj->signers) {
      if (signer.first == son_obj->id) {
          expected = !signer.second;
      }
   }
   FC_ASSERT(expected, "Signer not expected");

   FC_ASSERT(sto_obj->block == op.block, "Sidechain transaction already signed in this block");

   FC_ASSERT(sto_obj->valid, "Transaction not valid");
   FC_ASSERT(!sto_obj->complete, "Transaction signing completed");
   FC_ASSERT(!sto_obj->sent, "Transaction already sent");

   return void_result();
} FC_CAPTURE_AND_RETHROW( ( op ) ) }

object_id_type sidechain_transaction_sign_evaluator::do_apply(const sidechain_transaction_sign_operation &op)
{ try {
   const auto &sto_idx = db().get_index_type<sidechain_transaction_index>().indices().get<by_id>();
   auto sto_obj = sto_idx.find(op.sidechain_transaction_id);

   const auto &son_idx = db().get_index_type<son_index>().indices().get<by_account>();
   auto son_obj = son_idx.find(op.payer);

   db().modify(*sto_obj, [&](sidechain_transaction_object &sto) {
      sto.transaction = op.transaction;
      sto.block = db().head_block_id();
      sto.complete = op.complete;
      for (size_t i = 0; i < sto.signers.size(); i++) {
         if (sto.signers.at(i).first == son_obj->id) {
             sto.signers.at(i).second = true;
        }
      }
   });

   db().modify(son_obj->statistics(db()), [&](son_statistics_object& sso) {
      sso.txs_signed += 1;
   });

   return op.sidechain_transaction_id;
} FC_CAPTURE_AND_RETHROW( ( op ) ) }

void_result sidechain_transaction_send_evaluator::do_evaluate(const sidechain_transaction_send_operation &op)
{ try {
   FC_ASSERT(db().head_block_time() >= HARDFORK_SON_TIME, "Not allowed until SON HARDFORK"); // can be removed after HF date pass

   const auto &sto_idx = db().get_index_type<sidechain_transaction_index>().indices().get<by_id>();
   const auto &sto_obj = sto_idx.find(op.sidechain_transaction_id);
   FC_ASSERT(sto_obj != sto_idx.end(), "Sidechain transaction object not found");

   FC_ASSERT(sto_obj->valid, "Transaction not valid");
   FC_ASSERT(sto_obj->complete, "Transaction signing not complete");
   FC_ASSERT(!sto_obj->sent, "Transaction already sent");

   return void_result();
} FC_CAPTURE_AND_RETHROW( ( op ) ) }

object_id_type sidechain_transaction_send_evaluator::do_apply(const sidechain_transaction_send_operation &op)
{ try {
   const auto &sto_idx = db().get_index_type<sidechain_transaction_index>().indices().get<by_id>();
   auto sto_obj = sto_idx.find(op.sidechain_transaction_id);

   db().modify(*sto_obj, [&](sidechain_transaction_object &sto) {
      sto.block = db().head_block_id();
      sto.sent = true;
   });

   return op.sidechain_transaction_id;
} FC_CAPTURE_AND_RETHROW( ( op ) ) }

} // namespace chain
} // namespace graphene
