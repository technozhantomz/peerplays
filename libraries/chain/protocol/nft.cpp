#include <graphene/chain/protocol/nft_ops.hpp>
#include <graphene/chain/protocol/operations.hpp>

namespace graphene
{
namespace chain
{

bool is_valid_nft_token_name(const string &name)
{
   try
   {
      const size_t len = name.size();
      // NFT_TOKEN_MIN_LENGTH <= len minimum length check
      if (len < NFT_TOKEN_MIN_LENGTH)
      {
         return false;
      }
      // len <= NFT_TOKEN_MAX_LENGTH max length check
      if (len > NFT_TOKEN_MAX_LENGTH)
      {
         return false;
      }
      // First character should be a letter between a-z/A-Z
      if (!((name[0] >= 'a' && name[0] <= 'z') || (name[0] >= 'A' && name[0] <= 'Z')))
      {
         return false;
      }
      // Any character should either be a small case letter a-z or a digit 0-9
      for (const auto &ch : name)
      {
         if (!((ch >= 'a' && ch <= 'z') || (ch >= 'A' && ch <= 'Z') || (ch >= '0' && ch <= '9') || (ch == ' ')))
         {
            return false;
         }
      }

      return true;
   }
   FC_CAPTURE_AND_RETHROW((name))
}

void nft_metadata_create_operation::validate() const
{
   FC_ASSERT(fee.amount >= 0, "Fee must not be negative");
   FC_ASSERT(is_valid_nft_token_name(name), "Invalid NFT name provided");
   FC_ASSERT(is_valid_nft_token_name(symbol), "Invalid NFT symbol provided");
   if (lottery_options)
   {
      (*lottery_options).validate();
   }
}

void nft_metadata_update_operation::validate() const
{
   FC_ASSERT(fee.amount >= 0, "Fee must not be negative");
   if(name)
      FC_ASSERT(is_valid_nft_token_name(*name), "Invalid NFT name provided");
   if(symbol)
      FC_ASSERT(is_valid_nft_token_name(*symbol), "Invalid NFT symbol provided");
}

void nft_mint_operation::validate() const
{
   FC_ASSERT(fee.amount >= 0, "Fee must not be negative");
}

share_type nft_metadata_create_operation::calculate_fee(const fee_parameters_type &k) const
{
   return k.fee + calculate_data_fee( fc::raw::pack_size(*this), k.price_per_kbyte );
}

share_type nft_metadata_update_operation::calculate_fee(const fee_parameters_type &k) const
{
   return k.fee + calculate_data_fee( fc::raw::pack_size(*this), k.price_per_kbyte );
}

share_type nft_mint_operation::calculate_fee(const fee_parameters_type &k) const
{
   return k.fee + calculate_data_fee( fc::raw::pack_size(*this), k.price_per_kbyte );
}

share_type nft_safe_transfer_from_operation::calculate_fee(const fee_parameters_type &k) const
{
   return k.fee + calculate_data_fee( fc::raw::pack_size(*this), k.price_per_kbyte );
}

share_type nft_approve_operation::calculate_fee(const fee_parameters_type &k) const
{
   return k.fee;
}

share_type nft_set_approval_for_all_operation::calculate_fee(const fee_parameters_type &k) const
{
   return k.fee;
}

} // namespace chain
} // namespace graphene
