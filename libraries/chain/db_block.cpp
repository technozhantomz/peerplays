/*
 * Copyright (c) 2015 Cryptonomex, Inc., and contributors.
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

#include <graphene/chain/database.hpp>
#include <graphene/chain/db_with.hpp>
#include <graphene/chain/hardfork.hpp>

#include <graphene/chain/protocol/operations.hpp>
#include <graphene/chain/protocol/betting_market.hpp>

#include <graphene/chain/block_summary_object.hpp>
#include <graphene/chain/global_property_object.hpp>
#include <graphene/chain/operation_history_object.hpp>

#include <graphene/chain/proposal_object.hpp>
#include <graphene/chain/transaction_object.hpp>
#include <graphene/chain/witness_object.hpp>
#include <graphene/chain/protocol/fee_schedule.hpp>
#include <graphene/chain/exceptions.hpp>
#include <graphene/chain/evaluator.hpp>
#include <graphene/chain/witness_schedule_object.hpp>
#include <fc/crypto/digest.hpp>


namespace {

   struct proposed_operations_digest_accumulator
   {
      typedef void result_type;

      void operator()(const graphene::chain::proposal_create_operation& proposal)
      {
         for (auto& operation: proposal.proposed_ops)
         {
            proposed_operations_digests.push_back(fc::digest(operation.op));
         }
      }

      //empty template method is needed for all other operation types
      //we can ignore them, we are interested in only proposal_create_operation
      template<class T>
      void operator()(const T&)
      {}

      std::vector<fc::sha256> proposed_operations_digests;
   };

   std::vector<fc::sha256> gather_proposed_operations_digests(const graphene::chain::transaction& trx)
   {
      proposed_operations_digest_accumulator digest_accumulator;

      for (auto& operation: trx.operations)
      {
         if( operation.which() != graphene::chain::operation::tag<graphene::chain::betting_market_group_create_operation>::value
          && operation.which() != graphene::chain::operation::tag<graphene::chain::betting_market_create_operation>::value )
            operation.visit(digest_accumulator);
         else
            edump( ("Found dup"));
      }

      return digest_accumulator.proposed_operations_digests;
   }
}

namespace graphene { namespace chain {

bool database::is_known_block( const block_id_type& id )const
{
   return _fork_db.is_known_block(id) || _block_id_to_block.contains(id);
}
/**
 * Only return true *if* the transaction has not expired or been invalidated. If this
 * method is called with a VERY old transaction we will return false, they should
 * query things by blocks if they are that old.
 */
bool database::is_known_transaction( const transaction_id_type& id )const
{
   const auto& trx_idx = get_index_type<transaction_index>().indices().get<by_trx_id>();
   return trx_idx.find( id ) != trx_idx.end();
}

block_id_type  database::get_block_id_for_num( uint32_t block_num )const
{ try {
   return _block_id_to_block.fetch_block_id( block_num );
} FC_CAPTURE_AND_RETHROW( (block_num) ) }

optional<signed_block> database::fetch_block_by_id( const block_id_type& id )const
{
   auto b = _fork_db.fetch_block( id );
   if( !b )
      return _block_id_to_block.fetch_optional(id);
   return b->data;
}

optional<signed_block> database::fetch_block_by_number( uint32_t num )const
{
   auto results = _fork_db.fetch_block_by_number(num);
   if( results.size() == 1 )
      return results[0]->data;
   else
      return _block_id_to_block.fetch_by_number(num);
   return optional<signed_block>();
}

const signed_transaction& database::get_recent_transaction(const transaction_id_type& trx_id) const
{
   auto& index = get_index_type<transaction_index>().indices().get<by_trx_id>();
   auto itr = index.find(trx_id);
   FC_ASSERT(itr != index.end());
   return itr->trx;
}

std::vector<block_id_type> database::get_block_ids_on_fork(block_id_type head_of_fork) const
{
  pair<fork_database::branch_type, fork_database::branch_type> branches = _fork_db.fetch_branch_from(head_block_id(), head_of_fork);
  if( !((branches.first.back()->previous_id() == branches.second.back()->previous_id())) )
  {
     edump( (head_of_fork)
            (head_block_id())
            (branches.first.size())
            (branches.second.size()) );
     assert(branches.first.back()->previous_id() == branches.second.back()->previous_id());
  }
  std::vector<block_id_type> result;
  for (const item_ptr& fork_block : branches.second)
    result.emplace_back(fork_block->id);
  result.emplace_back(branches.first.back()->previous_id());
  return result;
}

