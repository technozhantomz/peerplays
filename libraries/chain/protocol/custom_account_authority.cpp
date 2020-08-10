#include <graphene/chain/protocol/custom_account_authority.hpp>
#include <graphene/chain/protocol/operations.hpp>

namespace graphene
{
namespace chain
{

void custom_account_authority_create_operation::validate() const
{
   FC_ASSERT(fee.amount >= 0, "Fee must not be negative");
   FC_ASSERT(owner_account != GRAPHENE_TEMP_ACCOUNT && owner_account != GRAPHENE_COMMITTEE_ACCOUNT && owner_account != GRAPHENE_WITNESS_ACCOUNT && owner_account != GRAPHENE_RELAXED_COMMITTEE_ACCOUNT,
             "Custom permissions and account auths cannot be created for special accounts");
   FC_ASSERT(valid_from < valid_to, "valid_from should be earlier than valid_to");
   FC_ASSERT(operation_type >= 0 && operation_type < operation::count(), "operation_type is not valid");
}

void custom_account_authority_update_operation::validate() const
{
   FC_ASSERT(fee.amount >= 0, "Fee must not be negative");
   FC_ASSERT(owner_account != GRAPHENE_TEMP_ACCOUNT && owner_account != GRAPHENE_COMMITTEE_ACCOUNT && owner_account != GRAPHENE_WITNESS_ACCOUNT && owner_account != GRAPHENE_RELAXED_COMMITTEE_ACCOUNT,
             "Custom permissions and account auths cannot be created for special accounts");
   FC_ASSERT(new_valid_from.valid() || new_valid_to.valid(), "Something must be updated");
   if (new_valid_from && new_valid_to)
   {
      FC_ASSERT(*new_valid_from < *new_valid_to, "valid_from should be earlier than valid_to");
   }
}

void custom_account_authority_delete_operation::validate() const
{
   FC_ASSERT(fee.amount >= 0, "Fee must not be negative");
   FC_ASSERT(owner_account != GRAPHENE_TEMP_ACCOUNT && owner_account != GRAPHENE_COMMITTEE_ACCOUNT && owner_account != GRAPHENE_WITNESS_ACCOUNT && owner_account != GRAPHENE_RELAXED_COMMITTEE_ACCOUNT,
             "Custom permissions and account auths cannot be created for special accounts");
}

share_type custom_account_authority_create_operation::calculate_fee(const fee_parameters_type &k) const
{
   return k.fee + calculate_data_fee( fc::raw::pack_size(*this), k.price_per_kbyte );
}

} // namespace chain
} // namespace graphene
