#pragma once

#include <boost/multiprecision/cpp_int.hpp>
#include <string>
#include <vector>

namespace graphene { namespace peerplays_sidechain { namespace ethereum {

class base_encoder {
public:
   static std::string encode_uint256(boost::multiprecision::uint256_t value);
   static std::string encode_address(const std::string &value);
   static std::string encode_string(const std::string &value);
};

class update_owners_encoder {
public:
   const std::string function_signature = "23ab6adf"; //! updateOwners((address,uint256)[],string)

   std::string encode(const std::vector<std::pair<std::string, uint16_t>> &owners_weights, const std::string &object_id) const;
};

class withdrawal_encoder {
public:
   const std::string function_signature = "e088747b"; //! withdraw(address,uint256,string)

   std::string encode(const std::string &to, boost::multiprecision::uint256_t amount, const std::string &object_id) const;
};

class rlp_encoder {
public:
   static std::string encode(const std::string &s);
   static std::string encode_length(int len, int offset);
   static std::string hex2bytes(const std::string &s);

private:
   static std::string encode_rlp(const std::string &s);
   static int char2int(char input);
   static void hex2bin(const char *src, char *target);
};

/*class ethereum_function_call_encoder {
public:
   enum operation_t {
      OPERATION_CALL,
      OPERATION_DELEGATE_CALL
   };

   static constexpr const char *const default_prev_addr = "0000000000000000000000000000000000000001";

   std::string encode_function_signature(const std::string &function_signature);
   std::string encode_address(const std::string &addr);
   std::string encode_uint256(const std::string &value);
   std::string encode_uint8(uint8_t value);
   std::string encode_bytes(const std::string &values);
};

class safe_transaction_encoder {
public:
   static constexpr const char *const default_safe_tx_gas = "0";
   static constexpr const char *const default_data_gas = "0";
   static constexpr const char *const default_gas_price = "0";
   static constexpr const char *const default_gas_token = "0000000000000000000000000000000000000000";
   static constexpr const char *const default_refund_receiver = "0000000000000000000000000000000000000000";

   std::string create_safe_address(const std::vector<std::string> &owner_addresses, uint32_t threshold);
   std::string build_transaction(const std::string &safe_account_addr, const std::string &value, const std::string &data, uint8_t operation, const std::string &safeTxGas, const std::string &dataGas, const std::string &gasPrice, const std::string &gasToken, const std::string &refundReceiver);

private:
   ethereum_function_call_encoder m_ethereum_function_call_encoder;
};*/

}}} // namespace graphene::peerplays_sidechain::ethereum