void database::check_transaction_for_duplicated_operations(const signed_transaction& trx)
{
   const auto& proposal_index = get_index<proposal_object>();
   std::set<fc::sha256> existed_operations_digests;

   proposal_index.inspect_all_objects( [&](const object& obj){
      const proposal_object& proposal = static_cast<const proposal_object&>(obj);
      auto proposed_operations_digests = gather_proposed_operations_digests( proposal.proposed_transaction );
      existed_operations_digests.insert( proposed_operations_digests.begin(), proposed_operations_digests.end() );
   });

   for (auto& pending_transaction: _pending_tx)
   {
      auto proposed_operations_digests = gather_proposed_operations_digests(pending_transaction);
      existed_operations_digests.insert(proposed_operations_digests.begin(), proposed_operations_digests.end());
   }

   auto proposed_operations_digests = gather_proposed_operations_digests(trx);
   for (auto& digest: proposed_operations_digests)
   {
      FC_ASSERT(existed_operations_digests.count(digest) == 0, "Proposed operation is already pending for approval.");
   }
}

/**
 * Push block "may fail" in which case every partial change is unwound.  After
 * push block is successful the block is appended to the chain database on disk.
 *
 * @return true if we switched forks as a result of this push.
 */
bool database::push_block(const signed_block& new_block, uint32_t skip)
{
//   idump((new_block.block_num())(new_block.id())(new_block.timestamp)(new_block.previous));
   bool result;
   detail::with_skip_flags( *this, skip, [&]()
   {
      detail::without_pending_transactions( *this, std::move(_pending_tx),
      [&]()
      {
         result = _push_block(new_block);
      });
   });
   return result;
}

bool database::_push_block(const signed_block& new_block)
{ try {
   uint32_t skip = get_node_properties().skip_flags;
   const auto now = fc::time_point::now().sec_since_epoch();

   if( _fork_db.head() && new_block.timestamp.sec_since_epoch() > now - 86400 )
   {
      // verify that the block signer is in the current set of active witnesses.
      shared_ptr<fork_item> prev_block = _fork_db.fetch_block( new_block.previous );
      GRAPHENE_ASSERT( prev_block, unlinkable_block_exception, "block does not link to known chain" );
      if( prev_block->scheduled_witnesses && !(skip&(skip_witness_schedule_check|skip_witness_signature)) )
         verify_signing_witness( new_block, *prev_block );
   }
   shared_ptr<fork_item> new_head = _fork_db.push_block(new_block);

   //If the head block from the longest chain does not build off of the current head, we need to switch forks.
   if( new_head->data.previous != head_block_id() )
   {
      //If the newly pushed block is the same height as head, we get head back in new_head
      //Only switch forks if new_head is actually higher than head
      if( new_head->data.block_num() > head_block_num() )
      {
         wlog( "Switching to fork: ${id}", ("id",new_head->data.id()) );
         auto branches = _fork_db.fetch_branch_from(new_head->data.id(), head_block_id());

         // pop blocks until we hit the forked block
         while( head_block_id() != branches.second.back()->data.previous )
         {
            ilog( "popping block #${n} ${id}", ("n",head_block_num())("id",head_block_id()) );
            pop_block();
         }

         // push all blocks on the new fork
         for( auto ritr = branches.first.rbegin(); ritr != branches.first.rend(); ++ritr )
         {
               ilog( "pushing block from fork #${n} ${id}", ("n",(*ritr)->data.block_num())("id",(*ritr)->id) );
               optional<fc::exception> except;
               try {
                  undo_database::session session = _undo_db.start_undo_session();
                  apply_block( (*ritr)->data, skip );
                  update_witnesses( **ritr );
                  _block_id_to_block.store( (*ritr)->id, (*ritr)->data );
                  session.commit();
               }
               catch ( const fc::exception& e ) { except = e; }
               if( except )
               {
                  wlog( "exception thrown while switching forks ${e}", ("e",except->to_detail_string() ) );
                  // remove the rest of branches.first from the fork_db, those blocks are invalid
                  while( ritr != branches.first.rend() )
                  {
                     ilog( "removing block from fork_db #${n} ${id}", ("n",(*ritr)->data.block_num())("id",(*ritr)->id) );
                     _fork_db.remove( (*ritr)->id );
                     ++ritr;
                  }
                  _fork_db.set_head( branches.second.front() );

                  // pop all blocks from the bad fork
                  while( head_block_id() != branches.second.back()->data.previous )
                  {
                     ilog( "popping block #${n} ${id}", ("n",head_block_num())("id",head_block_id()) );
                     pop_block();
                  }

                  ilog( "Switching back to fork: ${id}", ("id",branches.second.front()->data.id()) );
                  // restore all blocks from the good fork
                  for( auto ritr2 = branches.second.rbegin(); ritr2 != branches.second.rend(); ++ritr2 )
                  {
                     ilog( "pushing block #${n} ${id}", ("n",(*ritr2)->data.block_num())("id",(*ritr2)->id) );
                     auto session = _undo_db.start_undo_session();
                     apply_block( (*ritr2)->data, skip );
                     _block_id_to_block.store( (*ritr2)->id, (*ritr2)->data );
                     session.commit();
                  }
                  throw *except;
               }
         }
         return true;
      }
      else return false;
   }

   try {
      auto session = _undo_db.start_undo_session();
      apply_block(new_block, skip);
      if( new_block.timestamp.sec_since_epoch() > now - 86400 )
         update_witnesses( *new_head );
      _block_id_to_block.store(new_block.id(), new_block);
      session.commit();
   } catch ( const fc::exception& e ) {
      elog("Failed to push new block:\n${e}", ("e", e.to_detail_string()));
      _fork_db.remove(new_block.id());
      throw;
   }

   return false;
} FC_CAPTURE_AND_RETHROW( (new_block) ) }

