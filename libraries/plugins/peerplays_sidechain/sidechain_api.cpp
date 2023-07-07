#include <graphene/peerplays_sidechain/sidechain_api.hpp>

namespace graphene { namespace peerplays_sidechain {

namespace detail {

class sidechain_api_impl {
public:
   sidechain_api_impl(app::application &app);
   virtual ~sidechain_api_impl();

   std::shared_ptr<graphene::peerplays_sidechain::peerplays_sidechain_plugin> get_plugin();

   std::map<sidechain_type, std::vector<std::string>> get_son_listener_log();
   optional<asset> estimate_withdrawal_transaction_fee(sidechain_type sidechain);

private:
   app::application &app;
};

sidechain_api_impl::sidechain_api_impl(app::application &_app) :
      app(_app) {
}

sidechain_api_impl::~sidechain_api_impl() {
}

std::shared_ptr<graphene::peerplays_sidechain::peerplays_sidechain_plugin> sidechain_api_impl::get_plugin() {
   return app.get_plugin<graphene::peerplays_sidechain::peerplays_sidechain_plugin>("peerplays_sidechain");
}

std::map<sidechain_type, std::vector<std::string>> sidechain_api_impl::get_son_listener_log() {
   return get_plugin()->get_son_listener_log();
}

optional<asset> sidechain_api_impl::estimate_withdrawal_transaction_fee(sidechain_type sidechain) {
   return get_plugin()->estimate_withdrawal_transaction_fee(sidechain);
}

} // namespace detail

sidechain_api::sidechain_api(graphene::app::application &_app) :
      my(std::make_shared<detail::sidechain_api_impl>(_app)) {
}

sidechain_api::~sidechain_api() {
}

std::map<sidechain_type, std::vector<std::string>> sidechain_api::get_son_listener_log() {
   return my->get_son_listener_log();
}

optional<asset> sidechain_api::estimate_withdrawal_transaction_fee(sidechain_type sidechain) {
   return my->estimate_withdrawal_transaction_fee(sidechain);
}

}} // namespace graphene::peerplays_sidechain
