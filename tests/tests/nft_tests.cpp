#include <boost/test/unit_test.hpp>

#include "../common/database_fixture.hpp"

#include <graphene/chain/hardfork.hpp>
#include <graphene/chain/nft_object.hpp>

using namespace graphene::chain;
using namespace graphene::chain::test;

BOOST_FIXTURE_TEST_SUITE( nft_tests, database_fixture )

BOOST_AUTO_TEST_CASE( nft_metadata_create_test ) {

   BOOST_TEST_MESSAGE("nft_metadata_create_test");
   generate_blocks(HARDFORK_NFT_TIME);
   generate_block();
   generate_block();
   set_expiration(db, trx);

   ACTORS((mdowner));

   generate_block();
   set_expiration(db, trx);

   {
      BOOST_TEST_MESSAGE("Send nft_metadata_create_operation");

      nft_metadata_create_operation op;
      op.owner = mdowner_id;
      op.symbol = "NFT";
      op.base_uri = "http://nft.example.com";
      op.name = "123";
      op.is_transferable = true;
      BOOST_CHECK_THROW(op.validate(), fc::exception);
      op.name = "";
      BOOST_CHECK_THROW(op.validate(), fc::exception);
      op.name = "1ab";
      BOOST_CHECK_THROW(op.validate(), fc::exception);
      op.name = ".abc";
      BOOST_CHECK_THROW(op.validate(), fc::exception);
      op.name = "abc.";
      BOOST_CHECK_THROW(op.validate(), fc::exception);
      op.name = "ABC";
      BOOST_CHECK_NO_THROW(op.validate());
      op.name = "abcdefghijklmnopq";
      BOOST_CHECK_THROW(op.validate(), fc::exception);
      op.name = "ab";
      BOOST_CHECK_THROW(op.validate(), fc::exception);
      op.name = "***";
      BOOST_CHECK_THROW(op.validate(), fc::exception);
      op.name = "a12";
      BOOST_CHECK_NO_THROW(op.validate());
      op.name = "a1b";
      BOOST_CHECK_NO_THROW(op.validate());
      op.name = "abc";
      BOOST_CHECK_NO_THROW(op.validate());
      op.name = "abc123defg12345";
      BOOST_CHECK_NO_THROW(op.validate());
      op.name = "NFT Test";
      trx.operations.push_back(op);
      sign(trx, mdowner_private_key);
      PUSH_TX(db, trx, ~0);
   }
   generate_block();

   BOOST_TEST_MESSAGE("Check nft_metadata_create_operation results");

   const auto& idx = db.get_index_type<nft_metadata_index>().indices().get<by_id>();
   BOOST_REQUIRE( idx.size() == 1 );
   auto obj = idx.begin();
   BOOST_REQUIRE( obj != idx.end() );
   BOOST_CHECK( obj->owner == mdowner_id );
   BOOST_CHECK( obj->name == "NFT Test" );
   BOOST_CHECK( obj->symbol == "NFT" );
   BOOST_CHECK( obj->base_uri == "http://nft.example.com" );
}


BOOST_AUTO_TEST_CASE( nft_metadata_update_test ) {

   BOOST_TEST_MESSAGE("nft_metadata_update_test");

   INVOKE(nft_metadata_create_test);

   GET_ACTOR(mdowner);

   {
      BOOST_TEST_MESSAGE("Send nft_metadata_update_operation");

      nft_metadata_update_operation op;
      op.owner = mdowner_id;
      op.name = "New NFT Test";
      op.symbol = "New NFT";
      op.base_uri = "new http://nft.example.com";

      trx.operations.push_back(op);
      sign(trx, mdowner_private_key);
      PUSH_TX(db, trx, ~0);
   }
   generate_block();

   BOOST_TEST_MESSAGE("Check nft_metadata_update_operation results");

   const auto& idx = db.get_index_type<nft_metadata_index>().indices().get<by_id>();
   BOOST_REQUIRE( idx.size() == 1 );
   auto obj = idx.begin();
   BOOST_REQUIRE( obj != idx.end() );
   BOOST_CHECK( obj->owner == mdowner_id );
   BOOST_CHECK( obj->name == "New NFT Test" );
   BOOST_CHECK( obj->symbol == "New NFT" );
   BOOST_CHECK( obj->base_uri == "new http://nft.example.com" );
}


