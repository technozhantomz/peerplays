#include <graphene/peerplays_sidechain/bitcoin/bitcoin_script.hpp>
#include <graphene/peerplays_sidechain/bitcoin/serialize.hpp>
#include <fc/io/raw.hpp>

namespace graphene { namespace peerplays_sidechain { namespace bitcoin {

script_builder &script_builder::operator<<(op opcode) {
   const auto op_byte = static_cast<uint8_t>(opcode);
   if (op_byte < 0 || op_byte > 0xff)
      throw std::runtime_error("script_builder::operator<<(OP): invalid opcode");
   script.push_back(op_byte);
   return *this;
}

script_builder &script_builder::operator<<(uint32_t number) {
   if (number == 0) {
      script.push_back(static_cast<uint8_t>(op::_0));
   } else if (number <= 16) {
      script.push_back(static_cast<uint8_t>(op::_1) + number - 1);
   } else {
      bytes pack_buf;
      while (number) {
         pack_buf.push_back(number & 0xff);
         number >>= 8;
      }
      // - If the most significant byte is >= 0x80 and the value is positive, push a
      // new zero-byte to make the significant byte < 0x80 again. So,  the result can
      // be 5 bytes max.
      if (pack_buf.back() & 0x80)
         pack_buf.push_back(0);
      *this << pack_buf;
   }

   return *this;
}

script_builder &script_builder::operator<<(size_t size) {
   write_compact_size(script, size);
   return *this;
}

script_builder &script_builder::operator<<(const bytes &sc) {
   write_compact_size(script, sc.size());
   script.insert(script.end(), sc.begin(), sc.end());
   return *this;
}

script_builder &script_builder::operator<<(const fc::sha256 &hash) {
   write_compact_size(script, hash.data_size());
   script.insert(script.end(), hash.data(), hash.data() + hash.data_size());
   return *this;
}

script_builder &script_builder::operator<<(const fc::ripemd160 &hash) {
   write_compact_size(script, hash.data_size());
   script.insert(script.end(), hash.data(), hash.data() + hash.data_size());
   return *this;
}

script_builder &script_builder::operator<<(const fc::ecc::public_key_data &pubkey_data) {
   write_compact_size(script, pubkey_data.size());
   script.insert(script.end(), pubkey_data.begin(), pubkey_data.begin() + pubkey_data.size());
   return *this;
}

}}} // namespace graphene::peerplays_sidechain::bitcoin
