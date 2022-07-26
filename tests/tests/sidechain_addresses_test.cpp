#include <boost/test/unit_test.hpp>

#include "../common/database_fixture.hpp"

#include <graphene/chain/hardfork.hpp>
#include <graphene/chain/son_object.hpp>
#include <graphene/chain/sidechain_address_object.hpp>
#include <graphene/chain/sidechain_defs.hpp>
#include <graphene/chain/vesting_balance_object.hpp>

using namespace graphene::chain;
using namespace graphene::chain::test;

BOOST_FIXTURE_TEST_SUITE( sidechain_addresses_tests, database_fixture )

BOOST_AUTO_TEST_CASE( sidechain_address_add_test ) {

   BOOST_TEST_MESSAGE("sidechain_address_add_test");

   generate_block();
   set_expiration(db, trx);

   ACTORS((alice));

   generate_block();
   set_expiration(db, trx);

   {
      BOOST_TEST_MESSAGE("Send sidechain_address_add_operation");

      sidechain_address_add_operation op;
      op.payer = alice_id;
      op.sidechain_address_account = alice_id;
      op.sidechain = sidechain_type::bitcoin;
      op.deposit_public_key = "deposit_public_key";
      op.withdraw_public_key = "withdraw_public_key";
      op.withdraw_address = "withdraw_address";

      trx.operations.push_back(op);
      sign(trx, alice_private_key);
      PUSH_TX(db, trx, ~0);
   }
   generate_block();

   BOOST_TEST_MESSAGE("Check sidechain_address_add_operation results");

   const auto& idx = db.get_index_type<sidechain_address_index>().indices().get<by_account_and_sidechain_and_expires>();
   BOOST_REQUIRE( idx.size() == 1 );
   auto obj = idx.find( boost::make_tuple( alice_id, sidechain_type::bitcoin, time_point_sec::maximum() ) );
   BOOST_REQUIRE( obj != idx.end() );
   BOOST_CHECK( obj->sidechain_address_account == alice_id );
   BOOST_CHECK( obj->sidechain == sidechain_type::bitcoin );
   BOOST_CHECK( obj->deposit_public_key == "deposit_public_key" );
   BOOST_CHECK( obj->deposit_address == "" );
   BOOST_CHECK( obj->deposit_address_data == "" );
   BOOST_CHECK( obj->withdraw_public_key == "withdraw_public_key" );
   BOOST_CHECK( obj->withdraw_address == "withdraw_address" );
}

