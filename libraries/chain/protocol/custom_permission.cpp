#include <graphene/chain/protocol/custom_permission.hpp>
#include <graphene/chain/protocol/operations.hpp>

namespace graphene
{
namespace chain
{

bool is_valid_permission_name(const string &name)
{
   try
   {
      const size_t len = name.size();
      // RBAC_MIN_PERMISSION_NAME_LENGTH <= len minimum length check
      if (len < RBAC_MIN_PERMISSION_NAME_LENGTH)
      {
         return false;
      }
      // len <= RBAC_MAX_PERMISSION_NAME_LENGTH max length check
      if (len > RBAC_MAX_PERMISSION_NAME_LENGTH)
      {
         return false;
      }
      // First character should be a letter between a-z
      if (!(name[0] >= 'a' && name[0] <= 'z'))
      {
         return false;
      }
      // Any character of a permission name should either be a small case letter a-z or a digit 0-9
      for (const auto &ch : name)
      {
         if (!((ch >= 'a' && ch <= 'z') || (ch >= '0' && ch <= '9')))
         {
            return false;
         }
      }
      // Don't accept active and owner permissions as we already have them by default
      // This is for removing ambiguity for users, accepting them doesn't create any problems
      if (name == "active" || name == "owner")
      {
         return false;
      }

      return true;
   }
   FC_CAPTURE_AND_RETHROW((name))
}

void custom_permission_create_operation::validate() const
{
   FC_ASSERT(fee.amount >= 0, "Fee must not be negative");
   FC_ASSERT(is_valid_permission_name(permission_name), "Invalid permission name provided");
   FC_ASSERT(owner_account != GRAPHENE_TEMP_ACCOUNT && owner_account != GRAPHENE_COMMITTEE_ACCOUNT && owner_account != GRAPHENE_WITNESS_ACCOUNT && owner_account != GRAPHENE_RELAXED_COMMITTEE_ACCOUNT,
             "Custom permissions and account auths cannot be created for special accounts");
   FC_ASSERT(!auth.is_impossible(), "Impossible authority threshold auth provided");
   FC_ASSERT(auth.address_auths.size() == 0, "Only account and key auths supported");
}

void custom_permission_update_operation::validate() const
{
   FC_ASSERT(fee.amount >= 0, "Fee must not be negative");
   FC_ASSERT(owner_account != GRAPHENE_TEMP_ACCOUNT && owner_account != GRAPHENE_COMMITTEE_ACCOUNT && owner_account != GRAPHENE_WITNESS_ACCOUNT && owner_account != GRAPHENE_RELAXED_COMMITTEE_ACCOUNT,
             "Custom permissions and account auths cannot be created for special accounts");
   FC_ASSERT(new_auth.valid(), "Something must be updated");
   if (new_auth)
   {
      FC_ASSERT(!new_auth->is_impossible(), "Impossible authority threshold auth provided");
      FC_ASSERT(new_auth->address_auths.size() == 0, "Only account and key auths supported");
   }
}

void custom_permission_delete_operation::validate() const
{
   FC_ASSERT(fee.amount >= 0, "Fee must not be negative");
   FC_ASSERT(owner_account != GRAPHENE_TEMP_ACCOUNT && owner_account != GRAPHENE_COMMITTEE_ACCOUNT && owner_account != GRAPHENE_WITNESS_ACCOUNT && owner_account != GRAPHENE_RELAXED_COMMITTEE_ACCOUNT,
             "Custom permissions and account auths cannot be created for special accounts");
}

share_type custom_permission_create_operation::calculate_fee(const fee_parameters_type &k) const
{
   return k.fee + calculate_data_fee( fc::raw::pack_size(*this), k.price_per_kbyte );
}

} // namespace chain
} // namespace graphene
