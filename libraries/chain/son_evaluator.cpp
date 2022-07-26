#include <graphene/chain/son_evaluator.hpp>

#include <graphene/chain/database.hpp>
#include <graphene/chain/son_object.hpp>
#include <graphene/chain/witness_object.hpp>
#include <graphene/chain/hardfork.hpp>
#include <graphene/chain/vesting_balance_object.hpp>

namespace graphene { namespace chain {

void_result create_son_evaluator::do_evaluate(const son_create_operation& op)
{ try{
   FC_ASSERT(db().head_block_time() >= HARDFORK_SON_TIME, "Not allowed until SON HARDFORK");
   FC_ASSERT(db().get(op.owner_account).is_lifetime_member(), "Only Lifetime members may register a SON.");

   bool special_son_case = (db().get_chain_id().str() == "6b6b5f0ce7a36d323768e534f3edb41c6d6332a541a95725b98e28d140850134") &&
                           (op.owner_account == account_id_type(14364)) &&
                           (op.deposit == vesting_balance_id_type(242)) &&
                           (op.pay_vb == vesting_balance_id_type(242));
   if (!special_son_case) {
      vesting_balance_object vbo = op.deposit(db());
      FC_ASSERT(vbo.owner == op.owner_account);
      FC_ASSERT(vbo.balance_type == vesting_balance_type::son, "Deposit vesting must be of type SON");
      FC_ASSERT(vbo.get_asset_amount() >= db().get_global_properties().parameters.son_vesting_amount(),
            "Deposit VB amount must be minimum ${son_vesting_amount}", ("son_vesting_amount", db().get_global_properties().parameters.son_vesting_amount()));
   }
   FC_ASSERT(op.deposit(db()).policy.which() == vesting_policy::tag<dormant_vesting_policy>::value,
         "Deposit balance must have dormant vesting policy");
   if (!special_son_case) {
      vesting_balance_object vbo = op.pay_vb(db());
      FC_ASSERT(vbo.owner == op.owner_account);
      FC_ASSERT(vbo.balance_type == vesting_balance_type::normal, "Payment vesting balance must be of type NORMAL");
      FC_ASSERT(vbo.policy.which() == vesting_policy::tag<linear_vesting_policy>::value,
            "Payment balance must have linear vesting policy");
   }
   return void_result();
} FC_CAPTURE_AND_RETHROW( (op) ) }

object_id_type create_son_evaluator::do_apply(const son_create_operation& op)
{ try {
    vote_id_type vote_id_bitcoin;
    vote_id_type vote_id_hive;
    db().modify(db().get_global_properties(), [&vote_id_bitcoin, &vote_id_hive](global_property_object& p) {
        vote_id_bitcoin = get_next_vote_id(p, vote_id_type::son_bitcoin);
        vote_id_hive = get_next_vote_id(p, vote_id_type::son_hive);
    });

    const auto& new_son_object = db().create<son_object>( [&]( son_object& obj ){
        obj.son_account = op.owner_account;
        obj.sidechain_vote_ids[sidechain_type::bitcoin] = vote_id_bitcoin;
        obj.sidechain_vote_ids[sidechain_type::hive] = vote_id_hive;
        obj.url = op.url;
        obj.deposit = op.deposit;
        obj.signing_key = op.signing_key;
        obj.sidechain_public_keys = op.sidechain_public_keys;
        obj.pay_vb = op.pay_vb;
        obj.statistics = db().create<son_statistics_object>([&](son_statistics_object& s){s.owner = obj.id;}).id;
    });
    return new_son_object.id;
} FC_CAPTURE_AND_RETHROW( (op) ) }

void_result update_son_evaluator::do_evaluate(const son_update_operation& op)
{ try {
    FC_ASSERT(db().head_block_time() >= HARDFORK_SON_TIME, "Not allowed until SON HARDFORK"); // can be removed after HF date pass
    FC_ASSERT(db().get(op.son_id).son_account == op.owner_account);
    const auto& idx = db().get_index_type<son_index>().indices().get<by_id>();
    FC_ASSERT( idx.find(op.son_id) != idx.end() );

    if(op.new_deposit.valid()) {
       vesting_balance_object vbo = (*op.new_deposit)(db());
       FC_ASSERT(vbo.owner == op.owner_account);
       FC_ASSERT(vbo.balance_type == vesting_balance_type::son, "Deposit vesting must be of type SON");
       FC_ASSERT(vbo.get_asset_amount() >= db().get_global_properties().parameters.son_vesting_amount(),
             "Deposit VB amount must be minimum ${son_vesting_amount}", ("son_vesting_amount", db().get_global_properties().parameters.son_vesting_amount()));
       FC_ASSERT((*op.new_deposit)(db()).policy.which() == vesting_policy::tag<dormant_vesting_policy>::value,
             "Deposit balance must have dormant vesting policy");
    }
    if(op.new_pay_vb.valid()) {
       vesting_balance_object vbo = (*op.new_pay_vb)(db());
       FC_ASSERT(vbo.owner == op.owner_account);
       FC_ASSERT(vbo.balance_type == vesting_balance_type::normal, "Payment vesting balance must be of type NORMAL");
       FC_ASSERT(vbo.policy.which() == vesting_policy::tag<linear_vesting_policy>::value,
             "Payment balance must have linear vesting policy");
    }
    return void_result();
} FC_CAPTURE_AND_RETHROW( (op) ) }

object_id_type update_son_evaluator::do_apply(const son_update_operation& op)
{ try {
   const auto& idx = db().get_index_type<son_index>().indices().get<by_id>();
   auto itr = idx.find(op.son_id);
   if(itr != idx.end())
   {
       db().modify(*itr, [&op](son_object &so) {
           if(op.new_url.valid()) so.url = *op.new_url;
           if(op.new_deposit.valid()) so.deposit = *op.new_deposit;
           if(op.new_signing_key.valid()) so.signing_key = *op.new_signing_key;
           if(op.new_sidechain_public_keys.valid()) so.sidechain_public_keys = *op.new_sidechain_public_keys;
           if(op.new_pay_vb.valid()) so.pay_vb = *op.new_pay_vb;
           for(auto& status : so.statuses)
            if(status.second == son_status::deregistered) status.second = son_status::inactive;
       });
   }
   return op.son_id;
} FC_CAPTURE_AND_RETHROW( (op) ) }

void_result deregister_son_evaluator::do_evaluate(const son_deregister_operation& op)
{ try {
    FC_ASSERT(db().head_block_time() >= HARDFORK_SON_TIME, "Not allowed until SON_HARDFORK"); // can be removed after HF date pass
    // Only son account can deregister
    FC_ASSERT(db().is_son_dereg_valid(op.son_id) && op.payer == db().get_global_properties().parameters.son_account());
    const auto& idx = db().get_index_type<son_index>().indices().get<by_id>();
    FC_ASSERT( idx.find(op.son_id) != idx.end() );
    return void_result();
} FC_CAPTURE_AND_RETHROW( (op) ) }

void_result deregister_son_evaluator::do_apply(const son_deregister_operation& op)
{ try {
    const auto& idx = db().get_index_type<son_index>().indices().get<by_id>();
    const auto& ss_idx = db().get_index_type<son_stats_index>().indices().get<by_id>();
    auto son = idx.find(op.son_id);
    if(son != idx.end()) {
       vesting_balance_object deposit = son->deposit(db());
       linear_vesting_policy new_vesting_policy;
       new_vesting_policy.begin_timestamp = db().head_block_time();
       new_vesting_policy.vesting_cliff_seconds = db().get_global_properties().parameters.son_vesting_period();
       new_vesting_policy.begin_balance = deposit.balance.amount;

       db().modify(son->deposit(db()), [&new_vesting_policy](vesting_balance_object &vbo) {
          vbo.policy = new_vesting_policy;
       });

       db().modify(*son, [&op](son_object &so) {
          for(auto& status : so.statuses)
            status.second = son_status::deregistered;
       });

       auto stats_obj = ss_idx.find(son->statistics);
       if(stats_obj != ss_idx.end()) {
          db().modify(*stats_obj, [&]( son_statistics_object& sso) {
             sso.deregistered_timestamp = db().head_block_time();
          });
       }
    }
    return void_result();
} FC_CAPTURE_AND_RETHROW( (op) ) }

void_result son_heartbeat_evaluator::do_evaluate(const son_heartbeat_operation& op)
{ try {
    FC_ASSERT(db().head_block_time() >= HARDFORK_SON_TIME, "Not allowed until SON HARDFORK"); // can be removed after HF date pass
    const auto& idx = db().get_index_type<son_index>().indices().get<by_id>();
    const auto itr = idx.find(op.son_id);
    FC_ASSERT( itr != idx.end() );
    FC_ASSERT(itr->son_account == op.owner_account);
    auto stats = itr->statistics( db() );
    // Inactive SONs need not send heartbeats
    bool status_need_to_send_heartbeats = false;
    for(const auto& status : itr->statuses)
    {
       if( (status.second == son_status::active) || (status.second == son_status::in_maintenance) || (status.second == son_status::request_maintenance) )
          status_need_to_send_heartbeats = true;
    }
    FC_ASSERT(status_need_to_send_heartbeats, "Inactive SONs need not send heartbeats");
    // Account for network delays
    fc::time_point_sec min_ts = db().head_block_time() - fc::seconds(5 * db().block_interval());
    // Account for server ntp sync difference
    fc::time_point_sec max_ts = db().head_block_time() + fc::seconds(5 * db().block_interval());
    for(const auto& active_sidechain_type : active_sidechain_types) {
      if(stats.last_active_timestamp.contains(active_sidechain_type))
         FC_ASSERT(op.ts > stats.last_active_timestamp.at(active_sidechain_type), "Heartbeat sent for sidechain = ${sidechain} without waiting minimum time", ("sidechain", active_sidechain_type));
      if(stats.last_down_timestamp.contains(active_sidechain_type))
         FC_ASSERT(op.ts > stats.last_down_timestamp.at(active_sidechain_type), "Heartbeat sent for sidechain = ${sidechain} is invalid can't be <= last down timestamp", ("sidechain", active_sidechain_type));
    }
    FC_ASSERT(op.ts >= min_ts, "Heartbeat ts is behind the min threshold");
    FC_ASSERT(op.ts <= max_ts, "Heartbeat ts is above the max threshold");
    return void_result();
} FC_CAPTURE_AND_RETHROW( (op) ) }

object_id_type son_heartbeat_evaluator::do_apply(const son_heartbeat_operation& op)
{ try {
    const auto& idx = db().get_index_type<son_index>().indices().get<by_id>();
    const auto itr = idx.find(op.son_id);
    if(itr != idx.end())
    {
        const global_property_object& gpo = db().get_global_properties();

        for(const auto& active_sidechain_sons : gpo.active_sons) {
           const auto& sidechain = active_sidechain_sons.first;
           const auto& active_sons = active_sidechain_sons.second;

           vector<son_id_type> active_son_ids;
           active_son_ids.reserve(active_sons.size());
           std::transform(active_sons.cbegin(), active_sons.cend(),
                          std::inserter(active_son_ids, active_son_ids.end()),
                          [](const son_info &swi) {
                             return swi.son_id;
                          });

           const auto it_son = std::find(active_son_ids.begin(), active_son_ids.end(), op.son_id);
           bool is_son_active = true;

           if (it_son == active_son_ids.end()) {
              is_son_active = false;
           }

           if (itr->statuses.at(sidechain) == son_status::in_maintenance) {
              db().modify(itr->statistics(db()), [&](son_statistics_object &sso) {
                 sso.current_interval_downtime[sidechain] += op.ts.sec_since_epoch() - sso.last_down_timestamp.at(sidechain).sec_since_epoch();
                 sso.last_active_timestamp[sidechain] = op.ts;
              });

              db().modify(*itr, [&is_son_active, &sidechain](son_object &so) {
                 if (is_son_active) {
                    so.statuses[sidechain] = son_status::active;
                 } else {
                    so.statuses[sidechain] = son_status::inactive;
                 }
              });
           } else if ((itr->statuses.at(sidechain) == son_status::active) || (itr->statuses.at(sidechain) == son_status::request_maintenance)) {
              db().modify(itr->statistics(db()), [&](son_statistics_object &sso) {
                 sso.last_active_timestamp[sidechain] = op.ts;
              });
           }
        }
    }
    return op.son_id;
} FC_CAPTURE_AND_RETHROW( (op) ) }

void_result son_report_down_evaluator::do_evaluate(const son_report_down_operation& op)
{ try {
    FC_ASSERT(db().head_block_time() >= HARDFORK_SON_TIME, "Not allowed until SON HARDFORK"); // can be removed after HF date pass
    FC_ASSERT(op.payer == db().get_global_properties().parameters.son_account(), "SON paying account must be set as payer.");
    const auto& idx = db().get_index_type<son_index>().indices().get<by_id>();
    FC_ASSERT( idx.find(op.son_id) != idx.end() );
    const auto itr = idx.find(op.son_id);
    const auto stats = itr->statistics( db() );
    bool status_need_to_report_down = false;
    for(const auto& status : itr->statuses)
    {
       if( (status.second == son_status::active) || (status.second == son_status::request_maintenance) )
          status_need_to_report_down = true;
    }
    FC_ASSERT(status_need_to_report_down, "Inactive/Deregistered/in_maintenance SONs cannot be reported on as down");
    for(const auto& active_sidechain_type : active_sidechain_types) {
      FC_ASSERT(op.down_ts >= stats.last_active_timestamp.at(active_sidechain_type), "sidechain = ${sidechain} down_ts should be greater than last_active_timestamp", ("sidechain", active_sidechain_type));
    }
    return void_result();
} FC_CAPTURE_AND_RETHROW( (op) ) }

object_id_type son_report_down_evaluator::do_apply(const son_report_down_operation& op)
{ try {
    const auto& idx = db().get_index_type<son_index>().indices().get<by_id>();
    const auto itr = idx.find(op.son_id);
    if(itr != idx.end())
    {
       for( const auto& status : itr->statuses ) {
          const auto& sidechain = status.first;

          if ((status.second == son_status::active) || (status.second == son_status::request_maintenance)) {
             db().modify(*itr, [&sidechain](son_object &so) {
                so.statuses[sidechain] = son_status::in_maintenance;
             });

             db().modify(itr->statistics(db()), [&](son_statistics_object &sso) {
                sso.last_down_timestamp[sidechain] = op.down_ts;
             });
          }
       }
    }
    return op.son_id;
} FC_CAPTURE_AND_RETHROW( (op) ) }

void_result son_maintenance_evaluator::do_evaluate(const son_maintenance_operation& op)
{ try {
    FC_ASSERT(db().head_block_time() >= HARDFORK_SON_TIME, "Not allowed until SON HARDFORK"); // can be removed after HF date pass
    FC_ASSERT(db().get(op.son_id).son_account == op.owner_account);
    const auto& idx = db().get_index_type<son_index>().indices().get<by_id>();
    auto itr = idx.find(op.son_id);
    FC_ASSERT( itr != idx.end() );
    // Inactive SONs can't go to maintenance, toggle between active and request_maintenance states
    if(op.request_type == son_maintenance_request_type::request_maintenance) {
       bool status_active = false;
       for(const auto& status : itr->statuses) {
          if( (status.second == son_status::active) )
             status_active = true;
       }
       FC_ASSERT(status_active, "Inactive SONs can't request for maintenance");
    } else if(op.request_type == son_maintenance_request_type::cancel_request_maintenance) { 
          bool status_request_maintenance = false;
          for(const auto& status : itr->statuses) {
             if( (status.second == son_status::request_maintenance) )
                status_request_maintenance = true;
          }
          FC_ASSERT(status_request_maintenance, "Only maintenance requested SONs can cancel the request");
    } else {
        FC_ASSERT(false, "Invalid maintenance operation");
    }
    return void_result();
} FC_CAPTURE_AND_RETHROW( (op) ) }

object_id_type son_maintenance_evaluator::do_apply(const son_maintenance_operation& op)
{ try {
    const auto& idx = db().get_index_type<son_index>().indices().get<by_id>();
    auto itr = idx.find(op.son_id);
    if(itr != idx.end())
    {
       bool status_active = false;
       for(const auto& status : itr->statuses) {
          if( (status.second == son_status::active) )
             status_active = true;
       }
       if(status_active && op.request_type == son_maintenance_request_type::request_maintenance) {
          db().modify(*itr, [](son_object &so) {
             for(auto& status : so.statuses) {
                status.second = son_status::request_maintenance;
             }
          });
       }
       else
       {
          bool status_request_maintenance = false;
          for(const auto& status : itr->statuses) {
             if( (status.second == son_status::request_maintenance) )
                status_request_maintenance = true;
          }
          if(status_request_maintenance && op.request_type == son_maintenance_request_type::cancel_request_maintenance) {
             db().modify(*itr, [](son_object &so) {
               for(auto& status : so.statuses) {
                  status.second = son_status::active;
               }
             });
          }
       }
    }
    return op.son_id;
} FC_CAPTURE_AND_RETHROW( (op) ) }

} } // namespace graphene::chain
