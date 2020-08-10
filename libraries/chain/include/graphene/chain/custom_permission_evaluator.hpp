#pragma once
#include <graphene/chain/evaluator.hpp>
#include <graphene/chain/protocol/custom_permission.hpp>

namespace graphene
{
namespace chain
{

class create_custom_permission_evaluator : public evaluator<create_custom_permission_evaluator>
{
public:
   typedef custom_permission_create_operation operation_type;

   void_result do_evaluate(const custom_permission_create_operation &o);
   object_id_type do_apply(const custom_permission_create_operation &o);
};

class update_custom_permission_evaluator : public evaluator<update_custom_permission_evaluator>
{
public:
   typedef custom_permission_update_operation operation_type;

   void_result do_evaluate(const custom_permission_update_operation &o);
   object_id_type do_apply(const custom_permission_update_operation &o);
};

class delete_custom_permission_evaluator : public evaluator<delete_custom_permission_evaluator>
{
public:
   typedef custom_permission_delete_operation operation_type;

   void_result do_evaluate(const custom_permission_delete_operation &o);
   void_result do_apply(const custom_permission_delete_operation &o);
};

} // namespace chain
} // namespace graphene