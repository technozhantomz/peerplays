#include <fc/crypto/base58.hpp>
#include <fc/crypto/sha256.hpp>
#include <graphene/peerplays_sidechain/bitcoin/bitcoin_address.hpp>
#include <graphene/peerplays_sidechain/bitcoin/bitcoin_script.hpp>
#include <graphene/peerplays_sidechain/bitcoin/segwit_addr.hpp>
#include <sstream>

namespace graphene { namespace peerplays_sidechain { namespace bitcoin {

bool bitcoin_address::operator==(const bitcoin_address &btc_addr) const {
   return (this->address == btc_addr.address) &&
          (this->type == btc_addr.type) &&
          (this->raw_address == btc_addr.raw_address) &&
          (this->network_type == btc_addr.network_type);
}

bytes bitcoin_address::get_script() const {
   switch (type) {
   case payment_type::NULLDATA:
      return script_builder() << op::RETURN << raw_address;
   case payment_type::P2PK:
      return script_builder() << raw_address << op::CHECKSIG;
   case payment_type::P2PKH:
      return script_builder() << op::DUP << op::HASH160 << raw_address << op::EQUALVERIFY << op::CHECKSIG;
   case payment_type::P2SH:
   case payment_type::P2SH_WPKH:
   case payment_type::P2SH_WSH:
      return script_builder() << op::HASH160 << raw_address << op::EQUAL;
   case payment_type::P2WPKH:
   case payment_type::P2WSH:
      return script_builder() << op::_0 << raw_address;
   default:
      return raw_address;
   }
}

payment_type bitcoin_address::determine_type() {
   if (is_p2pk()) {
      return payment_type::P2PK;
   } else if (is_p2wpkh()) {
      return payment_type::P2WPKH;
   } else if (is_p2wsh()) {
      return payment_type::P2WSH;
   } else if (is_p2pkh()) {
      return payment_type::P2PKH;
   } else if (is_p2sh()) {
      return payment_type::P2SH;
   } else {
      return payment_type::NULLDATA;
   }
}

bytes bitcoin_address::determine_raw_address() {
   bytes result;
   switch (type) {
   case payment_type::P2PK: {
      result = parse_hex(address);
      break;
   }
   case payment_type::P2WPKH:
   case payment_type::P2WSH: {
      std::string prefix(address.compare(0, 4, "bcrt") == 0 ? std::string(address.begin(), address.begin() + 4) : std::string(address.begin(), address.begin() + 2));
      const auto &decode_bech32 = segwit_addr::decode(prefix, address);
      result = bytes(decode_bech32.second.begin(), decode_bech32.second.end());
      break;
   }
   case payment_type::P2SH_WPKH:
   case payment_type::P2SH_WSH:
   case payment_type::P2PKH:
   case payment_type::P2SH: {
      bytes hex_addr = fc::from_base58(address);
      result = bytes(hex_addr.begin() + 1, hex_addr.begin() + 21);
      break;
   }
   case payment_type::NULLDATA:
      return result;
   }
   return result;
}

bool bitcoin_address::check_segwit_address(const size_segwit_address &size) const {
   if (!address.compare(0, 4, "bcrt") || !address.compare(0, 2, "bc") || !address.compare(0, 2, "tb")) {
      std::string prefix(!address.compare(0, 4, "bcrt") ? std::string(address.begin(), address.begin() + 4) : std::string(address.begin(), address.begin() + 2));

      const auto &decode_bech32 = segwit_addr::decode(prefix, address);

      if (decode_bech32.first == -1 || decode_bech32.second.size() != size) {
         return false;
      }

      return true;
   }
   return false;
}

bool bitcoin_address::is_p2pk() const {
   try {
      bool prefix = !address.compare(0, 2, "02") || !address.compare(0, 2, "03");
      if (address.size() == 66 && prefix) {
         parse_hex(address);
         return true;
      }
   } catch (fc::exception &e) {
      return false;
   }
   return false;
}

bool bitcoin_address::is_p2wpkh() const {
   return check_segwit_address(size_segwit_address::P2WPKH);
}

bool bitcoin_address::is_p2wsh() const {
   return check_segwit_address(size_segwit_address::P2WSH);
}

bool bitcoin_address::is_p2pkh() const {
   try {
      bytes hex_addr = fc::from_base58(address);
      if (hex_addr.size() == 25 && (static_cast<unsigned char>(hex_addr[0]) == 0x00 ||
                                    static_cast<unsigned char>(hex_addr[0]) == 0x6f)) {
         return true;
      }
      return false;
   } catch (fc::exception &e) {
      return false;
   }
}

bool bitcoin_address::is_p2sh() const {
   try {
      bytes hex_addr = fc::from_base58(address);
      if (hex_addr.size() == 25 && (static_cast<unsigned char>(hex_addr[0]) == 0x05 ||
                                    static_cast<unsigned char>(hex_addr[0]) == 0xc4)) {
         return true;
      }
      return false;
   } catch (fc::exception &e) {
      return false;
   }
}

btc_multisig_address::btc_multisig_address(const size_t n_required, const accounts_keys &keys) :
      keys_required(n_required),
      witnesses_keys(keys) {
   create_redeem_script();
   create_address();
   type = payment_type::P2SH;
}

size_t btc_multisig_address::count_intersection(const accounts_keys &keys) const {
   FC_ASSERT(keys.size() > 0);

   int intersections_count = 0;
   for (auto &key : keys) {
      auto witness_key = witnesses_keys.find(key.first);
      if (witness_key == witnesses_keys.end())
         continue;
      if (key.second == witness_key->second)
         intersections_count++;
   }
   return intersections_count;
}

void btc_multisig_address::create_redeem_script() {
   FC_ASSERT(keys_required > 0);
   FC_ASSERT(keys_required < witnesses_keys.size());
   redeem_script.clear();
   redeem_script.push_back(op_num[keys_required - 1]);
   for (const auto &key : witnesses_keys) {
      std::stringstream ss;
      ss << std::hex << key.second.key_data.size();
      auto key_size_hex = parse_hex(ss.str());
      redeem_script.insert(redeem_script.end(), key_size_hex.begin(), key_size_hex.end());
      redeem_script.insert(redeem_script.end(), key.second.key_data.begin(), key.second.key_data.end());
   }
   redeem_script.push_back(op_num[witnesses_keys.size() - 1]);
   redeem_script.push_back(OP_CHECKMULTISIG);
}

void btc_multisig_address::create_address() {
   FC_ASSERT(redeem_script.size() > 0);
   raw_address.clear();
   fc::sha256 hash256 = fc::sha256::hash(redeem_script.data(), redeem_script.size());
   fc::ripemd160 hash160 = fc::ripemd160::hash(hash256.data(), hash256.data_size());
   bytes temp_addr_hash(parse_hex(hash160.str()));

   raw_address.push_back(OP_HASH160);
   std::stringstream ss;
   ss << std::hex << temp_addr_hash.size();
   auto address_size_hex = parse_hex(ss.str());
   raw_address.insert(raw_address.end(), address_size_hex.begin(), address_size_hex.end());
   raw_address.insert(raw_address.end(), temp_addr_hash.begin(), temp_addr_hash.end());
   raw_address.push_back(OP_EQUAL);
}

btc_multisig_segwit_address::btc_multisig_segwit_address(const size_t n_required, const accounts_keys &keys) :
      btc_multisig_address(n_required, keys) {
   create_witness_script();
   create_segwit_address();
   type = payment_type::P2SH;
}

bool btc_multisig_segwit_address::operator==(const btc_multisig_segwit_address &addr) const {
   if (address != addr.address || redeem_script != addr.redeem_script ||
       witnesses_keys != addr.witnesses_keys || witness_script != addr.witness_script ||
       raw_address != addr.raw_address)
      return false;
   return true;
}

std::vector<public_key_type> btc_multisig_segwit_address::get_keys() {
   std::vector<public_key_type> keys;
   for (const auto &k : witnesses_keys) {
      keys.push_back(k.second);
   }
   return keys;
}

void btc_multisig_segwit_address::create_witness_script() {
   const auto redeem_sha256 = fc::sha256::hash(redeem_script.data(), redeem_script.size());
   witness_script.push_back(OP_0);
   witness_script.push_back(0x20); // PUSH_32
   witness_script.insert(witness_script.end(), redeem_sha256.data(), redeem_sha256.data() + redeem_sha256.data_size());
}

void btc_multisig_segwit_address::create_segwit_address() {
   fc::sha256 hash256 = fc::sha256::hash(witness_script.data(), witness_script.size());
   fc::ripemd160 hash160 = fc::ripemd160::hash(hash256.data(), hash256.data_size());

   raw_address = bytes(hash160.data(), hash160.data() + hash160.data_size());
   address = fc::to_base58(get_address_bytes(raw_address));
}

bytes btc_multisig_segwit_address::get_address_bytes(const bytes &script_hash) {
   bytes address_bytes(1, TESTNET_SCRIPT); // 1 byte version
   address_bytes.insert(address_bytes.end(), script_hash.begin(), script_hash.end());
   fc::sha256 hash256 = fc::sha256::hash(fc::sha256::hash(address_bytes.data(), address_bytes.size()));
   address_bytes.insert(address_bytes.end(), hash256.data(), hash256.data() + 4); // 4 byte checksum

   return address_bytes;
}

btc_weighted_multisig_address::btc_weighted_multisig_address(const std::vector<std::pair<fc::ecc::public_key, uint16_t>> &keys_data,
                                                             network ntype) {
   network_type = ntype;
   create_redeem_script(keys_data);
   create_witness_script();
   create_segwit_address();
   type = payment_type::P2WSH;
}

void btc_weighted_multisig_address::create_redeem_script(const std::vector<std::pair<fc::ecc::public_key, uint16_t>> &keys_data) {
   script_builder builder;
   uint32_t total_weight = 0;
   builder << uint32_t(0);
   for (auto &p : keys_data) {
      total_weight += p.second;
      builder << op::SWAP;
      builder << p.first.serialize();
      builder << op::CHECKSIG;
      builder << op::IF;
      builder << uint32_t(p.second);
      builder << op::ADD;
      builder << op::ENDIF;
   }
   uint32_t threshold_weight = total_weight * 2 / 3 + 1;
   builder << threshold_weight;
   builder << op::GREATERTHANOREQUAL;

   redeem_script_ = builder;

   fc::sha256 sh = fc::sha256::hash(redeem_script_.data(), redeem_script_.size());
   raw_address = bytes(sh.data(), sh.data() + sh.data_size());
}

void btc_weighted_multisig_address::create_witness_script() {
   script_builder builder;
   builder << op::_0;
   builder << fc::sha256::hash(redeem_script_.data(), redeem_script_.size());

   witness_script_ = builder;
}

void btc_weighted_multisig_address::create_segwit_address() {
   std::string hrp;
   switch (network_type) {
   case (network::mainnet):
      hrp = "bc";
      break;
   case (network::testnet):
      hrp = "tb";
      break;
   case (network::regtest):
      hrp = "bcrt";
      break;
   }
   fc::sha256 sh = fc::sha256::hash(&redeem_script_[0], redeem_script_.size());
   std::vector<uint8_t> hash_data(sh.data(), sh.data() + sh.data_size());
   address = segwit_addr::encode(hrp, 0, hash_data);
}

btc_one_or_m_of_n_multisig_address::btc_one_or_m_of_n_multisig_address(const fc::ecc::public_key &user_key_data,
                                                                       const uint8_t nrequired, const std::vector<fc::ecc::public_key> &keys_data,
                                                                       network ntype) {
   network_type = ntype;
   create_redeem_script(user_key_data, nrequired, keys_data);
   create_witness_script();
   create_segwit_address();
   type = payment_type::P2WSH;
}
void btc_one_or_m_of_n_multisig_address::create_redeem_script(const fc::ecc::public_key &user_key_data,
                                                              const uint8_t nrequired, const std::vector<fc::ecc::public_key> &keys_data) {
   script_builder builder;
   builder << op::IF;
   builder << user_key_data.serialize();
   builder << op::CHECKSIG;
   builder << op::ELSE;
   builder << static_cast<uint32_t>(nrequired);
   for (auto &key : keys_data) {
      builder << key.serialize();
   }
   builder << static_cast<uint32_t>(keys_data.size());
   builder << op::CHECKMULTISIG;
   builder << op::ENDIF;
   redeem_script_ = builder;
   fc::sha256 sh = fc::sha256::hash(redeem_script_.data(), redeem_script_.size());
   raw_address = bytes(sh.data(), sh.data() + sh.data_size());
}
void btc_one_or_m_of_n_multisig_address::create_witness_script() {
   script_builder builder;
   builder << op::_0;
   builder << fc::sha256::hash(redeem_script_.data(), redeem_script_.size());
   witness_script_ = builder;
}
void btc_one_or_m_of_n_multisig_address::create_segwit_address() {
   std::string hrp;
   switch (network_type) {
   case (network::mainnet):
      hrp = "bc";
      break;
   case (network::testnet):
      hrp = "tb";
      break;
   case (network::regtest):
      hrp = "bcrt";
      break;
   }
   fc::sha256 sh = fc::sha256::hash(&redeem_script_[0], redeem_script_.size());
   std::vector<uint8_t> hash_data(sh.data(), sh.data() + sh.data_size());
   address = segwit_addr::encode(hrp, 0, hash_data);
}

btc_one_or_weighted_multisig_address::btc_one_or_weighted_multisig_address(const fc::ecc::public_key &user_key_data,
                                                                           const std::vector<std::pair<fc::ecc::public_key, uint16_t>> &keys_data,
                                                                           bitcoin_address::network ntype) {
   network_type = ntype;
   create_redeem_script(user_key_data, keys_data);
   create_witness_script();
   create_segwit_address();
   type = payment_type::P2WSH;
}

void btc_one_or_weighted_multisig_address::create_redeem_script(const fc::ecc::public_key &user_key_data, const std::vector<std::pair<fc::ecc::public_key, uint16_t>> &keys_data) {
   script_builder builder;
   builder << user_key_data.serialize();
   builder << op::CHECKSIG;
   builder << op::IF;
   builder << op::_1;
   builder << op::ELSE;
   uint32_t total_weight = 0;
   builder << uint32_t(0);
   for (auto &p : keys_data) {
      total_weight += p.second;
      builder << op::SWAP;
      builder << p.first.serialize();
      builder << op::CHECKSIG;
      builder << op::IF;
      builder << uint32_t(p.second);
      builder << op::ADD;
      builder << op::ENDIF;
   }
   uint32_t threshold_weight = total_weight * 2 / 3 + 1;
   builder << threshold_weight;
   builder << op::GREATERTHANOREQUAL;
   builder << op::ENDIF;
   redeem_script_ = builder;
   fc::sha256 sh = fc::sha256::hash(redeem_script_.data(), redeem_script_.size());
   raw_address = bytes(sh.data(), sh.data() + sh.data_size());
}

void btc_one_or_weighted_multisig_address::create_witness_script() {
   script_builder builder;
   builder << op::_0;
   builder << fc::sha256::hash(redeem_script_.data(), redeem_script_.size());
   witness_script_ = builder;
}

void btc_one_or_weighted_multisig_address::create_segwit_address() {
   std::string hrp;
   switch (network_type) {
   case (network::mainnet):
      hrp = "bc";
      break;
   case (network::testnet):
      hrp = "tb";
      break;
   case (network::regtest):
      hrp = "bcrt";
      break;
   }
   fc::sha256 sh = fc::sha256::hash(&redeem_script_[0], redeem_script_.size());
   std::vector<uint8_t> hash_data(sh.data(), sh.data() + sh.data_size());
   address = segwit_addr::encode(hrp, 0, hash_data);
}

btc_timelocked_one_or_weighted_multisig_address::btc_timelocked_one_or_weighted_multisig_address(const fc::ecc::public_key &user_key_data, uint32_t latency, const std::vector<std::pair<fc::ecc::public_key, uint16_t>> &keys_data, bitcoin_address::network ntype) :
      btc_one_or_weighted_multisig_address(user_key_data, keys_data, ntype),
      latency_(latency) {
   create_redeem_script(user_key_data, keys_data);
   create_witness_script();
   create_segwit_address();
}

void btc_timelocked_one_or_weighted_multisig_address::create_redeem_script(const fc::ecc::public_key &user_key_data, const std::vector<std::pair<fc::ecc::public_key, uint16_t>> &keys_data) {
   script_builder builder;
   builder << user_key_data.serialize();
   builder << op::CHECKSIG;
   builder << op::IF;
   builder << uint32_t(latency_);
   builder << op::CHECKSEQUENCEVERIFY;
   builder << op::DROP;
   builder << op::_1;
   builder << op::ELSE;
   uint32_t total_weight = 0;
   builder << uint32_t(0);
   for (auto &p : keys_data) {
      total_weight += p.second;
      builder << op::SWAP;
      builder << p.first.serialize();
      builder << op::CHECKSIG;
      builder << op::IF;
      builder << uint32_t(p.second);
      builder << op::ADD;
      builder << op::ENDIF;
   }
   uint32_t threshold_weight = total_weight * 2 / 3 + 1;
   builder << threshold_weight;
   builder << op::GREATERTHANOREQUAL;
   builder << op::ENDIF;
   redeem_script_ = builder;
   fc::sha256 sh = fc::sha256::hash(redeem_script_.data(), redeem_script_.size());
   raw_address = bytes(sh.data(), sh.data() + sh.data_size());
}

}}} // namespace graphene::peerplays_sidechain::bitcoin