BOOST_AUTO_TEST_CASE( sidechain_address_update_test ) {
   BOOST_TEST_MESSAGE("sidechain_address_update_test");
   generate_blocks(HARDFORK_SON_TIME);
   generate_block();

   INVOKE(sidechain_address_add_test);

   generate_block();

   //! ----- BEGIN CREATE SON bob -----

   ACTORS((bob));

   upgrade_to_lifetime_member(bob);

   transfer( committee_account, bob_id, asset( 1000*GRAPHENE_BLOCKCHAIN_PRECISION ) );

   set_expiration(db, trx);
   std::string test_url = "https://create_son_test";

   // create deposit vesting
   vesting_balance_id_type deposit;
   {
      vesting_balance_create_operation op;
      op.creator = bob_id;
      op.owner = bob_id;
      op.amount = asset(10*GRAPHENE_BLOCKCHAIN_PRECISION);
      op.balance_type = vesting_balance_type::son;
      op.policy = dormant_vesting_policy_initializer {};
      trx.clear();
      trx.operations.push_back(op);

      // amount in the son balance need to be at least 50
      GRAPHENE_REQUIRE_THROW( PUSH_TX( db, trx ), fc::exception );

      op.amount = asset(50*GRAPHENE_BLOCKCHAIN_PRECISION);
      trx.clear();

      trx.operations.push_back(op);
      processed_transaction ptx = PUSH_TX(db, trx, ~0);
      deposit = ptx.operation_results[0].get<object_id_type>();

      auto deposit_vesting = db.get<vesting_balance_object>(ptx.operation_results[0].get<object_id_type>());

      BOOST_CHECK_EQUAL(deposit(db).balance.amount.value, 50*GRAPHENE_BLOCKCHAIN_PRECISION);
      auto now = db.head_block_time();
      BOOST_CHECK_EQUAL(deposit(db).is_withdraw_allowed(now, asset(50*GRAPHENE_BLOCKCHAIN_PRECISION)), false); // cant withdraw
   }
   generate_block();
   set_expiration(db, trx);

   // create payment normal vesting
   vesting_balance_id_type payment ;
   {
      vesting_balance_create_operation op;
      op.creator = bob_id;
      op.owner = bob_id;
      op.amount = asset(1*GRAPHENE_BLOCKCHAIN_PRECISION);
      op.balance_type = vesting_balance_type::normal;
      op.policy = linear_vesting_policy_initializer {};
      op.validate();

      trx.clear();
      trx.operations.push_back(op);
      trx.validate();
      processed_transaction ptx = PUSH_TX(db, trx, ~0);
      trx.clear();
      payment = ptx.operation_results[0].get<object_id_type>();
   }

   generate_block();
   set_expiration(db, trx);

   // bob became son
   {
      flat_map<sidechain_type, string> sidechain_public_keys;
      sidechain_public_keys[sidechain_type::bitcoin] = "bitcoin address";
      sidechain_public_keys[sidechain_type::hive] = "hive address";

      son_create_operation op;
      op.owner_account = bob_id;
      op.url = test_url;
      op.deposit = deposit;
      op.pay_vb = payment;
      op.signing_key = bob_public_key;
      op.sidechain_public_keys = sidechain_public_keys;

      trx.clear();
      trx.operations.push_back(op);
      sign(trx, bob_private_key);
      PUSH_TX(db, trx, ~0);
      trx.clear();
   }
   generate_block();

   {
      const auto &idx = db.get_index_type<son_index>().indices().get<by_account>();
      BOOST_REQUIRE(idx.size() == 1);
      auto obj = idx.find(bob_id);
      BOOST_REQUIRE(obj != idx.end());
      BOOST_CHECK(obj->url == test_url);
      BOOST_CHECK(obj->signing_key == bob_public_key);
      BOOST_CHECK(obj->sidechain_public_keys.at(sidechain_type::bitcoin) == "bitcoin address");
      BOOST_CHECK(obj->deposit.instance == deposit.instance.value);
      BOOST_CHECK(obj->pay_vb.instance == payment.instance.value);
   }

   // Note payment time just to generate enough blocks to make budget
   const auto block_interval = db.get_global_properties().parameters.block_interval;
   auto pay_fee_time = db.head_block_time().sec_since_epoch();
   generate_block();
   // Do maintenance from the upcoming block
   auto schedule_maint = [&]()
   {
      db.modify( db.get_dynamic_global_properties(), [&]( dynamic_global_property_object& _dpo )
                {
                   _dpo.next_maintenance_time = db.head_block_time() + 1;
                } );
   };

   // Generate enough blocks to make budget
   while( db.head_block_time().sec_since_epoch() - pay_fee_time < 100 * block_interval )
   {
      generate_block();
   }

   // Enough blocks generated schedule maintenance now
   schedule_maint();
   // This block triggers maintenance
   generate_block();

   //! ----- END CREATE SON bob -----

   GET_ACTOR(alice);

   const auto& idx = db.get_index_type<sidechain_address_index>().indices().get<by_account_and_sidechain_and_expires>();
   BOOST_REQUIRE( idx.size() == 1 );
   auto obj = idx.find( boost::make_tuple( alice_id, sidechain_type::bitcoin, time_point_sec::maximum() ) );
   BOOST_REQUIRE( obj != idx.end() );
   std::string new_deposit_public_key = "deposit_public_key";
   std::string new_deposit_address = "new_deposit_address";
   std::string new_deposit_address_data = "new_deposit_address_data";
   std::string new_withdraw_public_key = "withdraw_public_key";
   std::string new_withdraw_address = "withdraw_address";

   generate_block();
   set_expiration(db, trx);
   {
      BOOST_TEST_MESSAGE("Send sidechain_address_update_operation");
      trx.clear();
      sidechain_address_update_operation op;
      op.payer = bob_id;
      op.sidechain_address_id = sidechain_address_id_type(0);
      op.sidechain_address_account = obj->sidechain_address_account;
      op.sidechain = obj->sidechain;
      op.deposit_public_key = new_deposit_public_key;
      op.deposit_address = new_deposit_address;
      op.deposit_address_data = new_deposit_address_data;
      op.withdraw_public_key = new_withdraw_public_key;
      op.withdraw_address = new_withdraw_address;
      trx.operations.push_back(op);
      sign(trx, bob_private_key);
      PUSH_TX(db, trx, ~0);
   }
   generate_block();
   {
      BOOST_TEST_MESSAGE("Check sidechain_address_update_operation results");

      const auto& idx = db.get_index_type<sidechain_address_index>().indices().get<by_account_and_sidechain_and_expires>();
      BOOST_REQUIRE( idx.size() == 1 );
      auto obj = idx.find( boost::make_tuple( alice_id, sidechain_type::bitcoin, time_point_sec::maximum() ) );
      BOOST_REQUIRE( obj != idx.end() );
      BOOST_CHECK( obj->sidechain_address_account == obj->sidechain_address_account );
      BOOST_CHECK( obj->sidechain == obj->sidechain );
      BOOST_CHECK( obj->deposit_public_key == new_deposit_public_key );
      BOOST_CHECK( obj->deposit_address == new_deposit_address );
      BOOST_CHECK( obj->deposit_address_data == new_deposit_address_data );
      BOOST_CHECK( obj->withdraw_public_key == new_withdraw_public_key );
      BOOST_CHECK( obj->withdraw_address == new_withdraw_address );
   }
}

