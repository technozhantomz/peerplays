#include <graphene/chain/custom_account_authority_evaluator.hpp>

#include <graphene/chain/database.hpp>
#include <graphene/chain/custom_account_authority_object.hpp>
#include <graphene/chain/custom_permission_object.hpp>
#include <graphene/chain/hardfork.hpp>

namespace graphene
{
namespace chain
{

struct rbac_operation_hardfork_visitor
{
   typedef void result_type;
   const fc::time_point_sec block_time;

   rbac_operation_hardfork_visitor(const fc::time_point_sec bt) : block_time(bt) {}
   void operator()(int op_type) const
   {
      int first_allowed_op = operation::tag<custom_permission_create_operation>::value;
      switch (op_type)
      {
      case operation::tag<custom_permission_create_operation>::value:
      case operation::tag<custom_permission_update_operation>::value:
      case operation::tag<custom_permission_delete_operation>::value:
      case operation::tag<custom_account_authority_create_operation>::value:
      case operation::tag<custom_account_authority_update_operation>::value:
      case operation::tag<custom_account_authority_delete_operation>::value:
         FC_ASSERT(block_time >= HARDFORK_NFT_TIME, "Custom permission not allowed on this operation yet!");
         break;
      default:
         FC_ASSERT(op_type < first_allowed_op, "Custom permission not allowed on this operation!");
      }
   }
};

void_result create_custom_account_authority_evaluator::do_evaluate(const custom_account_authority_create_operation &op)
{
   try
   {
      const database &d = db();
      auto now = d.head_block_time();
      FC_ASSERT(now >= HARDFORK_NFT_TIME, "Not allowed until NFT HF");
      op.owner_account(d);
      const custom_permission_object &pobj = op.permission_id(d);
      FC_ASSERT(pobj.account == op.owner_account, "Only owner account can update account authority object");
      FC_ASSERT(op.valid_to > now, "valid_to expiry should be in future");
      FC_ASSERT((op.valid_to - op.valid_from) <= fc::seconds(d.get_global_properties().parameters.rbac_max_account_authority_lifetime()), "Validity of the auth beyond max expiry");
      rbac_operation_hardfork_visitor rvtor(now);
      rvtor(op.operation_type);
      const auto& cindex = d.get_index_type<custom_account_authority_index>().indices().get<by_permission_and_op>();
      auto count = cindex.count(boost::make_tuple(op.permission_id));
      FC_ASSERT(count < d.get_global_properties().parameters.rbac_max_authorities_per_permission(), "Max operations that can be linked to a permission reached");
      return void_result();
   }
   FC_CAPTURE_AND_RETHROW((op))
}

object_id_type create_custom_account_authority_evaluator::do_apply(const custom_account_authority_create_operation &op)
{
   try
   {
      database &d = db();
      return d.create<custom_account_authority_object>([&op](custom_account_authority_object &obj) mutable {
                 obj.permission_id = op.permission_id;
                 obj.operation_type = op.operation_type;
                 obj.valid_from = op.valid_from;
                 obj.valid_to = op.valid_to;
              })
          .id;
   }
   FC_CAPTURE_AND_RETHROW((op))
}

void_result update_custom_account_authority_evaluator::do_evaluate(const custom_account_authority_update_operation &op)
{
   try
   {
      const database &d = db();
      auto now = d.head_block_time();
      FC_ASSERT(now >= HARDFORK_NFT_TIME, "Not allowed until NFT HF");
      op.owner_account(d);
      const custom_account_authority_object &aobj = op.auth_id(d);
      const custom_permission_object &pobj = aobj.permission_id(d);
      FC_ASSERT(pobj.account == op.owner_account, "Only owner account can update account authority object");
      auto valid_from = aobj.valid_from;
      auto valid_to = aobj.valid_to;
      if (op.new_valid_from)
      {
         valid_from = *op.new_valid_from;
      }

      if (op.new_valid_to)
      {
         FC_ASSERT(*op.new_valid_to > now, "New valid_to expiry should be in the future");
         valid_to = *op.new_valid_to;
      }
      FC_ASSERT(valid_from < valid_to, "valid_from should be before valid_to");
      FC_ASSERT((valid_to - valid_from) <= fc::seconds(d.get_global_properties().parameters.rbac_max_account_authority_lifetime()), "Validity of the auth beyond max expiry");
      return void_result();
   }
   FC_CAPTURE_AND_RETHROW((op))
}

object_id_type update_custom_account_authority_evaluator::do_apply(const custom_account_authority_update_operation &op)
{
   try
   {
      database &d = db();
      const custom_account_authority_object &aobj = op.auth_id(d);
      d.modify(aobj, [&op](custom_account_authority_object &obj) {
         if (op.new_valid_from)
            obj.valid_from = *op.new_valid_from;
         if (op.new_valid_to)
            obj.valid_to = *op.new_valid_to;
      });
      return op.auth_id;
   }
   FC_CAPTURE_AND_RETHROW((op))
}

void_result delete_custom_account_authority_evaluator::do_evaluate(const custom_account_authority_delete_operation &op)
{
   try
   {
      const database &d = db();
      auto now = d.head_block_time();
      FC_ASSERT(now >= HARDFORK_NFT_TIME, "Not allowed until NFT HF");
      op.owner_account(d);
      const custom_account_authority_object &aobj = op.auth_id(d);
      const custom_permission_object &pobj = aobj.permission_id(d);
      FC_ASSERT(pobj.account == op.owner_account, "Only owner account can delete account authority object");
      return void_result();
   }
   FC_CAPTURE_AND_RETHROW((op))
}

void_result delete_custom_account_authority_evaluator::do_apply(const custom_account_authority_delete_operation &op)
{
   try
   {
      database &d = db();
      const custom_account_authority_object &aobj = op.auth_id(d);
      d.remove(aobj);
      return void_result();
   }
   FC_CAPTURE_AND_RETHROW((op))
}

} // namespace chain
} // namespace graphene