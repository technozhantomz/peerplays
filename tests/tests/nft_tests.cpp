#include <boost/test/unit_test.hpp>

#include "../common/database_fixture.hpp"

#include <graphene/chain/hardfork.hpp>
#include <graphene/chain/nft_object.hpp>
#include <graphene/app/database_api.hpp>


using namespace graphene::chain;
using namespace graphene::chain::test;


class nft_test_helper
{
    database_fixture& fixture_;

public:
    nft_test_helper(database_fixture& fixture):
        fixture_(fixture)
    {
       fixture_.generate_blocks(HARDFORK_NFT_TIME);
       fixture_.generate_block();
       fixture_.generate_block();
       set_expiration(fixture_.db, fixture_.trx);
    }

    nft_metadata_object create_metadata(const std::string& name, const std::string& symbol, const std::string& uri, const account_id_type& owner, const fc::ecc::private_key &priv_key)
    {
       const auto& idx_by_id = fixture_.db.get_index_type<nft_metadata_index>().indices().get<by_id>();
       size_t obj_count0 = idx_by_id.size();

       fixture_.generate_block();

       signed_transaction trx;
       set_expiration(fixture_.db, trx);

       nft_metadata_create_operation op;
       op.owner = owner;
       op.symbol = symbol;
       op.base_uri = uri;
       op.name = name;
       op.is_transferable = true;
       BOOST_CHECK_NO_THROW(op.validate());
       trx.operations.push_back(op);
       fixture_.sign(trx, priv_key);
       PUSH_TX(fixture_.db, trx, ~0);
       fixture_.generate_block();

       BOOST_REQUIRE( idx_by_id.size() == obj_count0 + 1 );  // one more metadata created

       const auto& idx_by_name = fixture_.db.get_index_type<nft_metadata_index>().indices().get<by_name>();
       auto obj = idx_by_name.find(name);
       BOOST_CHECK( obj->owner == owner );
       BOOST_CHECK( obj->name == name );
       BOOST_CHECK( obj->symbol == symbol );
       BOOST_CHECK( obj->base_uri == uri );
       return *obj;
    }


    nft_object mint(const nft_metadata_id_type& metadata, const account_id_type& owner, const account_id_type& payer,
        const fc::optional<account_id_type>& approved, const std::vector<account_id_type>& approved_operators,
        const fc::ecc::private_key &priv_key)
    {
       const auto& idx_by_id = fixture_.db.get_index_type<nft_index>().indices().get<by_id>();
       size_t obj_count0 = idx_by_id.size();

       fixture_.generate_block();

       signed_transaction trx;
       set_expiration(fixture_.db, trx);

       nft_mint_operation op;
       op.nft_metadata_id = metadata;
       op.payer = payer;
       op.owner = owner;
       if (approved)
          op.approved = *approved;
       op.approved_operators = approved_operators;

       trx.operations.push_back(op);
       fixture_.sign(trx, priv_key);
       PUSH_TX(fixture_.db, trx, ~0);

       fixture_.generate_block();

       BOOST_REQUIRE(idx_by_id.size() == obj_count0 + 1);  // one more created

       auto obj = idx_by_id.rbegin();

       BOOST_REQUIRE(obj != idx_by_id.rend());
       BOOST_CHECK(obj->owner == owner);
       BOOST_CHECK(obj->approved_operators.size() == approved_operators.size());
       BOOST_CHECK(obj->approved_operators == approved_operators);

       return *obj;
    }
};


BOOST_FIXTURE_TEST_SUITE( nft_tests, database_fixture )


BOOST_AUTO_TEST_CASE( nft_metadata_name_validation_test ) {
   BOOST_TEST_MESSAGE("nft_metadata_name_validation_test");
   ACTORS((mdowner));
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
   BOOST_CHECK_NO_THROW(op.validate());
}


BOOST_AUTO_TEST_CASE( nft_metadata_create_test ) {

   BOOST_TEST_MESSAGE("nft_metadata_create_test");
   nft_test_helper nfth(*this);
   ACTORS((mdowner));
   nfth.create_metadata("NFT Test", "NFT", "http://nft.example.com", mdowner_id, mdowner_private_key);
}