BOOST_AUTO_TEST_CASE( sidechain_address_delete_test ) {

   BOOST_TEST_MESSAGE("sidechain_address_delete_test");

   INVOKE(sidechain_address_add_test);

   GET_ACTOR(alice);

   const auto& idx = db.get_index_type<sidechain_address_index>().indices().get<by_account_and_sidechain_and_expires>();
   BOOST_REQUIRE( idx.size() == 1 );
   auto obj = idx.find( boost::make_tuple( alice_id, sidechain_type::bitcoin, time_point_sec::maximum() ) );
   BOOST_REQUIRE( obj != idx.end() );

   {
      BOOST_TEST_MESSAGE("Send sidechain_address_delete_operation");

      sidechain_address_delete_operation op;
      op.payer = alice_id;
      op.sidechain_address_id = sidechain_address_id_type(0);
      op.sidechain_address_account = alice_id;
      op.sidechain = obj->sidechain;

      trx.operations.push_back(op);
      sign(trx, alice_private_key);
      PUSH_TX(db, trx, ~0);
   }
   time_point_sec now = db.head_block_time();
   generate_block();

   {
      BOOST_TEST_MESSAGE("Check sidechain_address_delete_operation results");

      const auto& idx = db.get_index_type<sidechain_address_index>().indices().get<by_account_and_sidechain_and_expires>();
      BOOST_REQUIRE( idx.size() == 1 );
      auto obj = idx.find( boost::make_tuple( alice_id, sidechain_type::bitcoin, time_point_sec::maximum() ) );
      BOOST_REQUIRE( obj == idx.end() );
      auto expired_obj = idx.find( boost::make_tuple( alice_id, sidechain_type::bitcoin, now ) );
      BOOST_REQUIRE( expired_obj != idx.end() );
   }
}

BOOST_AUTO_TEST_SUITE_END()

