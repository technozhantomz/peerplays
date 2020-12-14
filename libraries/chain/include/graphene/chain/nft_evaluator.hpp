#pragma once

#include <graphene/chain/database.hpp>
#include <graphene/chain/evaluator.hpp>
#include <graphene/chain/protocol/operations.hpp>
#include <graphene/chain/protocol/types.hpp>

namespace graphene { namespace chain {

   class nft_metadata_create_evaluator : public evaluator<nft_metadata_create_evaluator>
   {
      public:
         typedef nft_metadata_create_operation operation_type;
         void_result do_evaluate( const nft_metadata_create_operation& o );
         object_id_type do_apply( const nft_metadata_create_operation& o );
   };

   class nft_metadata_update_evaluator : public evaluator<nft_metadata_update_evaluator>
   {
      public:
         typedef nft_metadata_update_operation operation_type;
         void_result do_evaluate( const nft_metadata_update_operation& o );
         void_result do_apply( const nft_metadata_update_operation& o );
   };

   class nft_mint_evaluator : public evaluator<nft_mint_evaluator>
   {
      public:
         typedef nft_mint_operation operation_type;
         void_result do_evaluate( const nft_mint_operation& o );
         object_id_type do_apply( const nft_mint_operation& o );
   };

   class nft_safe_transfer_from_evaluator : public evaluator<nft_safe_transfer_from_evaluator>
   {
      public:
         typedef nft_safe_transfer_from_operation operation_type;
         void_result do_evaluate( const nft_safe_transfer_from_operation& o );
         object_id_type do_apply( const nft_safe_transfer_from_operation& o );
   };

   class nft_approve_evaluator : public evaluator<nft_approve_evaluator>
   {
      public:
         typedef nft_approve_operation operation_type;
         void_result do_evaluate( const nft_approve_operation& o );
         object_id_type do_apply( const nft_approve_operation& o );
   };

   class nft_set_approval_for_all_evaluator : public evaluator<nft_set_approval_for_all_evaluator>
   {
      public:
         typedef nft_set_approval_for_all_operation operation_type;
         void_result do_evaluate( const nft_set_approval_for_all_operation& o );
         void_result do_apply( const nft_set_approval_for_all_operation& o );
   };

} } // graphene::chain

