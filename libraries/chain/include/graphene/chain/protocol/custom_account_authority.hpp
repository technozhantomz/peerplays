#pragma once
#include <graphene/chain/protocol/base.hpp>

namespace graphene
{
namespace chain
{

struct custom_account_authority_create_operation : public base_operation
{
   struct fee_parameters_type
   {
      uint64_t fee = GRAPHENE_BLOCKCHAIN_PRECISION;
      uint32_t price_per_kbyte = GRAPHENE_BLOCKCHAIN_PRECISION;
   };

   asset fee;
   custom_permission_id_type permission_id;
   int operation_type;
   time_point_sec valid_from;
   time_point_sec valid_to;
   account_id_type owner_account;
   extensions_type    extensions;

   account_id_type fee_payer() const { return owner_account; }
   void validate() const;
   share_type calculate_fee(const fee_parameters_type &k) const;
};

struct custom_account_authority_update_operation : public base_operation
{
   struct fee_parameters_type
   {
      uint64_t fee = GRAPHENE_BLOCKCHAIN_PRECISION;
   };

   asset fee;
   custom_account_authority_id_type auth_id;
   optional<time_point_sec> new_valid_from;
   optional<time_point_sec> new_valid_to;
   account_id_type owner_account;
   extensions_type    extensions;

   account_id_type fee_payer() const { return owner_account; }
   void validate() const;
   share_type calculate_fee(const fee_parameters_type &k) const { return k.fee; }
};

struct custom_account_authority_delete_operation : public base_operation
{
   struct fee_parameters_type
   {
      uint64_t fee = GRAPHENE_BLOCKCHAIN_PRECISION;
   };

   asset fee;
   custom_account_authority_id_type auth_id;
   account_id_type owner_account;
   extensions_type    extensions;

   account_id_type fee_payer() const { return owner_account; }
   void validate() const;
   share_type calculate_fee(const fee_parameters_type &k) const { return k.fee; }
};

} // namespace chain
} // namespace graphene

FC_REFLECT(graphene::chain::custom_account_authority_create_operation::fee_parameters_type, (fee)(price_per_kbyte))
FC_REFLECT(graphene::chain::custom_account_authority_create_operation, (fee)(permission_id)(operation_type)(valid_from)(valid_to)(owner_account)(extensions))

FC_REFLECT(graphene::chain::custom_account_authority_update_operation::fee_parameters_type, (fee))
FC_REFLECT(graphene::chain::custom_account_authority_update_operation, (fee)(auth_id)(new_valid_from)(new_valid_to)(owner_account)(extensions))

FC_REFLECT(graphene::chain::custom_account_authority_delete_operation::fee_parameters_type, (fee))
FC_REFLECT(graphene::chain::custom_account_authority_delete_operation, (fee)(auth_id)(owner_account)(extensions))
