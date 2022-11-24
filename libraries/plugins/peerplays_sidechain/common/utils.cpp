#include <graphene/peerplays_sidechain/common/utils.hpp>

#include <boost/archive/iterators/base64_from_binary.hpp>
#include <boost/archive/iterators/binary_from_base64.hpp>
#include <boost/archive/iterators/transform_width.hpp>

namespace graphene { namespace peerplays_sidechain {

const std::string base64_padding[] = {"", "==", "="};

std::string base64_encode(const std::string &s) {
   using namespace boost::archive::iterators;

   typedef base64_from_binary<transform_width<const char *, 6, 8>> base64_enc;

   std::stringstream os;
   std::copy(base64_enc(s.c_str()), base64_enc(s.c_str() + s.size()), std::ostream_iterator<char>(os));
   os << base64_padding[s.size() % 3];

   return os.str();
}

std::string base64_decode(const std::string &s) {
   using namespace boost::archive::iterators;

   typedef transform_width<binary_from_base64<const char *>, 8, 6> base64_dec;

   std::stringstream os;
   unsigned int size = s.size();
   if (size && s[size - 1] == '=') {
      --size;
      if (size && s[size - 1] == '=')
         --size;
   }
   if (size == 0)
      return std::string();

   std::copy(base64_dec(s.data()), base64_dec(s.data() + size), std::ostream_iterator<char>(os));

   return os.str();
}

std::string object_id_to_string(graphene::chain::object_id_type id) {
   std::string object_id = fc::to_string(id.space()) + "." +
                           fc::to_string(id.type()) + "." +
                           fc::to_string(id.instance());
   return object_id;
}

}} // namespace graphene::peerplays_sidechain
