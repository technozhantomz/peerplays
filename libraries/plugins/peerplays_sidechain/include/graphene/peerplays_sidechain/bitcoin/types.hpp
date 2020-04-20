#pragma once

#include <fc/reflect/reflect.hpp>
#include <map>
#include <string>
#include <vector>

#include <graphene/chain/protocol/types.hpp>

namespace graphene { namespace peerplays_sidechain { namespace bitcoin {

class bitcoin_transaction;

using bytes = std::vector<char>;
using accounts_keys = std::map<graphene::chain::son_id_type, graphene::chain::public_key_type>;
using full_btc_transaction = std::pair<bitcoin_transaction, uint64_t>;

enum class payment_type {
   NULLDATA,
   P2PK,
   P2PKH,
   P2SH,
   P2WPKH,
   P2WSH,
   P2SH_WPKH,
   P2SH_WSH
};

enum class sidechain_proposal_type {
   ISSUE_BTC,
   SEND_BTC_TRANSACTION,
   REVERT_BTC_TRANSACTION
};

struct prev_out {
   bool operator!=(const prev_out &obj) const {
      if (this->hash_tx != obj.hash_tx ||
          this->n_vout != obj.n_vout ||
          this->amount != obj.amount) {
         return true;
      }
      return false;
   }

   bool operator==(const prev_out &obj) const {
      if (this->hash_tx == obj.hash_tx &&
          this->n_vout == obj.n_vout &&
          this->amount == obj.amount) {
         return true;
      }
      return false;
   }

   std::string hash_tx;
   uint32_t n_vout;
   uint64_t amount;
};

}}} // namespace graphene::peerplays_sidechain::bitcoin

FC_REFLECT_ENUM(graphene::peerplays_sidechain::bitcoin::payment_type, (NULLDATA)(P2PK)(P2PKH)(P2SH)(P2WPKH)(P2WSH)(P2SH_WPKH)(P2SH_WSH));
FC_REFLECT_ENUM(graphene::peerplays_sidechain::bitcoin::sidechain_proposal_type, (ISSUE_BTC)(SEND_BTC_TRANSACTION)(REVERT_BTC_TRANSACTION));
FC_REFLECT(graphene::peerplays_sidechain::bitcoin::prev_out, (hash_tx)(n_vout)(amount));
