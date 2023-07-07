#pragma once

#include <graphene/app/plugin.hpp>

#include <graphene/chain/son_object.hpp>

namespace graphene { namespace peerplays_sidechain {

using namespace chain;

namespace detail {
class peerplays_sidechain_plugin_impl;
}

struct son_proposal_type {
   son_proposal_type(int op, sidechain_type sid, son_id_type son, object_id_type object) :
         op_type(op),
         sidechain(sid),
         son_id(son),
         object_id(object) {
   }
   int op_type;
   sidechain_type sidechain;
   son_id_type son_id;
   object_id_type object_id;
   bool operator<(const son_proposal_type &other) const {
      return std::tie(op_type, sidechain, son_id, object_id) < std::tie(other.op_type, other.sidechain, other.son_id, other.object_id);
   }
};

class peerplays_sidechain_plugin : public graphene::app::plugin {
public:
   peerplays_sidechain_plugin();
   virtual ~peerplays_sidechain_plugin();

   std::string plugin_name() const override;
   virtual void plugin_set_program_options(
         boost::program_options::options_description &cli,
         boost::program_options::options_description &cfg) override;
   virtual void plugin_initialize(const boost::program_options::variables_map &options) override;
   virtual void plugin_startup() override;
   virtual void plugin_shutdown() override;

   std::unique_ptr<detail::peerplays_sidechain_plugin_impl> my;

   std::set<chain::son_id_type> &get_sons();
   const son_id_type get_current_son_id(sidechain_type sidechain);
   const son_object get_current_son_object(sidechain_type sidechain);
   const son_object get_son_object(son_id_type son_id);
   bool is_active_son(sidechain_type sidechain, son_id_type son_id);
   bool is_son_deregistered(son_id_type son_id);
   fc::ecc::private_key get_private_key(son_id_type son_id);
   fc::ecc::private_key get_private_key(chain::public_key_type public_key);
   void log_son_proposal_retry(sidechain_type sidechain, int op_type, object_id_type object_id);
   bool can_son_participate(sidechain_type sidechain, int op_type, object_id_type object_id);
   std::map<sidechain_type, std::vector<std::string>> get_son_listener_log();
   optional<asset> estimate_withdrawal_transaction_fee(sidechain_type sidechain);
};

}} // namespace graphene::peerplays_sidechain
