#pragma once
#include <graphene/chain/evaluator.hpp>
#include <graphene/chain/protocol/custom_account_authority.hpp>

namespace graphene
{
namespace chain
{

class create_custom_account_authority_evaluator : public evaluator<create_custom_account_authority_evaluator>
{
public:
   typedef custom_account_authority_create_operation operation_type;

   void_result do_evaluate(const custom_account_authority_create_operation &o);
   object_id_type do_apply(const custom_account_authority_create_operation &o);
};

class update_custom_account_authority_evaluator : public evaluator<update_custom_account_authority_evaluator>
{
public:
   typedef custom_account_authority_update_operation operation_type;

   void_result do_evaluate(const custom_account_authority_update_operation &o);
   object_id_type do_apply(const custom_account_authority_update_operation &o);
};

class delete_custom_account_authority_evaluator : public evaluator<delete_custom_account_authority_evaluator>
{
public:
   typedef custom_account_authority_delete_operation operation_type;

   void_result do_evaluate(const custom_account_authority_delete_operation &o);
   void_result do_apply(const custom_account_authority_delete_operation &o);
};

} // namespace chain
} // namespace graphene