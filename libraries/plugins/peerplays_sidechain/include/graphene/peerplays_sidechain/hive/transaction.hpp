#pragma once

#include <cstdint>
#include <vector>

#include <fc/container/flat_fwd.hpp>
#include <fc/time.hpp>

#include <graphene/peerplays_sidechain/hive/operations.hpp>

namespace graphene { namespace peerplays_sidechain { namespace hive {

struct transaction {
   uint16_t ref_block_num = 0;
   uint32_t ref_block_prefix = 0;
   fc::time_point_sec expiration;
   std::vector<hive_operation> operations;
   extensions_type extensions;

   digest_type digest() const;
   transaction_id_type id() const;
   digest_type sig_digest(const chain_id_type &chain_id) const;

   void set_expiration(fc::time_point_sec expiration_time);
   void set_reference_block(const block_id_type &reference_block);
};

struct signed_transaction : public transaction {

   std::vector<fc::ecc::compact_signature> signatures;

   const signature_type &sign(const hive::private_key_type &key, const hive::chain_id_type &chain_id);
   signature_type sign(const hive::private_key_type &key, const hive::chain_id_type &chain_id) const;
   void clear();
};

}}} // namespace graphene::peerplays_sidechain::hive

FC_REFLECT(graphene::peerplays_sidechain::hive::transaction,
           (ref_block_num)(ref_block_prefix)(expiration)(operations)(extensions))

FC_REFLECT_DERIVED(graphene::peerplays_sidechain::hive::signed_transaction,
                   (graphene::peerplays_sidechain::hive::transaction),
                   (signatures))
