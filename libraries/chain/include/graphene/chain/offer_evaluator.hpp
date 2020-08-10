#include <graphene/chain/protocol/operations.hpp>
#include <graphene/chain/evaluator.hpp>
#include <graphene/chain/database.hpp>

namespace graphene
{
   namespace chain
   {

      class offer_evaluator : public evaluator<offer_evaluator>
      {
      public:
         typedef offer_operation operation_type;

         void_result do_evaluate(const offer_operation &o);
         object_id_type do_apply(const offer_operation &o);
      };

      class bid_evaluator : public evaluator<bid_evaluator>
      {
      public:
         typedef bid_operation operation_type;

         void_result do_evaluate(const bid_operation &o);
         void_result do_apply(const bid_operation &o);
      };

      class cancel_offer_evaluator : public evaluator<cancel_offer_evaluator>
      {
      public:
         typedef cancel_offer_operation operation_type;

         void_result do_evaluate(const cancel_offer_operation &o);
         void_result do_apply(const cancel_offer_operation &o);
      };

      class finalize_offer_evaluator : public evaluator<finalize_offer_evaluator>
      {
      public:
         typedef finalize_offer_operation operation_type;

         void_result do_evaluate(const finalize_offer_operation &op);
         void_result do_apply(const finalize_offer_operation &op);
      };

   } // namespace chain
} // namespace graphene
