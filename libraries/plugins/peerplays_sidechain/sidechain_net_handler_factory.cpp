#include <graphene/peerplays_sidechain/sidechain_net_handler_factory.hpp>

#include <graphene/peerplays_sidechain/sidechain_net_handler_bitcoin.hpp>
#include <graphene/peerplays_sidechain/sidechain_net_handler_hive.hpp>
#include <graphene/peerplays_sidechain/sidechain_net_handler_peerplays.hpp>

namespace graphene { namespace peerplays_sidechain {

sidechain_net_handler_factory::sidechain_net_handler_factory(peerplays_sidechain_plugin &_plugin) :
      plugin(_plugin) {
}

std::unique_ptr<sidechain_net_handler> sidechain_net_handler_factory::create_handler(sidechain_type sidechain, const boost::program_options::variables_map &options) const {
   switch (sidechain) {
   case sidechain_type::bitcoin: {
      return std::unique_ptr<sidechain_net_handler>(new sidechain_net_handler_bitcoin(plugin, options));
   }
   case sidechain_type::hive: {
      return std::unique_ptr<sidechain_net_handler>(new sidechain_net_handler_hive(plugin, options));
   }
   case sidechain_type::peerplays: {
      return std::unique_ptr<sidechain_net_handler>(new sidechain_net_handler_peerplays(plugin, options));
   }
   default:
      assert(false);
   }

   return nullptr;
}

}} // namespace graphene::peerplays_sidechain
