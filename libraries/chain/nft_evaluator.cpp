#include <graphene/chain/nft_evaluator.hpp>
#include <graphene/chain/nft_object.hpp>
#include <graphene/chain/protocol/operations.hpp>
#include <graphene/chain/account_role_object.hpp>
#include <graphene/chain/hardfork.hpp>

namespace graphene { namespace chain {

void_result nft_metadata_create_evaluator::do_evaluate( const nft_metadata_create_operation& op )
{ try {
   auto now = db().head_block_time();
   FC_ASSERT(now >= HARDFORK_NFT_TIME, "Not allowed until NFT HF");
   op.owner(db());
   const auto& idx_nft_md_by_name = db().get_index_type<nft_metadata_index>().indices().get<by_name>();
   FC_ASSERT( idx_nft_md_by_name.find(op.name) == idx_nft_md_by_name.end(), "NFT name already in use" );
   const auto& idx_nft_md_by_symbol = db().get_index_type<nft_metadata_index>().indices().get<by_symbol>();
   FC_ASSERT( idx_nft_md_by_symbol.find(op.symbol) == idx_nft_md_by_symbol.end(), "NFT symbol already in use" );
   FC_ASSERT((op.revenue_partner && op.revenue_split) || (!op.revenue_partner && !op.revenue_split), "NFT revenue partner info invalid");
   if (op.revenue_partner) {
       (*op.revenue_partner)(db());
       FC_ASSERT(*op.revenue_split >= 0 && *op.revenue_split <= GRAPHENE_100_PERCENT, "Revenue split percent invalid");
   }
   if(op.account_role) {
      const auto& ar_obj = (*op.account_role)(db());
      FC_ASSERT(ar_obj.owner == op.owner, "Only the Account Role created by the owner can be attached");
   }

   // Lottery Related
   if (!op.lottery_options) {
      return void_result();
   }
   FC_ASSERT((*op.lottery_options).end_date > now || (*op.lottery_options).end_date == time_point_sec());
   if (op.max_supply) {
      FC_ASSERT(*op.max_supply >= 5);
   }

   for(auto lottery_id: (*op.lottery_options).progressive_jackpots) {
      const auto& lottery_obj = lottery_id(db());
      FC_ASSERT(lottery_obj.owner == op.owner, "Only the Owner can attach progressive jackpots");
      FC_ASSERT(lottery_obj.is_lottery(), "Only lottery objects can be attached as progressive jackpots");
      FC_ASSERT(lottery_obj.lottery_data->lottery_options.is_active == false, "Lottery should not be active");
      FC_ASSERT(lottery_obj.lottery_data->lottery_options.ticket_price.asset_id == (*op.lottery_options).ticket_price.asset_id, "Lottery asset type should be same");
      const auto& lottery_balance_obj = lottery_obj.lottery_data->lottery_balance_id(db());
      FC_ASSERT(lottery_balance_obj.jackpot.amount > 0, "Non zero progressive jackpot not allowed");
   }

   return void_result();
} FC_CAPTURE_AND_RETHROW( (op) ) }

object_id_type nft_metadata_create_evaluator::do_apply( const nft_metadata_create_operation& op )
{ try {
   const auto& new_nft_metadata_object = db().create<nft_metadata_object>( [&]( nft_metadata_object& obj ){
      obj.owner = op.owner;
      obj.name = op.name;
      obj.symbol = op.symbol;
      obj.base_uri = op.base_uri;
      obj.revenue_partner = op.revenue_partner;
      obj.revenue_split = op.revenue_split;
      obj.is_transferable = op.is_transferable;
      obj.is_sellable = op.is_sellable;
      obj.account_role = op.account_role;
      if (op.max_supply) {
         obj.max_supply = *op.max_supply;
      }
      if (op.lottery_options) {
         asset jackpot_sum(0,(*op.lottery_options).ticket_price.asset_id);
         for(auto lottery_id: (*op.lottery_options).progressive_jackpots) {
            const auto& lottery_obj = lottery_id(db());
            const auto& lottery_balance_obj = lottery_obj.lottery_data->lottery_balance_id(db());
            FC_ASSERT(lottery_balance_obj.jackpot.amount > 0, "Non zero progressive jackpot not allowed");
            db().modify(lottery_balance_obj, [&] ( nft_lottery_balance_object& nlbo ) {
               jackpot_sum += nlbo.jackpot;
               nlbo.jackpot -= nlbo.jackpot;
            });
         }
         const auto& new_lottery_balance_obj = db().create<nft_lottery_balance_object>([&](nft_lottery_balance_object& nlbo) {
            nlbo.total_progressive_jackpot = jackpot_sum;
            nlbo.jackpot = jackpot_sum;
         });
         obj.lottery_data = nft_lottery_data(*op.lottery_options, new_lottery_balance_obj.id);
      }
   });
   return new_nft_metadata_object.id;
} FC_CAPTURE_AND_RETHROW( (op) ) }


void_result nft_metadata_update_evaluator::do_evaluate( const nft_metadata_update_operation& op )
{ try {
   auto now = db().head_block_time();
   FC_ASSERT(now >= HARDFORK_NFT_TIME, "Not allowed until NFT HF");
   op.owner(db());
   const auto& idx_nft_md = db().get_index_type<nft_metadata_index>().indices().get<by_id>();
   auto itr_nft_md = idx_nft_md.find(op.nft_metadata_id);
   FC_ASSERT( itr_nft_md != idx_nft_md.end(), "NFT metadata not found" );
   FC_ASSERT( itr_nft_md->owner == op.owner, "Only owner can modify NFT metadata" );
   const auto& idx_nft_md_by_name = db().get_index_type<nft_metadata_index>().indices().get<by_name>();
   const auto& idx_nft_md_by_symbol = db().get_index_type<nft_metadata_index>().indices().get<by_symbol>();
   if (op.name.valid())
       FC_ASSERT((itr_nft_md->name != *op.name) && (idx_nft_md_by_name.find(*op.name) == idx_nft_md_by_name.end()), "NFT name already in use");
   if (op.symbol.valid())
       FC_ASSERT((itr_nft_md->symbol != *op.symbol) && (idx_nft_md_by_symbol.find(*op.symbol) == idx_nft_md_by_symbol.end()), "NFT symbol already in use");
   FC_ASSERT((op.revenue_partner && op.revenue_split) || (!op.revenue_partner && !op.revenue_split), "NFT revenue partner info invalid");
   if (op.revenue_partner) {
       (*op.revenue_partner)(db());
       FC_ASSERT(*op.revenue_split >= 0 && *op.revenue_split <= GRAPHENE_100_PERCENT, "Revenue split percent invalid");
   }
   if(op.account_role) {
      const auto& ar_obj = (*op.account_role)(db());
      FC_ASSERT(ar_obj.owner == op.owner, "Only the Account Role created by the owner can be attached");
   }
   return void_result();
} FC_CAPTURE_AND_RETHROW( (op) ) }

void_result nft_metadata_update_evaluator::do_apply( const nft_metadata_update_operation& op )
{ try {
   db().modify(db().get(op.nft_metadata_id), [&] ( nft_metadata_object& obj ) {
      if( op.name.valid() )
         obj.name = *op.name;
      if( op.symbol.valid() )
         obj.symbol = *op.symbol;
      if( op.base_uri.valid() )
         obj.base_uri = *op.base_uri;
      if( op.revenue_partner.valid() )
         obj.revenue_partner = op.revenue_partner;
      if( op.revenue_split.valid() )
         obj.revenue_split = op.revenue_split;
      if( op.is_transferable.valid() )
         obj.is_transferable = *op.is_transferable;
      if( op.is_sellable.valid() )
         obj.is_sellable = *op.is_sellable;
      if( op.account_role.valid() )
         obj.account_role = op.account_role;
   });
   return void_result();
} FC_CAPTURE_AND_RETHROW( (op) ) }


void_result nft_mint_evaluator::do_evaluate( const nft_mint_operation& op )
{ try {
   auto now = db().head_block_time();
   FC_ASSERT(now >= HARDFORK_NFT_TIME, "Not allowed until NFT HF");
   op.payer(db());
   op.owner(db());
   op.approved(db());
   for(const auto& op_iter: op.approved_operators) {
      op_iter(db());
   }
   const auto& idx_nft_md = db().get_index_type<nft_metadata_index>().indices().get<by_id>();
   auto itr_nft_md = idx_nft_md.find(op.nft_metadata_id);
   FC_ASSERT( itr_nft_md != idx_nft_md.end(), "NFT metadata not found" );
   FC_ASSERT( itr_nft_md->owner == op.payer, "Only metadata owner can mint NFT" );

   FC_ASSERT(itr_nft_md->get_token_current_supply(db()) < itr_nft_md->max_supply, "NFTs can't be minted more than max_supply");
   return void_result();
} FC_CAPTURE_AND_RETHROW( (op) ) }

object_id_type nft_mint_evaluator::do_apply( const nft_mint_operation& op )
{ try {
   const auto& new_nft_object = db().create<nft_object>( [&]( nft_object& obj ){
      obj.nft_metadata_id = op.nft_metadata_id;
      obj.owner = op.owner;
      obj.approved = op.approved;
      obj.approved_operators = op.approved_operators;
      obj.token_uri = op.token_uri;
   });
   return new_nft_object.id;
} FC_CAPTURE_AND_RETHROW( (op) ) }


void_result nft_safe_transfer_from_evaluator::do_evaluate( const nft_safe_transfer_from_operation& op )
{ try {
   auto now = db().head_block_time();
   FC_ASSERT(now >= HARDFORK_NFT_TIME, "Not allowed until NFT HF");
   const auto& idx_nft = db().get_index_type<nft_index>().indices().get<by_id>();
   const auto& idx_acc = db().get_index_type<account_index>().indices().get<by_id>();

   auto itr_nft = idx_nft.find(op.token_id);
   FC_ASSERT( itr_nft != idx_nft.end(), "NFT does not exists" );

   FC_ASSERT(!db().item_locked(op.token_id), "Item(s) is already on sale on market, transfer is not allowed");

   auto itr_operator = idx_acc.find(op.operator_);
   FC_ASSERT( itr_operator != idx_acc.end(), "Operator account does not exists" );

   auto itr_owner = idx_acc.find(itr_nft->owner);
   FC_ASSERT( itr_owner != idx_acc.end(), "Owner account does not exists" );

   auto itr_from = idx_acc.find(op.from);
   FC_ASSERT( itr_from != idx_acc.end(), "Sender account does not exists" );
   FC_ASSERT( itr_from->id == itr_owner->id, "Sender account is not owner of this NFT" );

   auto itr_to = idx_acc.find(op.to);
   FC_ASSERT( itr_to != idx_acc.end(), "Receiver account does not exists" );

   auto itr_approved_op = std::find(itr_nft->approved_operators.begin(), itr_nft->approved_operators.end(), op.operator_);
   FC_ASSERT( (itr_nft->owner == op.operator_) || (itr_nft->approved == itr_operator->id) || (itr_approved_op != itr_nft->approved_operators.end()), "Operator is not NFT owner or approved operator" );

   const auto& nft_meta_obj = itr_nft->nft_metadata_id(db());
   FC_ASSERT( nft_meta_obj.is_transferable == true, "NFT is not transferable");

   if (nft_meta_obj.account_role)
   {
      const auto &ar_idx = db().get_index_type<account_role_index>().indices().get<by_id>();
      auto ar_itr = ar_idx.find(*nft_meta_obj.account_role);
      if(ar_itr != ar_idx.end())
      {
         FC_ASSERT(db().account_role_valid(*ar_itr, op.operator_, get_type()), "Account role not valid");
         FC_ASSERT(db().account_role_valid(*ar_itr, op.to), "Account role not valid");
      }
   }

   return void_result();
} FC_CAPTURE_AND_RETHROW( (op) ) }

object_id_type nft_safe_transfer_from_evaluator::do_apply( const nft_safe_transfer_from_operation& op )
{ try {
   const auto& idx = db().get_index_type<nft_index>().indices().get<by_id>();
   auto itr = idx.find(op.token_id);
   if (itr != idx.end())
   {
      db().modify(*itr, [&op](nft_object &obj) {
         obj.owner = op.to;
         obj.approved = {};
         obj.approved_operators.clear();
      });
   }
   return op.token_id;
} FC_CAPTURE_AND_RETHROW( (op) ) }


void_result nft_approve_evaluator::do_evaluate( const nft_approve_operation& op )
{ try {
   auto now = db().head_block_time();
   FC_ASSERT(now >= HARDFORK_NFT_TIME, "Not allowed until NFT HF");
   const auto& idx_nft = db().get_index_type<nft_index>().indices().get<by_id>();
   const auto& idx_acc = db().get_index_type<account_index>().indices().get<by_id>();

   auto itr_nft = idx_nft.find(op.token_id);
   FC_ASSERT( itr_nft != idx_nft.end(), "NFT does not exists" );

   auto itr_owner = idx_acc.find(op.operator_);
   FC_ASSERT( itr_owner != idx_acc.end(), "Owner account does not exists" );

   auto itr_approved = idx_acc.find(op.approved);
   FC_ASSERT( itr_approved != idx_acc.end(), "Approved account does not exists" );

   auto itr_approved_op = std::find(itr_nft->approved_operators.begin(), itr_nft->approved_operators.end(), op.operator_);
   FC_ASSERT( (itr_nft->owner == itr_owner->id) || (itr_approved_op != itr_nft->approved_operators.end()), "Sender is not NFT owner or approved operator" );

   return void_result();
} FC_CAPTURE_AND_RETHROW( (op) ) }

object_id_type nft_approve_evaluator::do_apply( const nft_approve_operation& op )
{ try {
   const auto& idx = db().get_index_type<nft_index>().indices().get<by_id>();
   auto itr = idx.find(op.token_id);
   if (itr != idx.end())
   {
      db().modify(*itr, [&op](nft_object &obj) {
         obj.approved = op.approved;
         //auto itr = std::find(obj.approved_operators.begin(), obj.approved_operators.end(), op.approved);
         //if (itr == obj.approved_operators.end()) {
         //   obj.approved_operators.push_back(op.approved);
         //}
      });
   }
   return op.token_id;
} FC_CAPTURE_AND_RETHROW( (op) ) }


void_result nft_set_approval_for_all_evaluator::do_evaluate( const nft_set_approval_for_all_operation& op )
{ try {
   auto now = db().head_block_time();
   FC_ASSERT(now >= HARDFORK_NFT_TIME, "Not allowed until NFT HF");
   op.owner(db());
   const auto& idx_acc = db().get_index_type<account_index>().indices().get<by_id>();

   auto itr_operator = idx_acc.find(op.operator_);
   FC_ASSERT( itr_operator != idx_acc.end(), "Operator account does not exists" );

   return void_result();
} FC_CAPTURE_AND_RETHROW( (op) ) }

void_result nft_set_approval_for_all_evaluator::do_apply( const nft_set_approval_for_all_operation& op )
{ try {
   const auto &idx = db().get_index_type<nft_index>().indices().get<by_owner>();
   const auto &idx_range = idx.equal_range(op.owner);
   std::for_each(idx_range.first, idx_range.second, [&](const nft_object &obj) {
      db().modify(obj, [&op](nft_object &obj) {
         auto itr = std::find(obj.approved_operators.begin(), obj.approved_operators.end(), op.operator_);
         if ((op.approved) && (itr == obj.approved_operators.end())) {
            obj.approved_operators.push_back(op.operator_);
         }
         if ((!op.approved) && (itr != obj.approved_operators.end())) {
            obj.approved_operators.erase(itr);
         }
      });
   });
   return void_result();
} FC_CAPTURE_AND_RETHROW( (op) ) }

} } // graphene::chain