BOOST_AUTO_TEST_CASE( nft_mint_test ) {

   BOOST_TEST_MESSAGE("nft_mint_test");

   generate_block();
   set_expiration(db, trx);

   INVOKE(nft_metadata_create_test);

   ACTORS((alice));
   ACTORS((bob));
   ACTORS((operator1));
   ACTORS((operator2));

   GET_ACTOR(mdowner);

   generate_block();
   set_expiration(db, trx);

   {
      BOOST_TEST_MESSAGE("Send nft_mint_operation");

      const auto& idx = db.get_index_type<nft_metadata_index>().indices().get<by_id>();
      BOOST_REQUIRE( idx.size() == 1 );
      auto nft_md_obj = idx.begin();

      nft_mint_operation op;
      op.payer = mdowner_id;
      op.nft_metadata_id = nft_md_obj->id;
      op.owner = alice_id;
      op.approved = alice_id;
      op.approved_operators.push_back(operator1_id);
      op.approved_operators.push_back(operator2_id);

      trx.operations.push_back(op);
      sign(trx, alice_private_key);
      PUSH_TX(db, trx, ~0);
   }
   generate_block();

   BOOST_TEST_MESSAGE("Check nft_mint_operation results");

   const auto& idx = db.get_index_type<nft_index>().indices().get<by_id>();
   BOOST_REQUIRE( idx.size() == 1 );
   auto obj = idx.begin();
   BOOST_REQUIRE( obj != idx.end() );
   BOOST_CHECK( obj->owner == alice_id );
   BOOST_CHECK( obj->approved_operators.size() == 2 );
   BOOST_CHECK( obj->approved_operators.at(0) == operator1_id );
   BOOST_CHECK( obj->approved_operators.at(1) == operator2_id );
}


BOOST_AUTO_TEST_CASE( nft_safe_transfer_from_test ) {

   BOOST_TEST_MESSAGE("nft_safe_transfer_from_test");

   INVOKE(nft_mint_test);

   GET_ACTOR(alice);
   GET_ACTOR(bob);

   {
      BOOST_TEST_MESSAGE("Check nft_safe_transfer_operation preconditions");

      const auto& idx = db.get_index_type<nft_index>().indices().get<by_id>();
      BOOST_REQUIRE( idx.size() == 1 );
      auto obj = idx.begin();
      BOOST_REQUIRE( obj->owner == alice_id );
   }

   {
      BOOST_TEST_MESSAGE("Send nft_safe_transfer_operation");

      nft_safe_transfer_from_operation op;
      op.operator_ = alice_id;
      op.from = alice_id;
      op.to = bob_id;
      op.token_id = nft_id_type(0);
      op.data = "data";

      trx.operations.push_back(op);
      sign(trx, alice_private_key);
      PUSH_TX(db, trx, ~0);
   }
   generate_block();

   {
      BOOST_TEST_MESSAGE("Check nft_safe_transfer_operation results");

      const auto& idx = db.get_index_type<nft_index>().indices().get<by_id>();
      BOOST_REQUIRE( idx.size() == 1 );
      auto obj = idx.begin();
      BOOST_REQUIRE( obj->owner == bob_id );
   }
}

BOOST_AUTO_TEST_CASE( nft_approve_operation_test ) {

   BOOST_TEST_MESSAGE("nft_approve_operation_test");

   INVOKE(nft_mint_test);

   GET_ACTOR(alice);
   GET_ACTOR(operator1);
   GET_ACTOR(operator2);

   ACTORS((operator3));

   {
      BOOST_TEST_MESSAGE("Send nft_approve_operation");

      nft_approve_operation op;
      op.operator_ = alice_id;
      op.approved = operator3_id;
      op.token_id = nft_id_type(0);

      trx.operations.push_back(op);
      sign(trx, alice_private_key);
      PUSH_TX(db, trx, ~0);
   }
   generate_block();

   {
      BOOST_TEST_MESSAGE("Check nft_approve_operation results");

      const auto& idx = db.get_index_type<nft_index>().indices().get<by_id>();
      BOOST_REQUIRE( idx.size() == 1 );
      auto obj = idx.begin();
      BOOST_REQUIRE( obj != idx.end() );
      BOOST_CHECK( obj->approved == operator3_id );
      BOOST_CHECK( obj->approved_operators.size() == 2 );
      BOOST_CHECK( obj->approved_operators.at(0) == operator1_id );
      BOOST_CHECK( obj->approved_operators.at(1) == operator2_id );
   }
}

