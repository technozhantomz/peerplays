#pragma once
#include <graphene/chain/protocol/base.hpp>
#include <graphene/chain/protocol/types.hpp>
#include <graphene/peerplays_sidechain/defs.hpp>

namespace graphene { namespace chain {

   struct bitcoin_transaction_send_operation : public base_operation
   {
      struct fee_parameters_type { uint64_t fee = 0; };

      asset                                          fee;
      account_id_type                                payer;

      // TODO: BTC Transaction Structs go here
      fc::flat_map<son_id_type, std::vector<peerplays_sidechain::bytes>> signatures;

      account_id_type fee_payer()const { return payer; }
      void            validate()const {}
      share_type      calculate_fee(const fee_parameters_type& k)const { return 0; }
   };

   struct bitcoin_transaction_sign_operation : public base_operation
   {
      struct fee_parameters_type { uint64_t fee = 0; };

      asset                         fee;
      account_id_type               payer;
      proposal_id_type              proposal_id;
      std::vector<peerplays_sidechain::bytes> signatures;

      account_id_type   fee_payer()const { return payer; }
      void              validate()const {}
      share_type        calculate_fee( const fee_parameters_type& k )const { return 0; }
   };

   struct bitcoin_send_transaction_process_operation : public base_operation
   {
      struct fee_parameters_type { uint64_t fee = 0; };

      asset                         fee;
      account_id_type               payer;

      bitcoin_transaction_id_type   bitcoin_transaction_id;

      account_id_type   fee_payer()const { return payer; }
      void              validate()const {}
      share_type        calculate_fee( const fee_parameters_type& k )const { return 0; }
   };

} } // graphene::chain

FC_REFLECT( graphene::chain::bitcoin_transaction_send_operation::fee_parameters_type, (fee) )
FC_REFLECT( graphene::chain::bitcoin_transaction_send_operation, (fee)(payer)(signatures) )

FC_REFLECT( graphene::chain::bitcoin_transaction_sign_operation::fee_parameters_type, (fee) )
FC_REFLECT( graphene::chain::bitcoin_transaction_sign_operation, (fee)(payer)(proposal_id)(signatures) )

FC_REFLECT( graphene::chain::bitcoin_send_transaction_process_operation::fee_parameters_type, (fee) )
FC_REFLECT( graphene::chain::bitcoin_send_transaction_process_operation, (fee)(payer)(bitcoin_transaction_id) )