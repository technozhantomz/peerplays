#pragma once

#include <graphene/chain/sidechain_defs.hpp>
#include <graphene/peerplays_sidechain/peerplays_sidechain_plugin.hpp>
#include <graphene/peerplays_sidechain/sidechain_net_handler.hpp>

#include <vector>

#include <boost/program_options.hpp>

namespace graphene { namespace peerplays_sidechain {

class sidechain_net_handler_factory {
public:
   sidechain_net_handler_factory(peerplays_sidechain_plugin &_plugin);

   std::unique_ptr<sidechain_net_handler> create_handler(sidechain_type sidechain, const boost::program_options::variables_map &options) const;

private:
   peerplays_sidechain_plugin &plugin;
};

}} // namespace graphene::peerplays_sidechain