void database::verify_signing_witness( const signed_block& new_block, const fork_item& fork_entry )const
{
   FC_ASSERT( new_block.timestamp >= fork_entry.next_block_time );
   uint32_t slot_num = ( new_block.timestamp - fork_entry.next_block_time ).to_seconds() / block_interval();
   const global_property_object& gpo = get_global_properties();

   if (gpo.parameters.witness_schedule_algorithm == GRAPHENE_WITNESS_SHUFFLED_ALGORITHM)
   {
      uint64_t index = ( fork_entry.next_block_aslot + slot_num ) % fork_entry.scheduled_witnesses->size();
      const auto& scheduled_witness = (*fork_entry.scheduled_witnesses)[index];
      FC_ASSERT( new_block.witness == scheduled_witness.first, "Witness produced block at wrong time",
               ("block witness",new_block.witness)("scheduled",scheduled_witness)("slot_num",slot_num) );
      FC_ASSERT( new_block.validate_signee( scheduled_witness.second ) );
   }
   if (gpo.parameters.witness_schedule_algorithm == GRAPHENE_WITNESS_SCHEDULED_ALGORITHM &&
       slot_num != 0 )
   {
      witness_id_type wid;
      const witness_schedule_object& wso = get_witness_schedule_object();
      // ask the near scheduler who goes in the given slot
      bool slot_is_near = wso.scheduler.get_slot(slot_num, wid);
      if(! slot_is_near)
      {
         // if the near scheduler doesn't know, we have to extend it to
         //   a far scheduler.
         // n.b. instantiating it is slow, but block gaps long enough to
         //   need it are likely pretty rare.

         witness_scheduler_rng far_rng(wso.rng_seed.begin(), GRAPHENE_FAR_SCHEDULE_CTR_IV);

         far_future_witness_scheduler far_scheduler =
            far_future_witness_scheduler(wso.scheduler, far_rng);
         if(!far_scheduler.get_slot(slot_num, wid))
         {
            // no scheduled witness -- somebody set up us the bomb
            // n.b. this code path is impossible, the present
            // implementation of far_future_witness_scheduler
            // returns true unconditionally
            assert( false );
         }
      }

      FC_ASSERT( new_block.witness == wid, "Witness produced block at wrong time",
               ("block witness",new_block.witness)("scheduled",wid)("slot_num",slot_num) );
      FC_ASSERT( new_block.validate_signee( wid(*this).signing_key ) );
   }
}

void database::update_witnesses( fork_item& fork_entry )const
{
   if( fork_entry.scheduled_witnesses ) return;

   const dynamic_global_property_object& dpo = get_dynamic_global_properties();
   fork_entry.next_block_aslot = dpo.current_aslot + 1;
   fork_entry.next_block_time = get_slot_time( 1 );

   const witness_schedule_object& wso = get_witness_schedule_object();
   fork_entry.scheduled_witnesses = std::make_shared< vector< pair< witness_id_type, public_key_type > > >();
   fork_entry.scheduled_witnesses->reserve( wso.current_shuffled_witnesses.size() );

   for( size_t i = 0; i < wso.current_shuffled_witnesses.size(); ++i )
   {
      const auto& witness = wso.current_shuffled_witnesses[i](*this);
      fork_entry.scheduled_witnesses->emplace_back( wso.current_shuffled_witnesses[i], witness.signing_key );
   }
}