BOOST_AUTO_TEST_CASE( nft_set_approval_for_all_test ) {

   BOOST_TEST_MESSAGE("nft_set_approval_for_all_test");

   generate_block();
   set_expiration(db, trx);

   ACTORS((alice));
   ACTORS((bob));

   INVOKE(nft_metadata_create_test);

   GET_ACTOR(mdowner);

   generate_block();
   set_expiration(db, trx);

   BOOST_TEST_MESSAGE("Create NFT assets");

   const auto& idx = db.get_index_type<nft_metadata_index>().indices().get<by_id>();
   BOOST_REQUIRE( idx.size() == 1 );
   auto nft_md_obj = idx.begin();

   {
      BOOST_TEST_MESSAGE("Send nft_mint_operation 1");

      nft_mint_operation op;
      op.payer = mdowner_id;
      op.nft_metadata_id = nft_md_obj->id;
      op.owner = alice_id;

      trx.operations.push_back(op);
      sign(trx, mdowner_private_key);
      PUSH_TX(db, trx, ~0);
   }
   generate_block();

   {
      BOOST_TEST_MESSAGE("Send nft_mint_operation 2");

      nft_mint_operation op;
      op.payer = mdowner_id;
      op.nft_metadata_id = nft_md_obj->id;
      op.owner = bob_id;

      trx.operations.push_back(op);
      sign(trx, mdowner_private_key);
      PUSH_TX(db, trx, ~0);
   }
   generate_block();

   {
      BOOST_TEST_MESSAGE("Send nft_mint_operation 3");

      nft_mint_operation op;
      op.payer = mdowner_id;
      op.nft_metadata_id = nft_md_obj->id;
      op.owner = alice_id;

      trx.operations.push_back(op);
      sign(trx, mdowner_private_key);
      PUSH_TX(db, trx, ~0);
   }
   generate_block();

   {
      BOOST_TEST_MESSAGE("Send nft_mint_operation 4");

      nft_mint_operation op;
      op.payer = mdowner_id;
      op.nft_metadata_id = nft_md_obj->id;
      op.owner = bob_id;

      trx.operations.push_back(op);
      sign(trx, mdowner_private_key);
      PUSH_TX(db, trx, ~0);
   }
   generate_block();

   {
      BOOST_TEST_MESSAGE("Send nft_mint_operation 5");

      nft_mint_operation op;
      op.payer = mdowner_id;
      op.nft_metadata_id = nft_md_obj->id;
      op.owner = alice_id;

      trx.operations.push_back(op);
      sign(trx, mdowner_private_key);
      PUSH_TX(db, trx, ~0);
   }
   generate_block();


   {
      BOOST_TEST_MESSAGE("Send nft_approve_operation");

      nft_set_approval_for_all_operation op;
      op.owner = alice_id;
      op.operator_ = bob_id;
      op.approved = true;

      trx.operations.push_back(op);
      sign(trx, alice_private_key);
      PUSH_TX(db, trx, ~0);
   }
   generate_block();

   {
      BOOST_TEST_MESSAGE("Send nft_approve_operation");

      nft_set_approval_for_all_operation op;
      op.owner = alice_id;
      op.operator_ = bob_id;
      op.approved = true;

      trx.operations.push_back(op);
      sign(trx, alice_private_key);
      PUSH_TX(db, trx, ~0);
   }
   generate_block();

   {
      BOOST_TEST_MESSAGE("Check nft_approve_operation results");

      const auto& idx = db.get_index_type<nft_index>().indices().get<by_owner>();
      const auto &idx_range = idx.equal_range(alice_id);
      std::for_each(idx_range.first, idx_range.second, [&](const nft_object &obj) {
         BOOST_CHECK( obj.approved_operators.size() == 1 );
         BOOST_CHECK( obj.approved_operators.at(0) == bob_id );
      });
   }

   {
      BOOST_TEST_MESSAGE("Send nft_approve_operation");

      nft_set_approval_for_all_operation op;
      op.owner = alice_id;
      op.operator_ = bob_id;
      op.approved = false;

      trx.operations.push_back(op);
      sign(trx, alice_private_key);
      PUSH_TX(db, trx, ~0);
   }
   generate_block();

   {
      BOOST_TEST_MESSAGE("Check nft_approve_operation results");

      const auto& idx = db.get_index_type<nft_index>().indices().get<by_owner>();
      const auto &idx_range = idx.equal_range(alice_id);
      std::for_each(idx_range.first, idx_range.second, [&](const nft_object &obj) {
         BOOST_CHECK( obj.approved_operators.size() == 0 );
      });
   }
}

BOOST_AUTO_TEST_SUITE_END()

