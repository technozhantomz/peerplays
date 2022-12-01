#include <graphene/peerplays_sidechain/ethereum/types.hpp>

#include <boost/property_tree/json_parser.hpp>
#include <boost/property_tree/ptree.hpp>

namespace graphene { namespace peerplays_sidechain { namespace ethereum {

signature::signature(const std::string &sign) {
   deserialize(sign);
}

std::string signature::serialize() const {
   boost::property_tree::ptree pt;
   pt.put("v", v);
   pt.put("r", r);
   pt.put("s", s);

   std::stringstream ss;
   boost::property_tree::json_parser::write_json(ss, pt);
   return ss.str();
}

void signature::deserialize(const std::string &raw_tx) {
   std::stringstream ss_tx(raw_tx);
   boost::property_tree::ptree tx_json;
   boost::property_tree::read_json(ss_tx, tx_json);

   if (tx_json.count("v"))
      v = tx_json.get<std::string>("v");
   if (tx_json.count("r"))
      r = tx_json.get<std::string>("r");
   if (tx_json.count("s"))
      s = tx_json.get<std::string>("s");
}

}}} // namespace graphene::peerplays_sidechain::ethereum
