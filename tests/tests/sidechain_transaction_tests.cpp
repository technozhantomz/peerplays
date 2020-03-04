#include <boost/test/unit_test.hpp>

#include "../common/database_fixture.hpp"

#include <graphene/chain/hardfork.hpp>
#include <graphene/chain/sidechain_transaction_object.hpp>
#include <graphene/chain/proposal_object.hpp>
#include <graphene/chain/son_object.hpp>
#include <graphene/peerplays_sidechain/defs.hpp>

using namespace graphene;
using namespace graphene::chain;
using namespace graphene::chain::test;

BOOST_FIXTURE_TEST_SUITE(sidechain_transaction_tests, database_fixture)

BOOST_AUTO_TEST_CASE(bitcoin_transaction_send_test)
{

   try
   {
      BOOST_TEST_MESSAGE("bitcoin_transaction_send_test");

      generate_blocks(HARDFORK_SON_TIME);
      generate_block();
      set_expiration(db, trx);

      ACTORS((alice)(bob));

      upgrade_to_lifetime_member(alice);
      upgrade_to_lifetime_member(bob);

      transfer(committee_account, alice_id, asset(500000 * GRAPHENE_BLOCKCHAIN_PRECISION));
      transfer(committee_account, bob_id, asset(500000 * GRAPHENE_BLOCKCHAIN_PRECISION));

      generate_block();
      set_expiration(db, trx);

      std::string test_url = "https://create_son_test";

      // create deposit vesting
      vesting_balance_id_type deposit_alice;
      {
         vesting_balance_create_operation op;
         op.creator = alice_id;
         op.owner = alice_id;
         op.amount = asset(500 * GRAPHENE_BLOCKCHAIN_PRECISION);
         op.balance_type = vesting_balance_type::son;
         op.policy = dormant_vesting_policy_initializer{};
         trx.operations.push_back(op);
         sign(trx, alice_private_key);
         processed_transaction ptx = PUSH_TX(db, trx, ~0);
         trx.clear();
         deposit_alice = ptx.operation_results[0].get<object_id_type>();
      }
      generate_block();
      set_expiration(db, trx);

      // create payment normal vesting
      vesting_balance_id_type payment_alice;
      {
         vesting_balance_create_operation op;
         op.creator = alice_id;
         op.owner = alice_id;
         op.amount = asset(500 * GRAPHENE_BLOCKCHAIN_PRECISION);
         op.balance_type = vesting_balance_type::normal;
         trx.operations.push_back(op);
         sign(trx, alice_private_key);
         processed_transaction ptx = PUSH_TX(db, trx, ~0);
         trx.clear();
         payment_alice = ptx.operation_results[0].get<object_id_type>();
      }

      generate_block();
      set_expiration(db, trx);

      // alice becomes son
      {
         flat_map<graphene::peerplays_sidechain::sidechain_type, string> sidechain_public_keys;
         sidechain_public_keys[graphene::peerplays_sidechain::sidechain_type::bitcoin] = "bitcoin address";

         son_create_operation op;
         op.owner_account = alice_id;
         op.url = test_url;
         op.deposit = deposit_alice;
         op.pay_vb = payment_alice;
         op.signing_key = alice_public_key;
         op.sidechain_public_keys = sidechain_public_keys;
         trx.operations.push_back(op);
         sign(trx, alice_private_key);
         PUSH_TX(db, trx, ~0);
         trx.clear();
      }
      generate_block();
      set_expiration(db, trx);

      // create deposit vesting
      vesting_balance_id_type deposit_bob;
      {
         vesting_balance_create_operation op;
         op.creator = bob_id;
         op.owner = bob_id;
         op.amount = asset(500 * GRAPHENE_BLOCKCHAIN_PRECISION);
         op.balance_type = vesting_balance_type::son;
         op.policy = dormant_vesting_policy_initializer{};
         trx.operations.push_back(op);
         sign(trx, bob_private_key);
         processed_transaction ptx = PUSH_TX(db, trx, ~0);
         trx.clear();
         deposit_bob = ptx.operation_results[0].get<object_id_type>();
      }
      generate_block();
      set_expiration(db, trx);

      // create payment normal vesting
      vesting_balance_id_type payment_bob;
      {
         vesting_balance_create_operation op;
         op.creator = bob_id;
         op.owner = bob_id;
         op.amount = asset(500 * GRAPHENE_BLOCKCHAIN_PRECISION);
         op.balance_type = vesting_balance_type::normal;
         trx.operations.push_back(op);
         sign(trx, bob_private_key);
         processed_transaction ptx = PUSH_TX(db, trx, ~0);
         trx.clear();
         payment_bob = ptx.operation_results[0].get<object_id_type>();
      }
      generate_block();
      set_expiration(db, trx);

      // bob becomes son
      {
         flat_map<graphene::peerplays_sidechain::sidechain_type, string> sidechain_public_keys;
         sidechain_public_keys[graphene::peerplays_sidechain::sidechain_type::bitcoin] = "bitcoin address";

         son_create_operation op;
         op.owner_account = bob_id;
         op.url = test_url;
         op.deposit = deposit_bob;
         op.pay_vb = payment_bob;
         op.signing_key = bob_public_key;
         op.sidechain_public_keys = sidechain_public_keys;
         trx.operations.push_back(op);
         sign(trx, alice_private_key);
         PUSH_TX(db, trx, ~0);
         trx.clear();
      }
      generate_block();

      generate_block();

      const auto& acc_idx = db.get_index_type<account_index>().indices().get<by_id>();
      auto acc_itr = acc_idx.find(GRAPHENE_SON_ACCOUNT);
      BOOST_REQUIRE(acc_itr != acc_idx.end());
      db.modify(*acc_itr, [&](account_object &obj) {
         obj.active.account_auths.clear();
         obj.active.add_authority(bob_id, 1);
         obj.active.add_authority(alice_id, 1);
         obj.active.weight_threshold = 2;
         obj.owner.account_auths.clear();
         obj.owner.add_authority(bob_id, 1);
         obj.owner.add_authority(alice_id, 1);
         obj.owner.weight_threshold = 2;
      });

      /*const auto &son_btc_account = db.create<account_object>([&](account_object &obj) {
         obj.name = "son_btc_account";
         obj.statistics = db.create<account_statistics_object>([&](account_statistics_object &acc_stat) { acc_stat.owner = obj.id; }).id;
         obj.membership_expiration_date = time_point_sec::maximum();
         obj.network_fee_percentage = GRAPHENE_DEFAULT_NETWORK_PERCENT_OF_FEE;
         obj.lifetime_referrer_fee_percentage = GRAPHENE_100_PERCENT - GRAPHENE_DEFAULT_NETWORK_PERCENT_OF_FEE;

         obj.owner.add_authority(bob_id, 1);
         obj.active.add_authority(bob_id, 1);
         obj.owner.add_authority(alice_id, 1);
         obj.active.add_authority(alice_id, 1);
         obj.active.weight_threshold = 2;
         obj.owner.weight_threshold = 2;
      });

      db.modify( db.get_global_properties(), [&]( global_property_object& _gpo )
      {
         _gpo.parameters.extensions.value.son_btc_account = son_btc_account.get_id();
         if( _gpo.pending_parameters )
            _gpo.pending_parameters->extensions.value.son_btc_account = son_btc_account.get_id();
      });*/

      generate_block();

      const global_property_object &gpo = db.get_global_properties();

      {
         BOOST_TEST_MESSAGE("Send bitcoin_transaction_send_operation");

         bitcoin_transaction_send_operation send_op;

         send_op.payer = GRAPHENE_SON_ACCOUNT;

         proposal_create_operation proposal_op;
         proposal_op.fee_paying_account = alice_id;
         proposal_op.proposed_ops.push_back(op_wrapper(send_op));
         uint32_t lifetime = (gpo.parameters.block_interval * gpo.active_witnesses.size()) * 3;
         proposal_op.expiration_time = time_point_sec(db.head_block_time().sec_since_epoch() + lifetime);

         trx.operations.push_back(proposal_op);
         set_expiration(db, trx);
         sign(trx, alice_private_key);
         PUSH_TX(db, trx, ~0);
         trx.clear();
      }
      generate_block();

      BOOST_TEST_MESSAGE("Check proposal results");

      const auto &idx = db.get_index_type<proposal_index>().indices().get<by_id>();
      BOOST_REQUIRE(idx.size() == 1);
      auto obj = idx.find(proposal_id_type(0));
      BOOST_REQUIRE(obj != idx.end());

      const auto& btidx = db.get_index_type<bitcoin_transaction_index>().indices().get<by_id>();
      BOOST_REQUIRE(btidx.size() == 0);

      std::vector<unsigned char> a1 = {'a', 'l', 'i', 'c', 'e', '1'};
      std::vector<unsigned char> a2 = {'a', 'l', 'i', 'c', 'e', '2'};
      std::vector<unsigned char> a3 = {'a',  'l', 'i', 'c', 'e', '3'};

      {
         BOOST_TEST_MESSAGE("Send bitcoin_transaction_sign_operation");

         bitcoin_transaction_sign_operation sign_op;

         sign_op.payer = alice_id;
         sign_op.proposal_id = proposal_id_type(0);
         sign_op.signatures.push_back(a1);
         sign_op.signatures.push_back(a2);
         sign_op.signatures.push_back(a3);

         trx.operations.push_back(sign_op);
         set_expiration(db, trx);
         sign(trx, alice_private_key);
         PUSH_TX(db, trx, ~0);
         trx.clear();
      }

      generate_block();

      BOOST_REQUIRE(idx.size() == 1);
      BOOST_REQUIRE(btidx.size() == 0);
      auto pobj = idx.find(proposal_id_type(0));
      BOOST_REQUIRE(pobj != idx.end());

      const auto& sidx = db.get_index_type<son_index>().indices().get<graphene::chain::by_account>();
      const auto son_obj1 = sidx.find( alice_id );
      BOOST_REQUIRE(son_obj1 != sidx.end());

      auto bitcoin_transaction_send_op = pobj->proposed_transaction.operations[0].get<bitcoin_transaction_send_operation>();
      BOOST_REQUIRE(bitcoin_transaction_send_op.signatures.size() == 1);
      BOOST_REQUIRE(bitcoin_transaction_send_op.signatures[son_obj1->id][0] == a1);
      BOOST_REQUIRE(bitcoin_transaction_send_op.signatures[son_obj1->id][1] == a2);
      BOOST_REQUIRE(bitcoin_transaction_send_op.signatures[son_obj1->id][2] == a3);

      std::vector<unsigned char> b1 = {'b', 'o', 'b', '1'};
      std::vector<unsigned char> b2 = {'b', 'o', 'b', '2'};
      std::vector<unsigned char> b3 = {'b', 'o', 'b', '3'};

      {
         BOOST_TEST_MESSAGE("Send bitcoin_transaction_sign_operation");

         bitcoin_transaction_sign_operation sign_op;

         sign_op.payer = bob_id;
         sign_op.proposal_id = proposal_id_type(0);
         sign_op.signatures.push_back(b1);
         sign_op.signatures.push_back(b2);
         sign_op.signatures.push_back(b3);

         trx.operations.push_back(sign_op);
         set_expiration(db, trx);
         sign(trx, bob_private_key);
         PUSH_TX(db, trx, ~0);
         trx.clear();
      }

      generate_block();

      BOOST_REQUIRE(idx.size() == 0);

      const auto son_obj2 = sidx.find( bob_id );
      BOOST_REQUIRE(son_obj2 != sidx.end());

      BOOST_REQUIRE(btidx.size() == 1);

      const auto btobj = btidx.find(bitcoin_transaction_id_type(0));
      BOOST_REQUIRE(btobj != btidx.end());

      BOOST_REQUIRE(btobj->processed == false);

      auto stats1 = son_obj1->statistics( db );
      auto stats2 = son_obj2->statistics( db );

      BOOST_REQUIRE(stats1.txs_signed == 1);
      BOOST_REQUIRE(stats2.txs_signed == 1);

      auto sigs = btobj->signatures;

      BOOST_REQUIRE(sigs[son_obj1->id][0] == a1);
      BOOST_REQUIRE(sigs[son_obj1->id][1] == a2);
      BOOST_REQUIRE(sigs[son_obj1->id][2] == a3);

      BOOST_REQUIRE(sigs[son_obj2->id][0] == b1);
      BOOST_REQUIRE(sigs[son_obj2->id][1] == b2);
      BOOST_REQUIRE(sigs[son_obj2->id][2] == b3);

      {
         BOOST_TEST_MESSAGE("Send bitcoin_send_transaction_process_operation");

         bitcoin_send_transaction_process_operation process_op;
         process_op.bitcoin_transaction_id = bitcoin_transaction_id_type(0);
         process_op.payer = GRAPHENE_SON_ACCOUNT;

         proposal_create_operation proposal_op;
         proposal_op.fee_paying_account = alice_id;
         proposal_op.proposed_ops.push_back(op_wrapper(process_op));
         uint32_t lifetime = (gpo.parameters.block_interval * gpo.active_witnesses.size()) * 3;
         proposal_op.expiration_time = time_point_sec(db.head_block_time().sec_since_epoch() + lifetime);

         trx.operations.push_back(proposal_op);
         set_expiration(db, trx);
         sign(trx, alice_private_key);
         PUSH_TX(db, trx, ~0);
         trx.clear();
      }

      generate_block();
      BOOST_REQUIRE(idx.size() == 1);
      obj = idx.find(proposal_id_type(1));
      BOOST_REQUIRE(obj != idx.end());


      {
         BOOST_TEST_MESSAGE("Send proposal_update_operation");

         proposal_update_operation puo;
         puo.fee_paying_account = bob_id;
         puo.proposal = obj->id;
         puo.active_approvals_to_add = { bob_id };

         trx.operations.push_back(puo);
         set_expiration(db, trx);
         sign(trx, bob_private_key);
         PUSH_TX(db, trx, ~0);
         trx.clear();
      }
      generate_block();
      BOOST_REQUIRE(idx.size() == 1);
      obj = idx.find(proposal_id_type(1));
      BOOST_REQUIRE(obj != idx.end());

      BOOST_REQUIRE(btobj != btidx.end());
      BOOST_REQUIRE(btobj->processed == false);

      {
         BOOST_TEST_MESSAGE("Send proposal_update_operation");

         proposal_update_operation puo;
         puo.fee_paying_account = alice_id;
         puo.proposal = obj->id;
         puo.active_approvals_to_add = { alice_id };

         trx.operations.push_back(puo);
         set_expiration(db, trx);
         sign(trx, alice_private_key);
         PUSH_TX(db, trx, ~0);
         trx.clear();
      }
      generate_block();
      BOOST_REQUIRE(idx.size() == 0);

      BOOST_REQUIRE(btobj != btidx.end());
      BOOST_REQUIRE(btobj->processed == true);
   }
   FC_LOG_AND_RETHROW()
}

BOOST_AUTO_TEST_SUITE_END()
