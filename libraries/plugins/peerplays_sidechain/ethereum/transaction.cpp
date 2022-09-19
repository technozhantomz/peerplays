#include <graphene/peerplays_sidechain/ethereum/transaction.hpp>

#include <boost/algorithm/hex.hpp>
#include <boost/algorithm/string.hpp>
#include <boost/property_tree/json_parser.hpp>
#include <boost/property_tree/ptree.hpp>

#include <secp256k1_recovery.h>
#include <sha3/sha3.h>

#include <fc/crypto/elliptic.hpp>
#include <fc/crypto/hex.hpp>

#include <graphene/peerplays_sidechain/ethereum/decoders.hpp>
#include <graphene/peerplays_sidechain/ethereum/encoders.hpp>
#include <graphene/peerplays_sidechain/ethereum/utils.hpp>

namespace graphene { namespace peerplays_sidechain { namespace ethereum {

const secp256k1_context *eth_context() {
   static secp256k1_context *ctx = secp256k1_context_create(SECP256K1_CONTEXT_VERIFY | SECP256K1_CONTEXT_SIGN);
   return ctx;
}

//! transaction

base_transaction::base_transaction(const std::string &raw_tx) {
}

//! transaction

transaction::transaction(const std::string &raw_tx) :
      base_transaction{raw_tx} {
   deserialize(raw_tx);
}

const transaction &transaction::sign(const std::string &private_key) const {
   return *this;
}

std::string transaction::serialize() const {
   boost::property_tree::ptree pt;
   pt.put("from", from);
   pt.put("to", to);
   pt.put("data", data);

   std::stringstream ss;
   boost::property_tree::json_parser::write_json(ss, pt);
   return ss.str();
}

void transaction::deserialize(const std::string &raw_tx) {
   std::stringstream ss_tx(raw_tx);
   boost::property_tree::ptree tx_json;
   boost::property_tree::read_json(ss_tx, tx_json);

   if (tx_json.count("from"))
      from = tx_json.get<std::string>("from");
   if (tx_json.count("to"))
      to = tx_json.get<std::string>("to");
   if (tx_json.count("data"))
      data = tx_json.get<std::string>("data");
}

//! raw_transaction

raw_transaction::raw_transaction(const std::string &raw_tx) :
      base_transaction{raw_tx} {
   deserialize(raw_tx);
}

bytes raw_transaction::hash() const {
   bytes hash;
   hash.resize(32);
   const auto transaction_string = boost::algorithm::unhex(remove_0x(serialize()));
   keccak_256((const unsigned char *)transaction_string.data(), transaction_string.size(), (unsigned char *)hash.data());

   return hash;
}

signed_transaction raw_transaction::sign(const std::string &private_key) const {
   //! Prepare signed transaction
   signed_transaction tr;
   tr.nonce = nonce;
   tr.gas_price = gas_price;
   tr.gas_limit = gas_limit;
   tr.to = to;
   tr.value = value;
   tr.data = data;

   const bytes priv_key = parse_hex(private_key);

   int recid = 0;
   secp256k1_ecdsa_recoverable_signature sig;
   FC_ASSERT(secp256k1_ecdsa_sign_recoverable(eth_context(), &sig, (const unsigned char *)hash().data(), (const unsigned char *)priv_key.data(), NULL, NULL));
   fc::ecc::compact_signature result;
   FC_ASSERT(secp256k1_ecdsa_recoverable_signature_serialize_compact(eth_context(), (unsigned char *)result.begin() + 1, &recid, &sig));

   bytes r;
   for (int i = 1; i < 33; i++)
      r.emplace_back((char)result.at(i));

   bytes v = bytes{char(recid + from_hex<int>(chain_id) * 2 + 35)};

   bytes s;
   for (int i = 33; i < 65; i++)
      s.emplace_back((char)result.at(i));

   tr.r = fc::to_hex((char *)&r[0], r.size());
   tr.v = fc::to_hex((char *)&v[0], v.size());
   tr.s = fc::to_hex((char *)&s[0], s.size());

   return tr;
}

std::string raw_transaction::serialize() const {
   const std::string serialized = rlp_encoder::encode(remove_0x(nonce)) +
                                  rlp_encoder::encode(remove_0x(gas_price)) +
                                  rlp_encoder::encode(remove_0x(gas_limit)) +
                                  rlp_encoder::encode(remove_0x(to)) +
                                  rlp_encoder::encode(remove_0x(value)) +
                                  rlp_encoder::encode(remove_0x(data)) +
                                  rlp_encoder::encode(remove_0x(chain_id)) +
                                  rlp_encoder::encode("") +
                                  rlp_encoder::encode("");

   return add_0x(bytes2hex(rlp_encoder::encode_length(serialized.size(), 192) + serialized));
}

void raw_transaction::deserialize(const std::string &raw_tx) {
   const auto rlp_array = rlp_decoder::decode(remove_0x(raw_tx));
   FC_ASSERT(rlp_array.size() >= 7, "Wrong rlp format");

   nonce = !rlp_array.at(0).empty() ? add_0x(rlp_array.at(0)) : add_0x("0");
   boost::algorithm::to_lower(nonce);
   gas_price = add_0x(rlp_array.at(1));
   boost::algorithm::to_lower(gas_price);
   gas_limit = add_0x(rlp_array.at(2));
   boost::algorithm::to_lower(gas_limit);
   to = add_0x(rlp_array.at(3));
   boost::algorithm::to_lower(to);
   value = !rlp_array.at(4).empty() ? add_0x(rlp_array.at(4)) : add_0x("0");
   boost::algorithm::to_lower(value);
   data = !rlp_array.at(5).empty() ? add_0x(rlp_array.at(5)) : "";
   boost::algorithm::to_lower(data);
   chain_id = add_0x(rlp_array.at(6));
   boost::algorithm::to_lower(chain_id);
}

//! signed_transaction

signed_transaction::signed_transaction(const std::string &raw_tx) :
      base_transaction{raw_tx} {
   deserialize(raw_tx);
}

std::string signed_transaction::recover(const std::string &chain_id) const {
   fc::ecc::compact_signature input64;
   fc::from_hex(r, (char *)&input64.at(1), 32);
   fc::from_hex(v, (char *)&input64.at(0), 1);
   int recid = input64.at(0) - from_hex<int>(chain_id) * 2 - 35;
   fc::from_hex(s, (char *)&input64.at(33), 32);

   secp256k1_ecdsa_recoverable_signature sig;
   FC_ASSERT(secp256k1_ecdsa_recoverable_signature_parse_compact(eth_context(), &sig, (const unsigned char *)&input64.data[1], recid));

   raw_transaction tr;
   tr.nonce = nonce;
   tr.gas_price = gas_price;
   tr.gas_limit = gas_limit;
   tr.to = to;
   tr.value = value;
   tr.data = data;
   tr.chain_id = chain_id;

   secp256k1_pubkey rawPubkey;
   FC_ASSERT(secp256k1_ecdsa_recover(eth_context(), &rawPubkey, &sig, (const unsigned char *)tr.hash().data()));

   std::array<uint8_t, 65> pubkey;
   size_t biglen = 65;
   FC_ASSERT(secp256k1_ec_pubkey_serialize(eth_context(), pubkey.data(), &biglen, &rawPubkey, SECP256K1_EC_UNCOMPRESSED));

   const std::string out = std::string(pubkey.begin(), pubkey.end()).substr(1);
   bytes hash;
   hash.resize(32);
   keccak_256((const unsigned char *)out.data(), out.size(), (unsigned char *)hash.data());

   return add_0x(fc::to_hex((char *)&hash[0], hash.size()).substr(24));
}

std::string signed_transaction::serialize() const {
   const std::string serialized = rlp_encoder::encode(remove_0x(nonce)) +
                                  rlp_encoder::encode(remove_0x(gas_price)) +
                                  rlp_encoder::encode(remove_0x(gas_limit)) +
                                  rlp_encoder::encode(remove_0x(to)) +
                                  rlp_encoder::encode(remove_0x(value)) +
                                  rlp_encoder::encode(remove_0x(data)) +
                                  rlp_encoder::encode(remove_0x(v)) +
                                  rlp_encoder::encode(remove_0x(r)) +
                                  rlp_encoder::encode(remove_0x(s));

   return add_0x(bytes2hex(rlp_encoder::encode_length(serialized.size(), 192) + serialized));
}

void signed_transaction::deserialize(const std::string &raw_tx) {
   const auto rlp_array = rlp_decoder::decode(remove_0x(raw_tx));
   FC_ASSERT(rlp_array.size() >= 9, "Wrong rlp format");

   nonce = !rlp_array.at(0).empty() ? add_0x(rlp_array.at(0)) : add_0x("0");
   boost::algorithm::to_lower(nonce);
   gas_price = add_0x(rlp_array.at(1));
   boost::algorithm::to_lower(gas_price);
   gas_limit = add_0x(rlp_array.at(2));
   boost::algorithm::to_lower(gas_limit);
   to = add_0x(rlp_array.at(3));
   boost::algorithm::to_lower(to);
   value = !rlp_array.at(4).empty() ? add_0x(rlp_array.at(4)) : add_0x("0");
   boost::algorithm::to_lower(value);
   data = !rlp_array.at(5).empty() ? add_0x(rlp_array.at(5)) : "";
   boost::algorithm::to_lower(data);
   v = add_0x(rlp_array.at(6));
   boost::algorithm::to_lower(v);
   r = add_0x(rlp_array.at(7));
   boost::algorithm::to_lower(r);
   s = add_0x(rlp_array.at(8));
   boost::algorithm::to_lower(s);
}

}}} // namespace graphene::peerplays_sidechain::ethereum
