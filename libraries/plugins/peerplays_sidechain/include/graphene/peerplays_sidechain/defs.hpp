#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include <fc/crypto/sha256.hpp>
#include <fc/safe.hpp>
#include <fc/time.hpp>

#include <graphene/chain/protocol/asset.hpp>
#include <graphene/chain/protocol/types.hpp>
#include <graphene/chain/sidechain_defs.hpp>

namespace graphene { namespace peerplays_sidechain {

using namespace graphene::chain;

using bytes = std::vector<unsigned char>;

struct prev_out {
   bool operator!=(const prev_out &obj) const {
      if (this->hash_tx != obj.hash_tx ||
          this->n_vout != obj.n_vout ||
          this->amount != obj.amount) {
         return true;
      }
      return false;
   }

   std::string hash_tx;
   uint32_t n_vout;
   uint64_t amount;
};

struct info_for_vin {
   info_for_vin() = default;

   info_for_vin(const prev_out &_out, const std::string &_address, bytes _script = bytes(), bool _resend = false);

   bool operator!=(const info_for_vin &obj) const;

   struct comparer {
      bool operator()(const info_for_vin &lhs, const info_for_vin &rhs) const;
   };

   static uint64_t count_id_info_for_vin;
   uint64_t id;

   fc::sha256 identifier;

   prev_out out;
   std::string address;
   bytes script;

   bool used = false;
   bool resend = false;
};

struct sidechain_event_data {
   fc::time_point_sec timestamp;
   uint32_t block_num;
   sidechain_type sidechain;
   std::string sidechain_uid;
   std::string sidechain_transaction_id;
   std::string sidechain_from;
   std::string sidechain_to;
   std::string sidechain_currency;
   fc::safe<int64_t> sidechain_amount;
   account_id_type peerplays_from;
   account_id_type peerplays_to;
   asset peerplays_asset;
};

}} // namespace graphene::peerplays_sidechain
