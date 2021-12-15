#pragma once

#include <fc/crypto/elliptic.hpp>
#include <fc/crypto/ripemd160.hpp>

namespace graphene { namespace peerplays_sidechain { namespace hive {

#define KEY_PREFIX_STM "STM"
#define KEY_PREFIX_TST "TST"

enum network {
   mainnet,
   testnet
};

struct void_t {};

typedef fc::static_variant<void_t> future_extensions;
typedef fc::flat_set<future_extensions> extensions_type;

typedef fc::ecc::private_key private_key_type;
typedef fc::sha256 chain_id_type;
typedef std::string account_name_type;
typedef fc::ripemd160 block_id_type;
//typedef fc::ripemd160 checksum_type;
typedef fc::ripemd160 transaction_id_type;
typedef fc::sha256 digest_type;
typedef fc::ecc::compact_signature signature_type;
typedef fc::safe<int64_t> share_type;
//typedef safe<uint64_t> ushare_type;
//typedef uint16_t weight_type;
//typedef uint32_t contribution_id_type;
//typedef fixed_string<32> custom_id_type;

struct public_key_type {

   static std::string prefix;

   struct binary_key {
      binary_key() {
      }
      uint32_t check = 0;
      fc::ecc::public_key_data data;
   };
   fc::ecc::public_key_data key_data;
   public_key_type();
   public_key_type(const fc::ecc::public_key_data &data);
   public_key_type(const fc::ecc::public_key &pubkey);
   explicit public_key_type(const std::string &base58str);
   operator fc::ecc::public_key_data() const;
   operator fc::ecc::public_key() const;
   explicit operator std::string() const;
   friend bool operator==(const public_key_type &p1, const fc::ecc::public_key &p2);
   friend bool operator==(const public_key_type &p1, const public_key_type &p2);
   friend bool operator<(const public_key_type &p1, const public_key_type &p2) {
      return p1.key_data < p2.key_data;
   }
   friend bool operator!=(const public_key_type &p1, const public_key_type &p2);
};

}}} // namespace graphene::peerplays_sidechain::hive

namespace fc {
void to_variant(const graphene::peerplays_sidechain::hive::public_key_type &var, fc::variant &vo, uint32_t max_depth = 2);
void from_variant(const fc::variant &var, graphene::peerplays_sidechain::hive::public_key_type &vo, uint32_t max_depth = 2);
} // namespace fc

FC_REFLECT(graphene::peerplays_sidechain::hive::public_key_type, (key_data))
FC_REFLECT(graphene::peerplays_sidechain::hive::public_key_type::binary_key, (data)(check))

FC_REFLECT(graphene::peerplays_sidechain::hive::void_t, )
FC_REFLECT_TYPENAME(graphene::peerplays_sidechain::hive::future_extensions)
