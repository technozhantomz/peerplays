#include <graphene/chain/son_wallet_withdraw_evaluator.hpp>

#include <graphene/chain/database.hpp>
#include <graphene/chain/is_authorized_asset.hpp>
#include <graphene/chain/son_wallet_withdraw_object.hpp>

namespace graphene { namespace chain {

void_result create_son_wallet_withdraw_evaluator::do_evaluate(const son_wallet_withdraw_create_operation& op)
{ try{
   FC_ASSERT(db().head_block_time() >= HARDFORK_SON_TIME, "Not allowed until SON HARDFORK");
   FC_ASSERT( op.payer == GRAPHENE_SON_ACCOUNT, "SON paying account must be set as payer." );

   return void_result();
} FC_CAPTURE_AND_RETHROW( (op) ) }

object_id_type create_son_wallet_withdraw_evaluator::do_apply(const son_wallet_withdraw_create_operation& op)
{ try {
   const auto& idx = db().get_index_type<son_wallet_withdraw_index>().indices().get<by_peerplays_uid>();
   auto itr = idx.find(op.peerplays_uid);
   if (itr == idx.end()) {
      const auto& new_son_wallet_withdraw_object = db().create<son_wallet_withdraw_object>( [&]( son_wallet_withdraw_object& swwo ){
         swwo.timestamp = op.timestamp;
         swwo.sidechain = op.sidechain;
         swwo.confirmations = 1;
         swwo.peerplays_uid = op.peerplays_uid;
         swwo.peerplays_transaction_id = op.peerplays_transaction_id;
         swwo.peerplays_from = op.peerplays_from;
         swwo.peerplays_asset = op.peerplays_asset;
         swwo.withdraw_sidechain = op.withdraw_sidechain;
         swwo.withdraw_address = op.withdraw_address;
         swwo.withdraw_currency = op.withdraw_currency;
         swwo.withdraw_amount = op.withdraw_amount;
         swwo.processed = false;
      });
      return new_son_wallet_withdraw_object.id;
   } else {
      db().modify(*itr, [&op](son_wallet_withdraw_object &swto) {
         swto.confirmations = swto.confirmations + 1;
      });
      return (*itr).id;
   }
} FC_CAPTURE_AND_RETHROW( (op) ) }

void_result process_son_wallet_withdraw_evaluator::do_evaluate(const son_wallet_withdraw_process_operation& op)
{ try{
   FC_ASSERT(db().head_block_time() >= HARDFORK_SON_TIME, "Not allowed until SON HARDFORK");
   FC_ASSERT( op.payer == GRAPHENE_SON_ACCOUNT, "SON paying account must be set as payer." );

   const auto& idx = db().get_index_type<son_wallet_withdraw_index>().indices().get<by_id>();
   const auto& itr = idx.find(op.son_wallet_withdraw_id);
   FC_ASSERT(itr != idx.end(), "Son wallet withdraw not found");
} FC_CAPTURE_AND_RETHROW( (op) ) }

object_id_type process_son_wallet_withdraw_evaluator::do_apply(const son_wallet_withdraw_process_operation& op)
{ try {
   const auto& idx = db().get_index_type<son_wallet_withdraw_index>().indices().get<by_id>();
   auto itr = idx.find(op.son_wallet_withdraw_id);
   if(itr != idx.end())
   {
       if (itr->processed == false) {
          db().modify(*itr, [&op](son_wallet_withdraw_object &swto) {
             swto.processed = true;
          });
       }
   }
   return op.son_wallet_withdraw_id;
} FC_CAPTURE_AND_RETHROW( (op) ) }

} } // namespace graphene::chain
