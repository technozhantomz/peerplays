#pragma once
#include <graphene/chain/protocol/base.hpp>

namespace graphene
{
namespace chain
{

struct custom_permission_create_operation : public base_operation
{
   struct fee_parameters_type
   {
      uint64_t fee             = GRAPHENE_BLOCKCHAIN_PRECISION;
      uint32_t price_per_kbyte = GRAPHENE_BLOCKCHAIN_PRECISION;
   };

   asset fee;
   account_id_type owner_account;
   string permission_name;
   authority auth;

   account_id_type fee_payer() const { return owner_account; }
   void validate() const;
   share_type calculate_fee(const fee_parameters_type &k) const;
};

struct custom_permission_update_operation : public base_operation
{
   struct fee_parameters_type
   {
      uint64_t fee = GRAPHENE_BLOCKCHAIN_PRECISION;
   };

   asset fee;
   custom_permission_id_type permission_id;
   optional<authority> new_auth;
   account_id_type owner_account;

   account_id_type fee_payer() const { return owner_account; }
   void validate() const;
   share_type calculate_fee(const fee_parameters_type &k) const { return k.fee; }
};

struct custom_permission_delete_operation : public base_operation
{
   struct fee_parameters_type
   {
      uint64_t fee = GRAPHENE_BLOCKCHAIN_PRECISION;
   };

   asset fee;
   custom_permission_id_type permission_id;
   account_id_type owner_account;

   account_id_type fee_payer() const { return owner_account; }
   void validate() const;
   share_type calculate_fee(const fee_parameters_type &k) const { return k.fee; }
};

} // namespace chain
} // namespace graphene

FC_REFLECT(graphene::chain::custom_permission_create_operation::fee_parameters_type, (fee)(price_per_kbyte))
FC_REFLECT(graphene::chain::custom_permission_create_operation, (fee)(owner_account)(permission_name)(auth))

FC_REFLECT(graphene::chain::custom_permission_update_operation::fee_parameters_type, (fee))
FC_REFLECT(graphene::chain::custom_permission_update_operation, (fee)(permission_id)(new_auth)(owner_account))

FC_REFLECT(graphene::chain::custom_permission_delete_operation::fee_parameters_type, (fee))
FC_REFLECT(graphene::chain::custom_permission_delete_operation, (fee)(permission_id)(owner_account))