#pragma once

#include <graphene/peerplays_sidechain/bitcoin/types.hpp>
#include <graphene/peerplays_sidechain/bitcoin/utils.hpp>

using namespace graphene::chain;

namespace graphene { namespace peerplays_sidechain { namespace bitcoin {

const bytes op_num = {0x51, 0x52, 0x53, 0x54, 0x55, 0x56, 0x57, 0x58, 0x59, 0x5a, 0x5b, 0x5c, 0x5d, 0x5e, 0x5f}; // OP_1 - OP_15

class bitcoin_address {

public:
   enum network {
      mainnet,
      testnet,
      regtest
   };

   bitcoin_address() = default;

   bitcoin_address(const std::string &addr, network ntype = network::regtest) :
         address(addr),
         type(determine_type()),
         raw_address(determine_raw_address()),
         network_type(ntype) {
   }

   bool operator==(const bitcoin_address &btc_addr) const;

   payment_type get_type() const {
      return type;
   }

   std::string get_address() const {
      return address;
   }

   bytes get_raw_address() const {
      return raw_address;
   }

   bytes get_script() const;

   network get_network_type() const {
      return network_type;
   }

private:
   enum size_segwit_address { P2WSH = 32,
                              P2WPKH = 20 };

   payment_type determine_type();

   bytes determine_raw_address();

   bool check_segwit_address(const size_segwit_address &size) const;

   bool is_p2pk() const;

   bool is_p2wpkh() const;

   bool is_p2wsh() const;

   bool is_p2pkh() const;

   bool is_p2sh() const;

public:
   std::string address;

   payment_type type;

   bytes raw_address;

   network network_type;
};

class btc_multisig_address : public bitcoin_address {

public:
   btc_multisig_address() = default;

   btc_multisig_address(const size_t n_required, const accounts_keys &keys);

   size_t count_intersection(const accounts_keys &keys) const;

   bytes get_redeem_script() const {
      return redeem_script;
   }

private:
   void create_redeem_script();

   void create_address();

public:
   enum address_types { MAINNET_SCRIPT = 5,
                        TESTNET_SCRIPT = 196 };

   enum { OP_0 = 0x00,
          OP_EQUAL = 0x87,
          OP_HASH160 = 0xa9,
          OP_CHECKMULTISIG = 0xae };

   bytes redeem_script;

   size_t keys_required = 0;

   accounts_keys witnesses_keys;
};

// multisig segwit address (P2WSH)
// https://0bin.net/paste/nfnSf0HcBqBUGDto#7zJMRUhGEBkyh-eASQPEwKfNHgQ4D5KrUJRsk8MTPSa
class btc_multisig_segwit_address : public btc_multisig_address {

public:
   btc_multisig_segwit_address() = default;

   btc_multisig_segwit_address(const size_t n_required, const accounts_keys &keys);

   bool operator==(const btc_multisig_segwit_address &addr) const;

   bytes get_witness_script() const {
      return witness_script;
   }

   std::vector<public_key_type> get_keys();

private:
   void create_witness_script();

   void create_segwit_address();

   bytes get_address_bytes(const bytes &script_hash);

public:
   bytes witness_script;
};

class btc_weighted_multisig_address : public bitcoin_address {

public:
   btc_weighted_multisig_address() = default;

   btc_weighted_multisig_address(const std::vector<std::pair<fc::ecc::public_key, uint16_t>> &keys_data,
                                 network network_type = network::regtest);

