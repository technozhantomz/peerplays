#pragma once

#include <map>
#include <string>
#include <vector>

#include <fc/api.hpp>

#include <graphene/peerplays_sidechain/peerplays_sidechain_plugin.hpp>

namespace graphene { namespace app {
class application;
}} // namespace graphene::app

namespace graphene { namespace peerplays_sidechain {

namespace detail {
class sidechain_api_impl;
}

class sidechain_api {
public:
   sidechain_api(app::application &_app);
   virtual ~sidechain_api();

   std::shared_ptr<detail::sidechain_api_impl> my;

   std::map<sidechain_type, std::vector<std::string>> get_son_listener_log();
   optional<asset> estimate_withdrawal_transaction_fee(sidechain_type sidechain);
};

}} // namespace graphene::peerplays_sidechain

FC_API(graphene::peerplays_sidechain::sidechain_api,
       (get_son_listener_log)(estimate_withdrawal_transaction_fee))
