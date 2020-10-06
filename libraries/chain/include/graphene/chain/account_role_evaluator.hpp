#pragma once

#include <graphene/chain/database.hpp>
#include <graphene/chain/evaluator.hpp>
#include <graphene/chain/protocol/operations.hpp>
#include <graphene/chain/protocol/types.hpp>

namespace graphene { namespace chain {

   class account_role_create_evaluator : public evaluator<account_role_create_evaluator>
   {
      public:
         typedef account_role_create_operation operation_type;
         void_result do_evaluate( const account_role_create_operation& o );
         object_id_type do_apply( const account_role_create_operation& o );
   };

   class account_role_update_evaluator : public evaluator<account_role_update_evaluator>
   {
      public:
         typedef account_role_update_operation operation_type;
         void_result do_evaluate( const account_role_update_operation& o );
         void_result do_apply( const account_role_update_operation& o );
   };

   class account_role_delete_evaluator : public evaluator<account_role_delete_evaluator>
   {
      public:
         typedef account_role_delete_operation operation_type;
         void_result do_evaluate( const account_role_delete_operation& o );
         void_result do_apply( const account_role_delete_operation& o );
   };

} } // graphene::chain

