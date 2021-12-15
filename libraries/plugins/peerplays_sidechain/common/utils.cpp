#include <graphene/peerplays_sidechain/common/utils.hpp>

std::string object_id_to_string(graphene::chain::object_id_type id) {
   std::string object_id = fc::to_string(id.space()) + "." +
                           fc::to_string(id.type()) + "." +
                           fc::to_string(id.instance());
   return object_id;
}
