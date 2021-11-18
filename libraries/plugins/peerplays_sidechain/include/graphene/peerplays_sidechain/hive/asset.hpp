#pragma once

#include <graphene/peerplays_sidechain/hive/types.hpp>

namespace graphene { namespace peerplays_sidechain { namespace hive {

#define HBD_NAI "@@000000013"
#define HBD_PRECISION 3
#define HBD_SYMBOL_U64 (uint64_t('S') | (uint64_t('B') << 8) | (uint64_t('D') << 16))
#define HBD_SYMBOL_SER (uint64_t(3) | (HBD_SYMBOL_U64 << 8))

#define HIVE_NAI "@@000000021"
#define HIVE_PRECISION 3
#define HIVE_SYMBOL_U64 (uint64_t('S') | (uint64_t('T') << 8) | (uint64_t('E') << 16) | (uint64_t('E') << 24) | (uint64_t('M') << 32))
#define HIVE_SYMBOL_SER (uint64_t(3) | (HIVE_SYMBOL_U64 << 8))

#define TBD_NAI "@@000000013"
#define TBD_PRECISION 3
#define TBD_SYMBOL_U64 (uint64_t('T') | (uint64_t('B') << 8) | (uint64_t('D') << 16))
#define TBD_SYMBOL_SER (uint64_t(3) | (TBD_SYMBOL_U64 << 8))

#define TESTS_NAI "@@000000021"
#define TESTS_PRECISION 3
#define TESTS_SYMBOL_U64 (uint64_t('T') | (uint64_t('E') << 8) | (uint64_t('S') << 16) | (uint64_t('T') << 24) | (uint64_t('S') << 32))
#define TESTS_SYMBOL_SER (uint64_t(3) | (TESTS_SYMBOL_U64 << 8))

struct asset {
   static uint64_t hbd_symbol_ser;
   static uint64_t hive_symbol_ser;
   share_type amount;
   uint64_t symbol;
};

}}} // namespace graphene::peerplays_sidechain::hive

namespace fc {
void to_variant(const graphene::peerplays_sidechain::hive::asset &var, fc::variant &vo, uint32_t max_depth);
void from_variant(const fc::variant &var, graphene::peerplays_sidechain::hive::asset &vo, uint32_t max_depth);
} // namespace fc

FC_REFLECT(graphene::peerplays_sidechain::hive::asset, (amount)(symbol))
