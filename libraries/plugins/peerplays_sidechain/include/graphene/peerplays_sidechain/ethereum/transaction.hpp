#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include <graphene/peerplays_sidechain/ethereum/types.hpp>

namespace graphene { namespace peerplays_sidechain { namespace ethereum {

bytes keccak_hash(const std::string &data);
signature sign_hash(const bytes &hash, const std::string &chain_id, const std::string &private_key);

class base_transaction {
public:
   base_transaction() = default;
   base_transaction(const std::string &raw_tx);

   virtual std::string serialize() const = 0;
   virtual void deserialize(const std::string &raw_tx) = 0;
};

class transaction : base_transaction {
public:
   std::string from;
   std::string to;
   std::string data;

   transaction() = default;
   transaction(const std::string &raw_tx);

   const transaction &sign(const std::string &private_key) const;

   virtual std::string serialize() const override;
   virtual void deserialize(const std::string &raw_tx) override;
};

class signed_transaction;
class raw_transaction : base_transaction {
public:
   std::string nonce;
   std::string gas_price;
   std::string gas_limit;
   std::string to;
   std::string value;
   std::string data;
   std::string chain_id;

   raw_transaction() = default;
   raw_transaction(const std::string &raw_tx);

   bytes hash() const;
   signed_transaction sign(const std::string &private_key) const;

   virtual std::string serialize() const override;
   virtual void deserialize(const std::string &raw_tx) override;
};

class signed_transaction : base_transaction {
public:
   std::string nonce;
   std::string gas_price;
   std::string gas_limit;
   std::string to;
   std::string value;
   std::string data;
   std::string v;
   std::string r;
   std::string s;

   signed_transaction() = default;
   signed_transaction(const std::string &raw_tx);

   std::string recover(const std::string &chain_id) const;

   virtual std::string serialize() const override;
   virtual void deserialize(const std::string &raw_tx) override;
};

}}} // namespace graphene::peerplays_sidechain::ethereum
