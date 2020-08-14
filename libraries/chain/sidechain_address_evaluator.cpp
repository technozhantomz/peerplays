#include <graphene/chain/sidechain_address_evaluator.hpp>

#include <graphene/chain/database.hpp>
#include <graphene/chain/son_object.hpp>
#include <graphene/chain/sidechain_address_object.hpp>
#include <graphene/chain/hardfork.hpp>

namespace graphene { namespace chain {

void_result add_sidechain_address_evaluator::do_evaluate(const sidechain_address_add_operation& op)
{ try{
    FC_ASSERT( op.deposit_public_key.length() > 0 && op.deposit_address.length() == 0 && op.deposit_address_data.length() == 0, "User should add a valid deposit public key and a null deposit address");
    const auto& sdpke_idx = db().get_index_type<sidechain_address_index>().indices().get<by_sidechain_and_deposit_public_key_and_expires>();
    FC_ASSERT( sdpke_idx.find(boost::make_tuple(op.sidechain, op.deposit_public_key, time_point_sec::maximum())) == sdpke_idx.end(), "An active deposit key already exists" );
    return void_result();
} FC_CAPTURE_AND_RETHROW( (op) ) }

object_id_type add_sidechain_address_evaluator::do_apply(const sidechain_address_add_operation& op)
{ try {
    const auto &sidechain_addresses_idx = db().get_index_type<sidechain_address_index>().indices().get<by_account_and_sidechain_and_expires>();
    const auto &addr_itr = sidechain_addresses_idx.find(std::make_tuple(op.sidechain_address_account, op.sidechain, time_point_sec::maximum()));

    if (addr_itr != sidechain_addresses_idx.end())
    {
        db().modify(*addr_itr, [&](sidechain_address_object &sao) {
            sao.expires = db().head_block_time();
        });
    }

    const auto& new_sidechain_address_object = db().create<sidechain_address_object>( [&]( sidechain_address_object& obj ){
        obj.sidechain_address_account = op.sidechain_address_account;
        obj.sidechain = op.sidechain;
        obj.deposit_public_key = op.deposit_public_key;
        obj.deposit_address = "";
        obj.deposit_address_data = "";
        obj.withdraw_public_key = op.withdraw_public_key;
        obj.withdraw_address = op.withdraw_address;
        obj.valid_from = db().head_block_time();
        obj.expires = time_point_sec::maximum();
    });
    return new_sidechain_address_object.id;
} FC_CAPTURE_AND_RETHROW( (op) ) }

void_result update_sidechain_address_evaluator::do_evaluate(const sidechain_address_update_operation& op)
{ try {
    const auto& sidx = db().get_index_type<son_index>().indices().get<by_account>();
    const auto& son_obj = sidx.find(op.payer);
    FC_ASSERT( son_obj != sidx.end() && db().is_son_active(son_obj->id), "Non active SON trying to update deposit address object" );
    const auto& sdpke_idx = db().get_index_type<sidechain_address_index>().indices().get<by_sidechain_and_deposit_public_key_and_expires>();
    FC_ASSERT( op.deposit_address.valid() && op.deposit_public_key.valid() && op.deposit_address_data.valid(), "Update operation by SON is not valid");
    FC_ASSERT( (*op.deposit_address).length() > 0 && (*op.deposit_public_key).length() > 0 && (*op.deposit_address_data).length() > 0, "SON should create a valid deposit address with valid deposit public key");
    FC_ASSERT( sdpke_idx.find(boost::make_tuple(op.sidechain, *op.deposit_public_key, time_point_sec::maximum())) != sdpke_idx.end(), "Invalid update operation by SON" );
    FC_ASSERT( db().get(op.sidechain_address_id).sidechain_address_account == op.sidechain_address_account, "Invalid sidechain address account" );
    const auto& idx = db().get_index_type<sidechain_address_index>().indices().get<by_id>();
    FC_ASSERT( idx.find(op.sidechain_address_id) != idx.end(), "Invalid sidechain address ID" );
    return void_result();
} FC_CAPTURE_AND_RETHROW( (op) ) }

object_id_type update_sidechain_address_evaluator::do_apply(const sidechain_address_update_operation& op)
{ try {
    const auto& idx = db().get_index_type<sidechain_address_index>().indices().get<by_id>();
    auto itr = idx.find(op.sidechain_address_id);
    if(itr != idx.end())
    {
        // Case of change of Active SONs, store the outgoing address object with proper valid_from and expires updated
        if(itr->deposit_address.length() > 0) {
            const auto& new_sidechain_address_object = db().create<sidechain_address_object>( [&]( sidechain_address_object& obj ) {
                obj.sidechain_address_account = op.sidechain_address_account;
                obj.sidechain = op.sidechain;
                obj.deposit_public_key = *op.deposit_public_key;
                obj.deposit_address = itr->deposit_address;
                obj.deposit_address_data = itr->deposit_address_data;
                obj.withdraw_public_key = *op.withdraw_public_key;
                obj.withdraw_address = *op.withdraw_address;
                obj.valid_from = itr->valid_from;
                obj.expires = db().head_block_time();
            });
            db().modify(*itr, [&](sidechain_address_object &sao) {
                if(op.deposit_address.valid()) sao.deposit_address = *op.deposit_address;
                if(op.deposit_address_data.valid()) sao.deposit_address_data = *op.deposit_address_data;
                sao.valid_from = db().head_block_time();
            });
        } else {
            // Case of SON creating deposit address for a user input
            db().modify(*itr, [&op](sidechain_address_object &sao) {
                if(op.deposit_address.valid()) sao.deposit_address = *op.deposit_address;
                if(op.deposit_address_data.valid()) sao.deposit_address_data = *op.deposit_address_data;
            });
        }
    }
   return op.sidechain_address_id;
} FC_CAPTURE_AND_RETHROW( (op) ) }

void_result delete_sidechain_address_evaluator::do_evaluate(const sidechain_address_delete_operation& op)
{ try {
    FC_ASSERT(db().get(op.sidechain_address_id).sidechain_address_account == op.sidechain_address_account);
    const auto& idx = db().get_index_type<sidechain_address_index>().indices().get<by_id>();
    FC_ASSERT( idx.find(op.sidechain_address_id) != idx.end() );
    return void_result();
} FC_CAPTURE_AND_RETHROW( (op) ) }

void_result delete_sidechain_address_evaluator::do_apply(const sidechain_address_delete_operation& op)
{ try {
    const auto& idx = db().get_index_type<sidechain_address_index>().indices().get<by_id>();
    auto sidechain_address = idx.find(op.sidechain_address_id);
    if(sidechain_address != idx.end()) {
        db().modify(*sidechain_address, [&](sidechain_address_object &sao) {
            sao.expires = db().head_block_time();
        });
    }
    return void_result();
} FC_CAPTURE_AND_RETHROW( (op) ) }

} } // namespace graphene::chain