/**
 * Attempts to push the transaction into the pending queue
 *
 * When called to push a locally generated transaction, set the skip_block_size_check bit on the skip argument. This
 * will allow the transaction to be pushed even if it causes the pending block size to exceed the maximum block size.
 * Although the transaction will probably not propagate further now, as the peers are likely to have their pending
 * queues full as well, it will be kept in the queue to be propagated later when a new block flushes out the pending
 * queues.
 */
processed_transaction database::push_transaction( const signed_transaction& trx, uint32_t skip )
{ try {
   processed_transaction result;
   detail::with_skip_flags( *this, skip, [&]()
   {
      result = _push_transaction( trx );
   } );
   return result;
} FC_CAPTURE_AND_RETHROW( (trx) ) }

processed_transaction database::_push_transaction( const signed_transaction& trx )
{
   // If this is the first transaction pushed after applying a block, start a new undo session.
   // This allows us to quickly rewind to the clean state of the head block, in case a new block arrives.
   if( !_pending_tx_session.valid() )
      _pending_tx_session = _undo_db.start_undo_session();

   // Create a temporary undo session as a child of _pending_tx_session.
   // The temporary session will be discarded by the destructor if
   // _apply_transaction fails.  If we make it to merge(), we
   // apply the changes.

   auto temp_session = _undo_db.start_undo_session();
   auto processed_trx = _apply_transaction( trx );
   _pending_tx.push_back(processed_trx);

   // notify_changed_objects();
   // The transaction applied successfully. Merge its changes into the pending block session.
   temp_session.merge();

   // notify anyone listening to pending transactions
   notify_on_pending_transaction( trx );
   return processed_trx;
}

processed_transaction database::validate_transaction( const signed_transaction& trx )
{
   auto session = _undo_db.start_undo_session();
   return _apply_transaction( trx );
}

processed_transaction database::push_proposal(const proposal_object& proposal)
{ try {
   transaction_evaluation_state eval_state(this);
   eval_state._is_proposed_trx = true;

   eval_state.operation_results.reserve(proposal.proposed_transaction.operations.size());
   processed_transaction ptrx(proposal.proposed_transaction);
   eval_state._trx = &ptrx;
   size_t old_applied_ops_size = _applied_ops.size();

   try {
      if( _undo_db.size() >= _undo_db.max_size() )
         _undo_db.set_max_size( _undo_db.size() + 1 );
      auto session = _undo_db.start_undo_session(true);
      for( auto& op : proposal.proposed_transaction.operations )
         eval_state.operation_results.emplace_back(apply_operation(eval_state, op));
      remove_son_proposal(proposal);
      remove(proposal);
      session.merge();
   } catch ( const fc::exception& e ) {
      if( head_block_time() <= HARDFORK_483_TIME )
      {
         for( size_t i=old_applied_ops_size,n=_applied_ops.size(); i<n; i++ )
         {
            if(_applied_ops[i].valid()) {
               ilog("removing failed operation from applied_ops: ${op}", ("op", *(_applied_ops[i])));
            }
            else{
               ilog("Can't remove failed operation from applied_ops (operation is not valid), op_id : ${op_id}", ("op_id", i));
            }
            _applied_ops[i].reset();
         }
      }
      else
      {
         _applied_ops.resize( old_applied_ops_size );
      }
      edump((e));
      throw;
   }

   ptrx.operation_results = std::move(eval_state.operation_results);
   return ptrx;
} FC_CAPTURE_AND_RETHROW() }

signed_block database::generate_block(
   fc::time_point_sec when,
   witness_id_type witness_id,
   const fc::ecc::private_key& block_signing_private_key,
   uint32_t skip /* = 0 */
   )
{ try {
   signed_block result;
   detail::with_skip_flags( *this, skip, [&]()
   {
      result = _generate_block( when, witness_id, block_signing_private_key );
   } );
   return result;
} FC_CAPTURE_AND_RETHROW() }

