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
   FC_ASSERT(op.signers.size() > 0, "Sidechain transaction signers not set");

   return void_result();
} FC_CAPTURE_AND_RETHROW( ( op ) ) }

object_id_type sidechain_transaction_create_evaluator::do_apply(const sidechain_transaction_create_operation &op)
{ try {
   const auto &new_sidechain_transaction_object = db().create<sidechain_transaction_object>([&](sidechain_transaction_object &sto) {
      sto.sidechain = op.sidechain;
      sto.object_id = op.object_id;
      sto.transaction = op.transaction;
      sto.signers = op.signers;
      std::transform(op.signers.begin(), op.signers.end(), std::inserter(sto.signatures, sto.signatures.end()), [](const son_info &si) {
         return std::make_pair(si.son_id, std::string());
      });
      for (const auto &si : op.signers) {
         sto.total_weight = sto.total_weight + si.weight;
      }
      sto.sidechain_transaction = "";
      sto.current_weight = 0;
      sto.threshold = sto.total_weight * 2 / 3 + 1;
      sto.status = sidechain_transaction_status::valid;
   });
   return new_sidechain_transaction_object.id;
} FC_CAPTURE_AND_RETHROW( ( op ) ) }

void_result sidechain_transaction_sign_evaluator::do_evaluate(const sidechain_transaction_sign_operation &op)
{ try {
   FC_ASSERT(db().head_block_time() >= HARDFORK_SON_TIME, "Not allowed until SON HARDFORK"); // can be removed after HF date pass
   FC_ASSERT( op.payer == db().get_global_properties().parameters.son_account(), "SON paying account must be set as payer." );

   const auto &sto_idx = db().get_index_type<sidechain_transaction_index>().indices().get<by_id>();
   const auto &sto_obj = sto_idx.find(op.sidechain_transaction_id);
   FC_ASSERT(sto_obj != sto_idx.end(), "Sidechain transaction object not found");

   const auto &son_idx = db().get_index_type<son_index>().indices().get<by_id>();
   const auto &son_obj = son_idx.find(op.signer);
   FC_ASSERT(son_obj != son_idx.end(), "SON object not found");

   bool expected = false;
   for (auto signature : sto_obj->signatures) {
      if (signature.first == son_obj->id) {
          expected = signature.second.empty();
      }
   }
   FC_ASSERT(expected, "Signer not expected");

   FC_ASSERT(sto_obj->status == sidechain_transaction_status::valid, "Invalid transaction status");

   return void_result();
} FC_CAPTURE_AND_RETHROW( ( op ) ) }

object_id_type sidechain_transaction_sign_evaluator::do_apply(const sidechain_transaction_sign_operation &op)
{ try {
   const auto &sto_idx = db().get_index_type<sidechain_transaction_index>().indices().get<by_id>();
   auto sto_obj = sto_idx.find(op.sidechain_transaction_id);

   const auto &son_idx = db().get_index_type<son_index>().indices().get<by_id>();
   auto son_obj = son_idx.find(op.signer);

   db().modify(*sto_obj, [&](sidechain_transaction_object &sto) {
      for (size_t i = 0; i < sto.signatures.size(); i++) {
         if (sto.signatures.at(i).first == son_obj->id) {
             sto.signatures.at(i).second = op.signature;
         }
      }
      for (size_t i = 0; i < sto.signers.size(); i++) {
         if (sto.signers.at(i).son_id == son_obj->id) {
             sto.current_weight = sto.current_weight + sto.signers.at(i).weight;
         }
      }
      if (sto.current_weight >= sto.threshold) {
          sto.status = sidechain_transaction_status::complete;
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

   FC_ASSERT(sto_obj->status == sidechain_transaction_status::complete, "Invalid transaction status");

   return void_result();
} FC_CAPTURE_AND_RETHROW( ( op ) ) }

object_id_type sidechain_transaction_send_evaluator::do_apply(const sidechain_transaction_send_operation &op)
{ try {
   const auto &sto_idx = db().get_index_type<sidechain_transaction_index>().indices().get<by_id>();
   auto sto_obj = sto_idx.find(op.sidechain_transaction_id);

   db().modify(*sto_obj, [&](sidechain_transaction_object &sto) {
      sto.sidechain_transaction = op.sidechain_transaction;
      sto.status = sidechain_transaction_status::sent;
   });

   return op.sidechain_transaction_id;
} FC_CAPTURE_AND_RETHROW( ( op ) ) }


void_result sidechain_transaction_settle_evaluator::do_evaluate(const sidechain_transaction_settle_operation &op)
{ try {
   FC_ASSERT(db().head_block_time() >= HARDFORK_SON_TIME, "Not allowed until SON HARDFORK"); // can be removed after HF date pass

   const auto &sto_idx = db().get_index_type<sidechain_transaction_index>().indices().get<by_id>();
   const auto &sto_obj = sto_idx.find(op.sidechain_transaction_id);
   FC_ASSERT(sto_obj != sto_idx.end(), "Sidechain transaction object not found");

   FC_ASSERT(sto_obj->status == sidechain_transaction_status::sent, "Invalid transaction status");

   return void_result();
} FC_CAPTURE_AND_RETHROW( ( op ) ) }

object_id_type sidechain_transaction_settle_evaluator::do_apply(const sidechain_transaction_settle_operation &op)
{ try {
   const auto &sto_idx = db().get_index_type<sidechain_transaction_index>().indices().get<by_id>();
   auto sto_obj = sto_idx.find(op.sidechain_transaction_id);

   db().modify(*sto_obj, [&](sidechain_transaction_object &sto) {
      sto.status = sidechain_transaction_status::settled;
   });

   return op.sidechain_transaction_id;
} FC_CAPTURE_AND_RETHROW( ( op ) ) }

} // namespace chain
} // namespace graphene
