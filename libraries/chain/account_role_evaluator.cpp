#include <graphene/chain/account_role_evaluator.hpp>

#include <graphene/chain/database.hpp>
#include <graphene/chain/account_role_object.hpp>
#include <graphene/chain/hardfork.hpp>
#include <graphene/chain/rbac_hardfork_visitor.hpp>

namespace graphene
{
    namespace chain
    {

        void_result account_role_create_evaluator::do_evaluate(const account_role_create_operation &op)
        {
            try
            {
                const database &d = db();
                auto now = d.head_block_time();
                FC_ASSERT(now >= HARDFORK_NFT_TIME, "Not allowed until NFT HF");
                op.owner(d);

                rbac_operation_hardfork_visitor arvtor(now);
                for (const auto &op_type : op.allowed_operations)
                {
                    arvtor(op_type);
                }

                for (const auto &acc : op.whitelisted_accounts)
                {
                    acc(d);
                }

                FC_ASSERT(op.valid_to > now, "valid_to expiry should be in future");
                FC_ASSERT((op.valid_to - now) <= fc::seconds(d.get_global_properties().parameters.account_roles_max_lifetime()), "Validity of the account role beyond max expiry");

                const auto &ar_idx = d.get_index_type<account_role_index>().indices().get<by_owner>();
                auto aro_range = ar_idx.equal_range(op.owner);
                FC_ASSERT(std::distance(aro_range.first, aro_range.second) < d.get_global_properties().parameters.account_roles_max_per_account(), "Max account roles that can be created by one owner is reached");
                return void_result();
            }
            FC_CAPTURE_AND_RETHROW((op))
        }

        object_id_type account_role_create_evaluator::do_apply(const account_role_create_operation &op)
        {
            try
            {
                database &d = db();
                return d.create<account_role_object>([&op](account_role_object &obj) mutable {
                            obj.owner = op.owner;
                            obj.name = op.name;
                            obj.metadata = op.metadata;
                            obj.allowed_operations = op.allowed_operations;
                            obj.whitelisted_accounts = op.whitelisted_accounts;
                            obj.valid_to = op.valid_to;
                        })
                    .id;
            }
            FC_CAPTURE_AND_RETHROW((op))
        }

        void_result account_role_update_evaluator::do_evaluate(const account_role_update_operation &op)
        {
            try
            {
                const database &d = db();
                auto now = d.head_block_time();
                FC_ASSERT(now >= HARDFORK_NFT_TIME, "Not allowed until NFT HF");
                op.owner(d);
                const account_role_object &aobj = op.account_role_id(d);
                FC_ASSERT(aobj.owner == op.owner, "Only owner account can update account role object");

                for (const auto &op_type : op.allowed_operations_to_remove)
                {
                    FC_ASSERT(aobj.allowed_operations.find(op_type) != aobj.allowed_operations.end(),
                              "Cannot remove non existent operation");
                }

                for (const auto &acc : op.accounts_to_remove)
                {
                    FC_ASSERT(aobj.whitelisted_accounts.find(acc) != aobj.whitelisted_accounts.end(),
                              "Cannot remove non existent account");
                }

                rbac_operation_hardfork_visitor arvtor(now);
                for (const auto &op_type : op.allowed_operations_to_add)
                {
                    arvtor(op_type);
                }
                FC_ASSERT((aobj.allowed_operations.size() + op.allowed_operations_to_add.size() - op.allowed_operations_to_remove.size()) > 0, "Allowed operations should be positive");

                for (const auto &acc : op.accounts_to_add)
                {
                    acc(d);
                }
                FC_ASSERT((aobj.whitelisted_accounts.size() + op.accounts_to_add.size() - op.accounts_to_remove.size()) > 0, "Accounts should be positive");

                if (op.valid_to)
                {
                    FC_ASSERT(*op.valid_to > now, "valid_to expiry should be in future");
                    FC_ASSERT((*op.valid_to - now) <= fc::seconds(d.get_global_properties().parameters.account_roles_max_lifetime()), "Validity of the account role beyond max expiry");
                }

                return void_result();
            }
            FC_CAPTURE_AND_RETHROW((op))
        }

        void_result account_role_update_evaluator::do_apply(const account_role_update_operation &op)
        {
            try
            {
                database &d = db();
                const account_role_object &aobj = op.account_role_id(d);
                d.modify(aobj, [&op](account_role_object &obj) {
                    if (op.name)
                        obj.name = *op.name;
                    if (op.metadata)
                        obj.metadata = *op.metadata;
                    obj.allowed_operations.insert(op.allowed_operations_to_add.begin(), op.allowed_operations_to_add.end());
                    obj.whitelisted_accounts.insert(op.accounts_to_add.begin(), op.accounts_to_add.end());
                    for (const auto &op_type : op.allowed_operations_to_remove)
                        obj.allowed_operations.erase(op_type);
                    for (const auto &acc : op.accounts_to_remove)
                        obj.whitelisted_accounts.erase(acc);
                    if (op.valid_to)
                        obj.valid_to = *op.valid_to;
                });
                return void_result();
            }
            FC_CAPTURE_AND_RETHROW((op))
        }

        void_result account_role_delete_evaluator::do_evaluate(const account_role_delete_operation &op)
        {
            try
            {
                const database &d = db();
                auto now = d.head_block_time();
                FC_ASSERT(now >= HARDFORK_NFT_TIME, "Not allowed until NFT HF");
                op.owner(d);
                const account_role_object &aobj = op.account_role_id(d);
                FC_ASSERT(aobj.owner == op.owner, "Only owner account can delete account role object");
                return void_result();
            }
            FC_CAPTURE_AND_RETHROW((op))
        }

        void_result account_role_delete_evaluator::do_apply(const account_role_delete_operation &op)
        {
            try
            {
                database &d = db();
                const account_role_object &aobj = op.account_role_id(d);
                d.remove(aobj);
                return void_result();
            }
            FC_CAPTURE_AND_RETHROW((op))
        }

    } // namespace chain
} // namespace graphene
