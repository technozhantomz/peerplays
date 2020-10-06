#pragma once
#include <fc/crypto/elliptic.hpp>
#include <fc/crypto/hex.hpp>
#include <graphene/peerplays_sidechain/bitcoin/types.hpp>

namespace graphene { namespace peerplays_sidechain { namespace bitcoin {

fc::ecc::public_key_data create_public_key_data(const std::vector<char> &public_key);

bytes get_privkey_bytes(const std::string &privkey_base58);

bytes parse_hex(const std::string &str);

std::vector<bytes> get_pubkey_from_redeemScript(bytes script);

bytes public_key_data_to_bytes(const fc::ecc::public_key_data &key);

std::vector<bytes> read_byte_arrays_from_string(const std::string &string_buf);

std::string write_transaction_signatures(const std::vector<bytes> &data);

void read_transaction_data(const std::string &string_buf, std::string &tx_hex, std::vector<uint64_t> &in_amounts, std::string &redeem_script);

std::string write_transaction_data(const std::string &tx, const std::vector<uint64_t> &in_amounts, const std::string &redeem_script);

}}} // namespace graphene::peerplays_sidechain::bitcoin