signed_block database::_generate_block(
   fc::time_point_sec when,
   witness_id_type witness_id,
   const fc::ecc::private_key& block_signing_private_key
   )
{
   try {
   uint32_t skip = get_node_properties().skip_flags;
   uint32_t slot_num = get_slot_at_time( when );
   FC_ASSERT( slot_num > 0 );
   witness_id_type scheduled_witness = get_scheduled_witness( slot_num );
   FC_ASSERT( scheduled_witness == witness_id );

   const auto& witness_obj = witness_id(*this);

   if( !(skip & skip_witness_signature) )
      FC_ASSERT( witness_obj.signing_key == block_signing_private_key.get_public_key() );

   static const size_t max_block_header_size = fc::raw::pack_size( signed_block_header() ) + 4;
   auto maximum_block_size = get_global_properties().parameters.maximum_block_size;
   size_t total_block_size = max_block_header_size;

   signed_block pending_block;

   //
   // The following code throws away existing pending_tx_session and
   // rebuilds it by re-applying pending transactions.
   //
   // This rebuild is necessary because pending transactions' validity
   // and semantics may have changed since they were received, because
   // time-based semantics are evaluated based on the current block
   // time.  These changes can only be reflected in the database when
   // the value of the "when" variable is known, which means we need to
   // re-apply pending transactions in this method.
   //
   _pending_tx_session.reset();
   _pending_tx_session = _undo_db.start_undo_session();

   uint64_t postponed_tx_count = 0;
   // pop pending state (reset to head block state)
   for( const processed_transaction& tx : _pending_tx )
   {
      size_t new_total_size = total_block_size + fc::raw::pack_size( tx );

      // postpone transaction if it would make block too big
      if( new_total_size >= maximum_block_size )
      {
         postponed_tx_count++;
         continue;
      }

      try
      {
         auto temp_session = _undo_db.start_undo_session();
         processed_transaction ptx = _apply_transaction( tx );
         temp_session.merge();

         // We have to recompute pack_size(ptx) because it may be different
         // than pack_size(tx) (i.e. if one or more results increased
         // their size)
         total_block_size += fc::raw::pack_size( ptx );
         pending_block.transactions.push_back( ptx );
      }
      catch ( const fc::exception& e )
      {
         // Do nothing, transaction will not be re-applied
         wlog( "Transaction was not processed while generating block due to ${e}", ("e", e) );
         wlog( "The transaction was ${t}", ("t", tx) );
      }
   }
   if( postponed_tx_count > 0 )
   {
      wlog( "Postponed ${n} transactions due to block size limit", ("n", postponed_tx_count) );
   }

   _pending_tx_session.reset();

   // We have temporarily broken the invariant that
   // _pending_tx_session is the result of applying _pending_tx, as
   // _pending_tx now consists of the set of postponed transactions.
   // However, the push_block() call below will re-create the
   // _pending_tx_session.

   pending_block.previous = head_block_id();
   pending_block.timestamp = when;
   pending_block.transaction_merkle_root = pending_block.calculate_merkle_root();
   pending_block.witness = witness_id;

   // Genesis witnesses start with a default initial secret
   if( witness_obj.next_secret_hash == secret_hash_type::hash( secret_hash_type() ) ) {
         pending_block.previous_secret = secret_hash_type();
   } else {
         secret_hash_type::encoder last_enc;
         fc::raw::pack( last_enc, block_signing_private_key );
         fc::raw::pack( last_enc, witness_obj.previous_secret );
         pending_block.previous_secret = last_enc.result();
   }

   secret_hash_type::encoder next_enc;
   fc::raw::pack( next_enc, block_signing_private_key );
   fc::raw::pack( next_enc, pending_block.previous_secret );
   pending_block.next_secret_hash = secret_hash_type::hash(next_enc.result());

   if( !(skip & skip_witness_signature) )
      pending_block.sign( block_signing_private_key );

   // TODO:  Move this to _push_block() so session is restored.
   if( !(skip & skip_block_size_check) )
   {
      FC_ASSERT( fc::raw::pack_size(pending_block) <= get_global_properties().parameters.maximum_block_size );
   }

   push_block( pending_block, skip | skip_transaction_signatures ); // skip authority check when pushing self-generated blocks

   return pending_block;
} FC_CAPTURE_AND_RETHROW( (witness_id) ) }

/**
 * Removes the most recent block from the database and
 * undoes any changes it made.
 */
