#pragma once
#include <graphene/chain/database.hpp>
#include <graphene/chain/evaluator.hpp>
#include <graphene/chain/protocol/operations.hpp>
#include <graphene/chain/protocol/types.hpp>

namespace graphene { namespace chain {

   class random_number_store_evaluator : public evaluator<random_number_store_evaluator>
   {
      public:
         typedef random_number_store_operation operation_type;

         void_result do_evaluate( const random_number_store_operation& o );
         object_id_type do_apply( const random_number_store_operation& o );
   };

} } // graphene::chain

