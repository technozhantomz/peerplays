#pragma once
#include <graphene/chain/evaluator.hpp>
#include <graphene/chain/protocol/sidechain_transaction.hpp>

namespace graphene { namespace chain {

class bitcoin_transaction_send_evaluator : public evaluator<bitcoin_transaction_send_evaluator>
{
public:
    typedef bitcoin_transaction_send_operation operation_type;

    void_result do_evaluate(const bitcoin_transaction_send_operation& o);
    object_id_type do_apply(const bitcoin_transaction_send_operation& o);
};

class bitcoin_transaction_sign_evaluator : public evaluator<bitcoin_transaction_sign_evaluator>
{
public:
    typedef bitcoin_transaction_sign_operation operation_type;

    void_result do_evaluate(const bitcoin_transaction_sign_operation& o);
    object_id_type do_apply(const bitcoin_transaction_sign_operation& o);
    void update_proposal( const bitcoin_transaction_sign_operation& o );
};

class bitcoin_send_transaction_process_evaluator : public evaluator<bitcoin_send_transaction_process_evaluator>
{
public:
    typedef bitcoin_send_transaction_process_operation operation_type;

    void_result do_evaluate(const bitcoin_send_transaction_process_operation& o);
    object_id_type do_apply(const bitcoin_send_transaction_process_operation& o);
};

} } // namespace graphene::chain