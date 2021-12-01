#include <graphene/chain/random_number_evaluator.hpp>
#include <graphene/chain/random_number_object.hpp>

namespace graphene { namespace chain {

void_result random_number_store_evaluator::do_evaluate( const random_number_store_operation& op )
{ try {

   return void_result();
} FC_CAPTURE_AND_RETHROW( (op) ) }

object_id_type random_number_store_evaluator::do_apply( const random_number_store_operation& op )
{ try {
   const auto& new_random_number_object = db().create<random_number_object>( [&]( random_number_object& obj ) {
         obj.account        = op.account;
         obj.timestamp      = db().head_block_time();
         obj.random_number  = op.random_number;
         obj.data           = op.data;
   });
   return new_random_number_object.id;
} FC_CAPTURE_AND_RETHROW( (op) ) }

} } // graphene::chain

