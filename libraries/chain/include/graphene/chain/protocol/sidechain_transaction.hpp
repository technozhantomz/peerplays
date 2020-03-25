#pragma once
#include <graphene/chain/protocol/base.hpp>
#include <graphene/chain/protocol/types.hpp>
#include <graphene/chain/sidechain_defs.hpp>

namespace graphene { namespace chain {

   struct sidechain_transaction_create_operation : public base_operation
   {
      struct fee_parameters_type { uint64_t fee = 0; };

      asset fee;
      account_id_type payer;

      sidechain_type sidechain;
      object_id_type object_id;
      std::string transaction;
      std::vector<son_id_type> signers;

      account_id_type fee_payer()const { return payer; }
      share_type      calculate_fee(const fee_parameters_type& k)const { return 0; }
   };

   struct sidechain_transaction_sign_operation : public base_operation
   {
      struct fee_parameters_type { uint64_t fee = 0; };

      asset fee;
      account_id_type payer;

      sidechain_transaction_id_type sidechain_transaction_id;
      std::string transaction;
      block_id_type block;
      bool complete;

      account_id_type   fee_payer()const { return payer; }
      share_type        calculate_fee( const fee_parameters_type& k )const { return 0; }
   };

   struct sidechain_transaction_send_operation : public base_operation
   {
      struct fee_parameters_type { uint64_t fee = 0; };

      asset fee;
      account_id_type payer;

      sidechain_transaction_id_type sidechain_transaction_id;

      account_id_type   fee_payer()const { return payer; }
      share_type        calculate_fee( const fee_parameters_type& k )const { return 0; }
   };

} } // graphene::chain

FC_REFLECT( graphene::chain::sidechain_transaction_create_operation::fee_parameters_type, (fee) )
FC_REFLECT( graphene::chain::sidechain_transaction_create_operation, (fee)(payer)
        (sidechain)
        (object_id)
        (transaction)
        (signers) )

FC_REFLECT( graphene::chain::sidechain_transaction_sign_operation::fee_parameters_type, (fee) )
FC_REFLECT( graphene::chain::sidechain_transaction_sign_operation, (fee)(payer)
        (sidechain_transaction_id)
        (transaction)
        (block)
        (complete) )

FC_REFLECT( graphene::chain::sidechain_transaction_send_operation::fee_parameters_type, (fee) )
FC_REFLECT( graphene::chain::sidechain_transaction_send_operation, (fee)(payer)
        (sidechain_transaction_id) )