   bytes get_redeem_script() const {
      return redeem_script_;
   }
   bytes get_witness_script() const {
      return witness_script_;
   }

private:
   void create_redeem_script(const std::vector<std::pair<fc::ecc::public_key, uint16_t>> &keys_data);
   void create_witness_script();
   void create_segwit_address();

public:
   bytes redeem_script_;
   bytes witness_script_;
};

class btc_one_or_m_of_n_multisig_address : public bitcoin_address {
public:
   btc_one_or_m_of_n_multisig_address() = default;
   btc_one_or_m_of_n_multisig_address(const fc::ecc::public_key &user_key_data, const uint8_t nrequired, const std::vector<fc::ecc::public_key> &keys_data,
                                      network network_type = network::regtest);
   bytes get_redeem_script() const {
      return redeem_script_;
   }
   bytes get_witness_script() const {
      return witness_script_;
   }

private:
   void create_redeem_script(const fc::ecc::public_key &user_key_data, const uint8_t nrequired, const std::vector<fc::ecc::public_key> &keys_data);
   void create_witness_script();
   void create_segwit_address();

public:
   bytes redeem_script_;
   bytes witness_script_;
};

class btc_one_or_weighted_multisig_address : public bitcoin_address {
public:
   btc_one_or_weighted_multisig_address() = default;
   btc_one_or_weighted_multisig_address(const fc::ecc::public_key &user_key_data, const std::vector<std::pair<fc::ecc::public_key, uint16_t>> &keys_data,
                                        network network_type = network::regtest);
   bytes get_redeem_script() const {
      return redeem_script_;
   }
   bytes get_witness_script() const {
      return witness_script_;
   }

protected:
   void create_redeem_script(const fc::ecc::public_key &user_key_data, const std::vector<std::pair<fc::ecc::public_key, uint16_t>> &keys_data);
   void create_witness_script();
   void create_segwit_address();

public:
   bytes redeem_script_;
   bytes witness_script_;
};

class btc_timelocked_one_or_weighted_multisig_address : public btc_one_or_weighted_multisig_address {
public:
   btc_timelocked_one_or_weighted_multisig_address() = default;
   btc_timelocked_one_or_weighted_multisig_address(const fc::ecc::public_key &user_key_data, uint32_t latency, const std::vector<std::pair<fc::ecc::public_key, uint16_t>> &keys_data,
                                        network network_type = network::regtest);

private:
   void create_redeem_script(const fc::ecc::public_key &user_key_data, const std::vector<std::pair<fc::ecc::public_key, uint16_t>> &keys_data);

   uint32_t latency_;
};

}}} // namespace graphene::peerplays_sidechain::bitcoin

FC_REFLECT_ENUM(graphene::peerplays_sidechain::bitcoin::bitcoin_address::network,
                (mainnet)(testnet)(regtest))

FC_REFLECT(graphene::peerplays_sidechain::bitcoin::bitcoin_address, (address)(type)(raw_address)(network_type));

FC_REFLECT_DERIVED(graphene::peerplays_sidechain::bitcoin::btc_multisig_address, (graphene::peerplays_sidechain::bitcoin::bitcoin_address),
                   (redeem_script)(keys_required)(witnesses_keys));

FC_REFLECT_DERIVED(graphene::peerplays_sidechain::bitcoin::btc_multisig_segwit_address, (graphene::peerplays_sidechain::bitcoin::btc_multisig_address), (witness_script));

FC_REFLECT_DERIVED(graphene::peerplays_sidechain::bitcoin::btc_weighted_multisig_address,
                   (graphene::peerplays_sidechain::bitcoin::bitcoin_address),
                   (redeem_script_)(witness_script_));

FC_REFLECT_DERIVED(graphene::peerplays_sidechain::bitcoin::btc_one_or_m_of_n_multisig_address,
                   (graphene::peerplays_sidechain::bitcoin::bitcoin_address),
                   (redeem_script_)(witness_script_));

FC_REFLECT_DERIVED(graphene::peerplays_sidechain::bitcoin::btc_one_or_weighted_multisig_address,
                   (graphene::peerplays_sidechain::bitcoin::bitcoin_address),
                   (redeem_script_)(witness_script_));

FC_REFLECT_DERIVED(graphene::peerplays_sidechain::bitcoin::btc_timelocked_one_or_weighted_multisig_address,
                   (graphene::peerplays_sidechain::bitcoin::btc_one_or_weighted_multisig_address),
                   (latency_));
