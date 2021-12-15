#pragma once

namespace graphene { namespace chain {

   struct random_number_store_operation : public base_operation
   {
      struct fee_parameters_type { uint64_t fee = 5000 * GRAPHENE_BLOCKCHAIN_PRECISION; };

      asset             fee;

      account_id_type account;
      vector<uint64_t> random_number;
      std::string data;

      account_id_type fee_payer()const { return account; }
   };

} } // graphene::chain

FC_REFLECT( graphene::chain::random_number_store_operation::fee_parameters_type, (fee) )
FC_REFLECT( graphene::chain::random_number_store_operation, (fee)
             (account)
             (random_number)
             (data) )