BOOST_AUTO_TEST_CASE( nft_metadata_listing_test ) {

   BOOST_TEST_MESSAGE("nft_metadata_listing_test");

   nft_test_helper nfth(*this);

   ACTORS((mdowner1));
   ACTORS((mdowner2));

   // prepare metadata set
   for (int i=0; i < 200; i++)
   {
      string sfx = fc::to_pretty_string(i);
      nft_metadata_object md = nfth.create_metadata("NFT Test " + sfx, "NFT" + sfx, "http://nft.example.com", mdowner1_id, mdowner1_private_key);
      BOOST_REQUIRE(md.id == nft_metadata_id_type(i));
   }
   for (int i=200; i < 250; i++)
   {
      string sfx = fc::to_pretty_string(i);
      nft_metadata_object md = nfth.create_metadata("NFT Test " + sfx, "NFT" + sfx, "http://nft.example.com", mdowner2_id, mdowner2_private_key);
      BOOST_REQUIRE(md.id == nft_metadata_id_type(i));
   }

   graphene::app::database_api db_api(db);
   vector<nft_metadata_object> listed;

   // first 100 returned
   listed = db_api.nft_get_metadata_by_owner(mdowner1_id, nft_metadata_id_type(0), 100);
   BOOST_REQUIRE(listed.size() == 100);
   BOOST_REQUIRE(listed[ 0].id == nft_metadata_id_type( 0));
   BOOST_REQUIRE(listed[99].id == nft_metadata_id_type(99));

   // 100 starting from 50
   listed = db_api.nft_get_metadata_by_owner(mdowner1_id, nft_metadata_id_type(50), 100);
   BOOST_REQUIRE(listed.size() == 100);
   BOOST_REQUIRE(listed[ 0].id == nft_metadata_id_type( 50));
   BOOST_REQUIRE(listed[99].id == nft_metadata_id_type(149));

   // the last 5 must be returned
   listed = db_api.nft_get_metadata_by_owner(mdowner1_id, nft_metadata_id_type(195), 10);
   BOOST_REQUIRE(listed.size() == 5);
   BOOST_REQUIRE(listed[0].id == nft_metadata_id_type(195));
   BOOST_REQUIRE(listed[4].id == nft_metadata_id_type(199));

   // too much requested at once
   BOOST_CHECK_THROW(db_api.nft_get_metadata_by_owner(mdowner1_id, nft_metadata_id_type(0), 101), fc::exception);

   // the last 40 must be returned
   listed = db_api.nft_get_metadata_by_owner(mdowner2_id, nft_metadata_id_type(210), 100);
   BOOST_REQUIRE(listed.size() == 40);
   BOOST_REQUIRE(listed[ 0].id == nft_metadata_id_type(210));
   BOOST_REQUIRE(listed[39].id == nft_metadata_id_type(249));
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

   nft_test_helper nfth(*this);

   ACTORS((mdowner));
   ACTORS((alice));
   ACTORS((bob));
   ACTORS((operator1));
   ACTORS((operator2));

   nft_metadata_object md = nfth.create_metadata("NFT Test", "NFT", "http://nft.example.com", mdowner_id, mdowner_private_key);

   nfth.mint(md.id, alice_id, mdowner_id, alice_id, {operator1_id, operator2_id}, alice_private_key);
}


