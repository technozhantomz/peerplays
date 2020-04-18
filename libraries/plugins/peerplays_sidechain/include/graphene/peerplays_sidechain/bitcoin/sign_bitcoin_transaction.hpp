#pragma once

#include <fc/crypto/sha256.hpp>
#include <graphene/peerplays_sidechain/bitcoin/types.hpp>
#include <secp256k1.h>

namespace graphene { namespace peerplays_sidechain { namespace bitcoin {

class bitcoin_transaction;

const secp256k1_context_t *btc_context();

fc::sha256 get_signature_hash(const bitcoin_transaction &tx, const bytes &scriptPubKey, int64_t amount,
                              size_t in_index, int hash_type, bool is_witness);

std::vector<char> privkey_sign(const bytes &privkey, const fc::sha256 &hash, const secp256k1_context_t *context_sign = nullptr);

std::vector<bytes> sign_witness_transaction_part(const bitcoin_transaction &tx, const std::vector<bytes> &redeem_scripts,
                                                 const std::vector<uint64_t> &amounts, const bytes &privkey,
                                                 const secp256k1_context_t *context_sign = nullptr, int hash_type = 1);

void sign_witness_transaction_finalize(bitcoin_transaction &tx, const std::vector<bytes> &redeem_scripts, bool use_mulisig_workaround = true);

bool verify_sig(const bytes &sig, const bytes &pubkey, const bytes &msg, const secp256k1_context_t *context);

std::vector<std::vector<bytes>> sort_sigs(const bitcoin_transaction &tx, const std::vector<bytes> &redeem_scripts,
                                          const std::vector<uint64_t> &amounts, const secp256k1_context_t *context);

void add_signatures_to_transaction_multisig(bitcoin_transaction &tx, std::vector<std::vector<bytes>> &signature_set);

void add_signatures_to_transaction_weighted_multisig(bitcoin_transaction &tx, std::vector<std::vector<bytes>> &signature_set);

void add_signatures_to_transaction_user_weighted_multisig(bitcoin_transaction &tx, std::vector<std::vector<bytes>> &signature_set);

}}} // namespace graphene::peerplays_sidechain::bitcoin
