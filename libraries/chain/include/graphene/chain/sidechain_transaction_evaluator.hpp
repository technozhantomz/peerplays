#pragma once
#include <graphene/chain/evaluator.hpp>
#include <graphene/chain/protocol/sidechain_transaction.hpp>

namespace graphene { namespace chain {

class sidechain_transaction_create_evaluator : public evaluator<sidechain_transaction_create_evaluator>
{
public:
    typedef sidechain_transaction_create_operation operation_type;

    void_result do_evaluate(const sidechain_transaction_create_operation& o);
    object_id_type do_apply(const sidechain_transaction_create_operation& o);
};

class sidechain_transaction_sign_evaluator : public evaluator<sidechain_transaction_sign_evaluator>
{
public:
    typedef sidechain_transaction_sign_operation operation_type;

    void_result do_evaluate(const sidechain_transaction_sign_operation& o);
    object_id_type do_apply(const sidechain_transaction_sign_operation& o);
};

class sidechain_transaction_send_evaluator : public evaluator<sidechain_transaction_send_evaluator>
{
public:
    typedef sidechain_transaction_send_operation operation_type;

    void_result do_evaluate(const sidechain_transaction_send_operation& o);
    object_id_type do_apply(const sidechain_transaction_send_operation& o);
};

} } // namespace graphene::chain