void database::pop_block()
{ try {
   _pending_tx_session.reset();
   auto head_id = head_block_id();
   optional<signed_block> head_block = fetch_block_by_id( head_id );
   GRAPHENE_ASSERT( head_block.valid(), pop_empty_chain, "there are no blocks to pop" );

   _fork_db.pop_block();
   pop_undo();

   _popped_tx.insert( _popped_tx.begin(), head_block->transactions.begin(), head_block->transactions.end() );

} FC_CAPTURE_AND_RETHROW() }

void database::clear_pending()
{ try {
   assert( (_pending_tx.size() == 0) || _pending_tx_session.valid() );
   _pending_tx.clear();
   _pending_tx_session.reset();
} FC_CAPTURE_AND_RETHROW() }

uint32_t database::push_applied_operation( const operation& op )
{
   _applied_ops.emplace_back(op);
   operation_history_object& oh = *(_applied_ops.back());
   oh.block_num    = _current_block_num;
   oh.trx_in_block = _current_trx_in_block;
   oh.op_in_trx    = _current_op_in_trx;
   oh.virtual_op   = _current_virtual_op++;
   return _applied_ops.size() - 1;
}
void database::set_applied_operation_result( uint32_t op_id, const operation_result& result )
{
   assert( op_id < _applied_ops.size() );
   if( _applied_ops[op_id].valid() )
      _applied_ops[op_id]->result = result;
   else
   {
      elog( "Could not set operation result (head_block_num=${b})", ("b", head_block_num()) );
   }
}

const vector<optional< operation_history_object > >& database::get_applied_operations() const
{
   return _applied_ops;
}

vector<optional< operation_history_object > >& database::get_applied_operations()
{
   return _applied_ops;
}
//////////////////// private methods ////////////////////

void database::apply_block( const signed_block& next_block, uint32_t skip )
{
   auto block_num = next_block.block_num();
   if( _checkpoints.size() && _checkpoints.rbegin()->second != block_id_type() )
   {
      auto itr = _checkpoints.find( block_num );
      if( itr != _checkpoints.end() )
         FC_ASSERT( next_block.id() == itr->second, "Block did not match checkpoint", ("checkpoint",*itr)("block_id",next_block.id()) );

      if( _checkpoints.rbegin()->first >= block_num )
         skip = ~0;// WE CAN SKIP ALMOST EVERYTHING
   }

   detail::with_skip_flags( *this, skip, [&]()
   {
      _apply_block( next_block );
   } );
   return;
}

void database::_apply_block( const signed_block& next_block )
{ try {
   uint32_t next_block_num = next_block.block_num();
   uint32_t skip = get_node_properties().skip_flags;
   _applied_ops.clear();

   FC_ASSERT( (skip & skip_merkle_check) || next_block.transaction_merkle_root == next_block.calculate_merkle_root(), "", ("next_block.transaction_merkle_root",next_block.transaction_merkle_root)("calc",next_block.calculate_merkle_root())("next_block",next_block)("id",next_block.id()) );

   const witness_object& signing_witness = validate_block_header(skip, next_block);
   const auto& global_props = get_global_properties();
   const auto& dynamic_global_props = get_dynamic_global_properties();
   bool maint_needed = (dynamic_global_props.next_maintenance_time <= next_block.timestamp);

   _current_block_num    = next_block_num;
   _current_trx_in_block = 0;
   _current_op_in_trx    = 0;
   _current_virtual_op   = 0;

   _issue_453_affected_assets.clear();

   for( const auto& trx : next_block.transactions )
   {
      /* We do not need to push the undo state for each transaction
       * because they either all apply and are valid or the
       * entire block fails to apply.  We only need an "undo" state
       * for transactions when validating broadcast transactions or
       * when building a block.
       */

      apply_transaction( trx, skip );
      // For real operations which are explicitly included in a transaction, virtual_op is 0.
      // For VOPs derived directly from a real op,
      //     use the real op's (block_num,trx_in_block,op_in_trx), virtual_op starts from 1.
      // For VOPs created after processed all transactions,
      //     trx_in_block = the_block.trsanctions.size(), virtual_op starts from 0.
      ++_current_trx_in_block;
      _current_op_in_trx  = 0;
      _current_virtual_op = 0;
   }

   if (global_props.parameters.witness_schedule_algorithm == GRAPHENE_WITNESS_SCHEDULED_ALGORITHM) {
      update_witness_schedule(next_block);
      if(global_props.active_sons.size() > 0) {
         update_son_schedule(next_block);
      }
   }

   const uint32_t missed = update_witness_missed_blocks( next_block );
   update_global_dynamic_data( next_block, missed );
   update_signing_witness(signing_witness, next_block);
   update_last_irreversible_block();

   // Are we at the maintenance interval?
   if( maint_needed )
      perform_chain_maintenance(next_block, global_props);

   check_ending_lotteries();
   check_ending_nft_lotteries();
   
   create_block_summary(next_block);
   place_delayed_bets(); // must happen after update_global_dynamic_data() updates the time
   clear_expired_transactions();
   clear_expired_proposals();
   clear_expired_orders();
   update_expired_feeds();       // this will update expired feeds and some core exchange rates
   update_core_exchange_rates(); // this will update remaining core exchange rates
   update_withdraw_permissions();
   update_tournaments();
   update_betting_markets(next_block.timestamp);
   finalize_expired_offers();

   // n.b., update_maintenance_flag() happens this late
   // because get_slot_time() / get_slot_at_time() is needed above
   // TODO:  figure out if we could collapse this function into
   // update_global_dynamic_data() as perhaps these methods only need
   // to be called for header validation?
   update_maintenance_flag( maint_needed );
   if (global_props.parameters.witness_schedule_algorithm == GRAPHENE_WITNESS_SHUFFLED_ALGORITHM) {
      update_witness_schedule();
      if(global_props.active_sons.size() > 0) {
         update_son_schedule();
      }
   }

   if( !_node_property_object.debug_updates.empty() )
      apply_debug_updates();

   // notify observers that the block has been applied
   notify_applied_block( next_block ); //emit
   _applied_ops.clear();

   notify_changed_objects();
} FC_CAPTURE_AND_RETHROW( (next_block.block_num()) )  }