BOOST_AUTO_TEST_CASE( nft_object_listing_test ) {

   BOOST_TEST_MESSAGE("nft_object_listing_test");

   nft_test_helper nfth(*this);

   ACTORS((mdowner1));
   ACTORS((mdowner2));
   ACTORS((alice));
   ACTORS((bob));

   nft_metadata_object md1 = nfth.create_metadata("NFT Test 1", "NFT1", "http://nft.example.com", mdowner1_id, mdowner1_private_key);
   nft_metadata_object md2 = nfth.create_metadata("NFT Test 2", "NFT2", "http://nft.example.com", mdowner2_id, mdowner2_private_key);

   // create NFT objects: 200 owned by alice and 200 by bob
   for (int i=0; i < 200; i++)
   {
      nft_object nft = nfth.mint(md1.id, alice_id, mdowner1_id, alice_id, {}, alice_private_key);
      BOOST_REQUIRE(nft.id == nft_id_type(i));
   }
   for (int i=200; i < 250; i++)
   {
      nft_object nft = nfth.mint(md1.id, bob_id, mdowner1_id, bob_id, {}, bob_private_key);
      BOOST_REQUIRE(nft.id == nft_id_type(i));
   }

   graphene::app::database_api db_api(db);
   vector<nft_object> listed;

   //
   // listing all tokens:
   //
   // first 100 returned, all alice's
   listed = db_api.nft_get_all_tokens(nft_id_type(0), 100);
   BOOST_REQUIRE(listed.size() == 100);
   BOOST_REQUIRE(listed[ 0].id == nft_id_type( 0));
   BOOST_REQUIRE(listed[99].id == nft_id_type(99));
   BOOST_REQUIRE(all_of(listed.begin(), listed.end(), [alice_id](const nft_object &obj){ return obj.owner == alice_id; }));

   // 100 starting from 50, all alice's
   listed = db_api.nft_get_all_tokens(nft_id_type(50), 100);
   BOOST_REQUIRE(listed.size() == 100);
   BOOST_REQUIRE(listed[ 0].id == nft_id_type( 50));
   BOOST_REQUIRE(listed[99].id == nft_id_type(149));
   BOOST_REQUIRE(all_of(listed.begin(), listed.end(), [alice_id](const nft_object &obj){ return obj.owner == alice_id; }));

   // the last 5 must be returned, all bob's
   listed = db_api.nft_get_all_tokens(nft_id_type(245), 10);
   BOOST_REQUIRE(listed.size() == 5);
   BOOST_REQUIRE(listed[0].id == nft_id_type(245));
   BOOST_REQUIRE(listed[4].id == nft_id_type(249));
   BOOST_REQUIRE(all_of(listed.begin(), listed.end(), [bob_id](const nft_object &obj){ return obj.owner == bob_id; }));

   // 10 from the middle of the set, half alice's, half bob's
   listed = db_api.nft_get_all_tokens(nft_id_type(195), 10);
   BOOST_REQUIRE(listed.size() == 10);
   BOOST_REQUIRE(listed[0].id == nft_id_type(195));
   BOOST_REQUIRE(listed[9].id == nft_id_type(204));
   BOOST_REQUIRE(listed[0].owner == alice_id);
   BOOST_REQUIRE(listed[4].owner == alice_id);
   BOOST_REQUIRE(listed[5].owner == bob_id);
   BOOST_REQUIRE(listed[9].owner == bob_id);

   // too much requested at once
   BOOST_CHECK_THROW(db_api.nft_get_all_tokens(nft_id_type(0), 101), fc::exception);

   //
   // listing tokens by owner:
   //
   // first 100 alice's
   listed = db_api.nft_get_tokens_by_owner(alice_id, nft_id_type(0), 100);
   BOOST_REQUIRE(listed.size() == 100);
   BOOST_REQUIRE(all_of(listed.begin(), listed.end(), [alice_id](const nft_object &obj){ return obj.owner == alice_id; }));
   BOOST_REQUIRE(listed[ 0].id == nft_id_type( 0));
   BOOST_REQUIRE(listed[99].id == nft_id_type(99));

   // the last 5 alice's must be returned
   listed = db_api.nft_get_tokens_by_owner(alice_id, nft_id_type(195), 10);
   BOOST_REQUIRE(listed.size() == 5);
   BOOST_REQUIRE(all_of(listed.begin(), listed.end(), [alice_id](const nft_object &obj){ return obj.owner == alice_id; }));
   BOOST_REQUIRE(listed[0].id == nft_id_type(195));
   BOOST_REQUIRE(listed[4].id == nft_id_type(199));

   // all 50 bob's
   listed = db_api.nft_get_tokens_by_owner(bob_id, nft_id_type(0), 60);
   BOOST_REQUIRE(listed.size() == 50);
   BOOST_REQUIRE(all_of(listed.begin(), listed.end(), [bob_id](const nft_object &obj){ return obj.owner == bob_id; }));
   BOOST_REQUIRE(listed[ 0].id == nft_id_type(200));
   BOOST_REQUIRE(listed[49].id == nft_id_type(249));

   // too much requested at once
   BOOST_CHECK_THROW(db_api.nft_get_tokens_by_owner(alice_id, nft_id_type(0), 101), fc::exception);
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

