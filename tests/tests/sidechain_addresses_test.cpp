#include <boost/test/unit_test.hpp>

#include "../common/database_fixture.hpp"

#include <graphene/chain/hardfork.hpp>
#include <graphene/chain/son_object.hpp>
#include <graphene/chain/sidechain_address_object.hpp>
#include <graphene/chain/sidechain_defs.hpp>

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

   GET_ACTOR(alice);
   ACTORS((bob));

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
   auto& son = db.create<son_object>( [&]( son_object& sobj )
   {
      sobj.son_account = bob_id;
      sobj.statistics = db.create<son_statistics_object>([&](son_statistics_object& s){s.owner = sobj.id;}).id;
   });
   generate_block();
   db.modify( db.get_global_properties(), [&]( global_property_object& _gpo )
   {
      son_info sinfo;
      sinfo.son_id = son.id;
      _gpo.active_sons.push_back(sinfo);
   });
   generate_block();
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

