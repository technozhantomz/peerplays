#include <graphene/chain/protocol/account_role.hpp>
#include <graphene/chain/protocol/operations.hpp>

namespace graphene
{
    namespace chain
    {

        void account_role_create_operation::validate() const
        {
            FC_ASSERT(fee.amount >= 0, "Fee must not be negative");
            FC_ASSERT(allowed_operations.size() > 0, "Allowed operations should be positive");
            FC_ASSERT(whitelisted_accounts.size() > 0, "Whitelisted accounts should be positive");
        }

        void account_role_update_operation::validate() const
        {
            FC_ASSERT(fee.amount >= 0, "Fee must not be negative");
            for (auto aop : allowed_operations_to_add)
            {
                FC_ASSERT(aop >= 0 && aop < operation::count(), "operation_type is not valid");
                FC_ASSERT(allowed_operations_to_remove.find(aop) == allowed_operations_to_remove.end(),
                          "Cannot add and remove allowed operation at the same time.");
            }
            for (auto aop : allowed_operations_to_remove)
            {
                FC_ASSERT(aop >= 0 && aop < operation::count(), "operation_type is not valid");
                FC_ASSERT(allowed_operations_to_add.find(aop) == allowed_operations_to_add.end(),
                          "Cannot add and remove allowed operation at the same time.");
            }

            for (auto acc : accounts_to_add)
            {
                FC_ASSERT(accounts_to_remove.find(acc) == accounts_to_remove.end(),
                          "Cannot add and remove accounts at the same time.");
            }

            for (auto acc : accounts_to_remove)
            {
                FC_ASSERT(accounts_to_add.find(acc) == accounts_to_add.end(),
                          "Cannot add and remove accounts at the same time.");
            }
        }

        void account_role_delete_operation::validate() const
        {
            FC_ASSERT(fee.amount >= 0, "Fee must not be negative");
        }

        share_type account_role_create_operation::calculate_fee(const fee_parameters_type &k) const
        {
            return k.fee + calculate_data_fee(fc::raw::pack_size(*this), k.price_per_kbyte);
        }

        share_type account_role_update_operation::calculate_fee(const fee_parameters_type &k) const
        {
            return k.fee + calculate_data_fee(fc::raw::pack_size(*this), k.price_per_kbyte);
        }

    } // namespace chain
} // namespace graphene