processed_transaction database::apply_transaction(const signed_transaction& trx, uint32_t skip)
{
   processed_transaction result;
   detail::with_skip_flags( *this, skip, [&]()
   {
      result = _apply_transaction(trx);
   });
   return result;
}

class undo_size_restorer {
   public:
      undo_size_restorer( undo_database& db ) : _db( db ), old_max( db.max_size() ) {
         _db.set_max_size( old_max * 2 );
      }
      ~undo_size_restorer() {
         _db.set_max_size( old_max );
      }
   private:
      undo_database& _db;
      size_t         old_max;
};

processed_transaction database::_apply_transaction(const signed_transaction& trx)
{ try {
   uint32_t skip = get_node_properties().skip_flags;

   if( true || !(skip&skip_validate) )   /* issue #505 explains why this skip_flag is disabled */
      trx.validate();

   auto& trx_idx = get_mutable_index_type<transaction_index>();
   const chain_id_type& chain_id = get_chain_id();
   transaction_id_type trx_id;

   if( !(skip & skip_transaction_dupe_check) )
   {
      trx_id = trx.id();
      FC_ASSERT( trx_idx.indices().get<by_trx_id>().find(trx_id) == trx_idx.indices().get<by_trx_id>().end() );
   }

   transaction_evaluation_state eval_state(this);
   const chain_parameters& chain_parameters = get_global_properties().parameters;
   eval_state._trx = &trx;

   if( !(skip & (skip_transaction_signatures | skip_authority_check) ) )
   {
      auto get_active = [this]( account_id_type id ) { return &id(*this).active; };
      auto get_owner  = [this]( account_id_type id ) { return &id(*this).owner;  };
      auto get_custom = [this]( account_id_type id, const operation& op ) {
         return get_account_custom_authorities(id, op);
      };
      trx.verify_authority( chain_id, get_active, get_owner, get_custom,
                            true,
                            get_global_properties().parameters.max_authority_depth );
   }

   //Skip all manner of expiration and TaPoS checking if we're on block 1; It's impossible that the transaction is
   //expired, and TaPoS makes no sense as no blocks exist.
   if( BOOST_LIKELY(head_block_num() > 0) )
   {
      if( !(skip & skip_tapos_check) )
      {
         const auto& tapos_block_summary = block_summary_id_type( trx.ref_block_num )(*this);

         //Verify TaPoS block summary has correct ID prefix, and that this block's time is not past the expiration
         FC_ASSERT( trx.ref_block_prefix == tapos_block_summary.block_id._hash[1] );
      }

      fc::time_point_sec now = head_block_time();

      FC_ASSERT( trx.expiration <= now + chain_parameters.maximum_time_until_expiration, "",
                 ("trx.expiration",trx.expiration)("now",now)("max_til_exp",chain_parameters.maximum_time_until_expiration));
      FC_ASSERT( now <= trx.expiration, "", ("now",now)("trx.exp",trx.expiration) );
   }

   //Insert transaction into unique transactions database.
   if( !(skip & skip_transaction_dupe_check) )
   {
      create<transaction_object>([&trx_id,&trx](transaction_object& transaction) {
         transaction.trx_id = trx_id;
         transaction.trx = trx;
      });
   }

   eval_state.operation_results.reserve(trx.operations.size());

   const undo_size_restorer undo_guard( _undo_db );
   //Finally process the operations
   processed_transaction ptrx(trx);
   _current_op_in_trx = 0;
   _current_virtual_op = 0;
   for( const auto& op : ptrx.operations )
   {
      _current_virtual_op = 0;
      eval_state.operation_results.emplace_back(apply_operation(eval_state, op));
      ++_current_op_in_trx;
   }
   ptrx.operation_results = std::move(eval_state.operation_results);

   //Make sure the temp account has no non-zero balances
   const auto& balances = get_index_type< primary_index< account_balance_index > >().get_secondary_index< balances_by_account_index >().get_account_balances( GRAPHENE_TEMP_ACCOUNT );
   for( const auto b : balances )
      FC_ASSERT(b.second->balance == 0);

   return ptrx;
} FC_CAPTURE_AND_RETHROW( (trx) ) }

