#pragma once

#include <graphene/peerplays_sidechain/sidechain_net_handler.hpp>

#include <string>

#include <fc/signals.hpp>

namespace graphene { namespace peerplays_sidechain {

class sidechain_net_handler_peerplays : public sidechain_net_handler {
public:
    sidechain_net_handler_peerplays(peerplays_sidechain_plugin& _plugin, const boost::program_options::variables_map& options);
    virtual ~sidechain_net_handler_peerplays();

    void recreate_primary_wallet();
    void process_deposits();
    void process_deposit(const son_wallet_deposit_object& swdo);
    void process_withdrawals();
    void process_withdrawal(const son_wallet_withdraw_object& swwo);

    std::string create_multisignature_wallet( const std::vector<std::string> public_keys );
    std::string transfer( const std::string& from, const std::string& to, const uint64_t amount );
    std::string sign_transaction( const std::string& transaction );
    std::string send_transaction( const std::string& transaction );

private:

    void on_applied_block(const signed_block& b);

};

} } // graphene::peerplays_sidechain

