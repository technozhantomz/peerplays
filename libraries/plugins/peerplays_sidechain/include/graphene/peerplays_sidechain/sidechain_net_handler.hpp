#pragma once

#include <mutex>
#include <vector>

#include <boost/program_options.hpp>

#include <graphene/chain/proposal_object.hpp>
#include <graphene/chain/sidechain_address_object.hpp>
#include <graphene/chain/sidechain_transaction_object.hpp>
#include <graphene/chain/son_wallet_deposit_object.hpp>
#include <graphene/chain/son_wallet_withdraw_object.hpp>
#include <graphene/peerplays_sidechain/defs.hpp>
#include <graphene/peerplays_sidechain/peerplays_sidechain_plugin.hpp>

namespace graphene { namespace peerplays_sidechain {

class sidechain_net_handler {
protected:
   sidechain_net_handler(sidechain_type _sidechain, peerplays_sidechain_plugin &_plugin, const boost::program_options::variables_map &options);

public:
   virtual ~sidechain_net_handler();

   sidechain_type get_sidechain() const;
   std::vector<std::string> get_sidechain_deposit_addresses() const;
   std::vector<std::string> get_sidechain_withdraw_addresses() const;
   std::vector<sidechain_transaction_object> get_sidechain_transaction_objects(sidechain_transaction_status status) const;
   std::string get_private_key(std::string public_key) const;

   bool proposal_exists(int32_t operation_tag, const object_id_type &object_id, boost::optional<chain::operation &> proposal_op = boost::none);
   bool signer_expected(const sidechain_transaction_object &sto, son_id_type signer);
   bool approve_proposal(const proposal_id_type &proposal_id, const son_id_type &son_id);
   void sidechain_event_data_received(const sidechain_event_data &sed);

   void process_proposals();
   void process_active_sons_change();
   void create_deposit_addresses();
   void process_deposits();
   void process_withdrawals();
   void process_sidechain_transactions();
   void send_sidechain_transactions();
   void settle_sidechain_transactions();

   virtual bool process_proposal(const proposal_object &po) = 0;
   virtual void process_primary_wallet() = 0;
   virtual void process_sidechain_addresses() = 0;
   virtual bool process_deposit(const son_wallet_deposit_object &swdo) = 0;
   virtual bool process_withdrawal(const son_wallet_withdraw_object &swwo) = 0;
   virtual std::string process_sidechain_transaction(const sidechain_transaction_object &sto) = 0;
   virtual std::string send_sidechain_transaction(const sidechain_transaction_object &sto) = 0;
   virtual bool settle_sidechain_transaction(const sidechain_transaction_object &sto, asset &settle_amount) = 0;

   void add_to_son_listener_log(std::string trx_id);
   std::vector<std::string> get_son_listener_log();
   virtual optional<asset> estimate_withdrawal_transaction_fee() const = 0;

protected:
   const sidechain_type sidechain;
   peerplays_sidechain_plugin &plugin;
   graphene::chain::database &database;

   bool debug_rpc_calls;
   bool use_bitcoind_client;

   std::map<std::string, std::string> private_keys;

   std::vector<std::string> son_listener_log;
   std::mutex son_listener_log_mutex;

   void on_applied_block(const signed_block &b);

private:
};

}} // namespace graphene::peerplays_sidechain