operation_result database::apply_operation(transaction_evaluation_state& eval_state, const operation& op)
{ try {
   int i_which = op.which();
   uint64_t u_which = uint64_t( i_which );
   FC_ASSERT( i_which >= 0, "Negative operation tag in operation ${op}", ("op",op) );
   FC_ASSERT( u_which < _operation_evaluators.size(), "No registered evaluator for operation ${op}", ("op",op) );
   unique_ptr<op_evaluator>& eval = _operation_evaluators[ u_which ];
   FC_ASSERT( eval, "No registered evaluator for operation ${op}", ("op",op) );
   auto op_id = push_applied_operation( op );
   auto result = eval->evaluate( eval_state, op, true );
   set_applied_operation_result( op_id, result );
   return result;
} FC_CAPTURE_AND_RETHROW( (op) ) }

const witness_object& database::validate_block_header( uint32_t skip, const signed_block& next_block )const
{
   FC_ASSERT( head_block_id() == next_block.previous, "", ("head_block_id",head_block_id())("next.prev",next_block.previous) );
   FC_ASSERT( head_block_time() < next_block.timestamp, "", ("head_block_time",head_block_time())("next",next_block.timestamp)("blocknum",next_block.block_num()) );
   const witness_object& witness = next_block.witness(*this);
//DLN: TODO: Temporarily commented out to test shuffle vs RNG scheduling algorithm for witnesses, this was causing shuffle agorithm to fail during create_witness test. This should be re-enabled for RNG, and maybe for shuffle too, don't really know for sure.
   if( next_block.timestamp > HARDFORK_SWEEPS_TIME )
      FC_ASSERT( secret_hash_type::hash( next_block.previous_secret ) == witness.next_secret_hash, "",
               ( "previous_secret", next_block.previous_secret )( "next_secret_hash", witness.next_secret_hash ) );

   if( !(skip&skip_witness_signature) )
      FC_ASSERT( next_block.validate_signee( witness.signing_key ) );

   if( !(skip&skip_witness_schedule_check) )
   {
      uint32_t slot_num = get_slot_at_time( next_block.timestamp );
      FC_ASSERT( slot_num > 0 );

      witness_id_type scheduled_witness = get_scheduled_witness( slot_num );

      FC_ASSERT( next_block.witness == scheduled_witness, "Witness produced block at wrong time",
                 ("block witness",next_block.witness)("scheduled",scheduled_witness)("slot_num",slot_num) );
   }

   return witness;
}

void database::create_block_summary(const signed_block& next_block)
{
   block_summary_id_type sid(next_block.block_num() & 0xffff );
   modify( sid(*this), [&](block_summary_object& p) {
         p.block_id = next_block.id();
   });
}

void database::add_checkpoints( const flat_map<uint32_t,block_id_type>& checkpts )
{
   for( const auto& i : checkpts )
      _checkpoints[i.first] = i.second;
}

bool database::before_last_checkpoint()const
{
   return (_checkpoints.size() > 0) && (_checkpoints.rbegin()->first >= head_block_num());
}

} }
