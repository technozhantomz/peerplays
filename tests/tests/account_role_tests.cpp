#include <boost/test/unit_test.hpp>
#include <fc/reflect/variant.hpp>
#include <graphene/chain/protocol/operations.hpp>
#include <graphene/chain/hardfork.hpp>
#include <graphene/chain/database.hpp>
#include <graphene/chain/protocol/protocol.hpp>
#include <graphene/chain/exceptions.hpp>

#include <graphene/chain/account_object.hpp>
#include <graphene/chain/asset_object.hpp>
#include <graphene/chain/committee_member_object.hpp>
#include <graphene/chain/proposal_object.hpp>
#include <graphene/chain/custom_permission_object.hpp>
#include <graphene/chain/custom_account_authority_object.hpp>
#include <graphene/chain/nft_object.hpp>
#include <graphene/chain/account_role_object.hpp>
#include <graphene/chain/offer_object.hpp>

#include <graphene/db/simple_index.hpp>

#include <fc/crypto/digest.hpp>
#include "../common/database_fixture.hpp"

using namespace graphene::chain;
using namespace graphene::chain::test;

BOOST_FIXTURE_TEST_SUITE(account_role_tests, database_fixture)

BOOST_AUTO_TEST_CASE(account_role_create_test)
{
    try
    {
        BOOST_TEST_MESSAGE("account_role_create_test");
        generate_blocks(HARDFORK_NFT_TIME);
        generate_block();
        generate_block();
        set_expiration(db, trx);
        ACTORS((resourceowner)(alice)(bob)(charlie));
        upgrade_to_lifetime_member(resourceowner);
        upgrade_to_lifetime_member(alice);
        upgrade_to_lifetime_member(bob);
        upgrade_to_lifetime_member(charlie);
        transfer(committee_account, resourceowner_id, asset(100000 * GRAPHENE_BLOCKCHAIN_PRECISION));
        transfer(committee_account, alice_id, asset(100000 * GRAPHENE_BLOCKCHAIN_PRECISION));
        transfer(committee_account, bob_id, asset(100000 * GRAPHENE_BLOCKCHAIN_PRECISION));
        transfer(committee_account, charlie_id, asset(100000 * GRAPHENE_BLOCKCHAIN_PRECISION));
        {
            BOOST_TEST_MESSAGE("Send account_role_create_operation");

            account_role_create_operation op;
            op.owner = resourceowner_id;
            op.name = "Test Account Role";
            op.metadata = "{\"country\": \"earth\", \"race\": \"human\" }";

            int ops[] = {operation::tag<nft_safe_transfer_from_operation>::value,
                         operation::tag<nft_approve_operation>::value,
                         operation::tag<nft_set_approval_for_all_operation>::value,
                         operation::tag<offer_operation>::value,
                         operation::tag<bid_operation>::value,
                         operation::tag<cancel_offer_operation>::value};
            op.allowed_operations.insert(ops, ops + 6);
            op.whitelisted_accounts.emplace(alice_id);
            op.whitelisted_accounts.emplace(bob_id);
            op.valid_to = db.head_block_time() + 1000;

            trx.operations.push_back(op);
            sign(trx, resourceowner_private_key);
            PUSH_TX(db, trx);
            trx.clear();
        }
        const auto &idx = db.get_index_type<account_role_index>().indices().get<by_id>();
        BOOST_REQUIRE(idx.size() == 1);
        auto obj = idx.begin();
        BOOST_REQUIRE(obj != idx.end());
        BOOST_CHECK(obj->owner == resourceowner_id);
        BOOST_CHECK(obj->name == "Test Account Role");
        BOOST_CHECK(obj->metadata == "{\"country\": \"earth\", \"race\": \"human\" }");
        flat_set<int> expected_allowed_operations = {operation::tag<nft_safe_transfer_from_operation>::value,
                                                     operation::tag<nft_approve_operation>::value,
                                                     operation::tag<nft_set_approval_for_all_operation>::value,
                                                     operation::tag<offer_operation>::value,
                                                     operation::tag<bid_operation>::value,
                                                     operation::tag<cancel_offer_operation>::value};
        BOOST_CHECK(obj->allowed_operations == expected_allowed_operations);
        flat_set<account_id_type> expected_whitelisted_accounts = {alice_id, bob_id};
        BOOST_CHECK(obj->whitelisted_accounts == expected_whitelisted_accounts);
        BOOST_CHECK(obj->valid_to == db.head_block_time() + 1000);
    }
    FC_LOG_AND_RETHROW()
}

BOOST_AUTO_TEST_CASE(account_role_update_test)
{
    try
    {
        BOOST_TEST_MESSAGE("account_role_create_test");
        INVOKE(account_role_create_test);
        GET_ACTOR(alice);
        GET_ACTOR(bob);
        GET_ACTOR(charlie);
        GET_ACTOR(resourceowner);
        set_expiration(db, trx);
        {
            BOOST_TEST_MESSAGE("Send account_role_update_operation");

            account_role_update_operation op;
            op.owner = resourceowner_id;
            op.account_role_id = account_role_id_type(0);
            op.name = "Test Account Role Update";
            op.metadata = "{\"country\": \"earth\", \"race\": \"human\", \"op\":\"update\" }";

            int ops_add[] = {operation::tag<finalize_offer_operation>::value};
            int ops_delete[] = {operation::tag<nft_safe_transfer_from_operation>::value, operation::tag<nft_approve_operation>::value};
            op.allowed_operations_to_add.insert(ops_add, ops_add + 1);
            op.allowed_operations_to_remove.insert(ops_delete, ops_delete + 2);
            op.valid_to = db.head_block_time() + 10000;

            trx.operations.push_back(op);
            sign(trx, resourceowner_private_key);
            PUSH_TX(db, trx);
            trx.clear();
        }
        const auto &idx = db.get_index_type<account_role_index>().indices().get<by_id>();
        BOOST_REQUIRE(idx.size() == 1);
        auto obj = idx.begin();
        BOOST_REQUIRE(obj != idx.end());
        BOOST_CHECK(obj->owner == resourceowner_id);
        BOOST_CHECK(obj->name == "Test Account Role Update");
        BOOST_CHECK(obj->metadata == "{\"country\": \"earth\", \"race\": \"human\", \"op\":\"update\" }");
        flat_set<int> expected_allowed_operations = {operation::tag<finalize_offer_operation>::value,
                                                     operation::tag<nft_set_approval_for_all_operation>::value,
                                                     operation::tag<offer_operation>::value,
                                                     operation::tag<bid_operation>::value,
                                                     operation::tag<cancel_offer_operation>::value};
        BOOST_CHECK(obj->allowed_operations == expected_allowed_operations);
        flat_set<account_id_type> expected_whitelisted_accounts = {alice_id, bob_id};
        BOOST_CHECK(obj->whitelisted_accounts == expected_whitelisted_accounts);
        BOOST_CHECK(obj->valid_to == db.head_block_time() + 10000);
    }
    FC_LOG_AND_RETHROW()
}

BOOST_AUTO_TEST_CASE(account_role_delete_test)
{
    try
    {
        BOOST_TEST_MESSAGE("account_role_delete_test");
        INVOKE(account_role_create_test);
        GET_ACTOR(resourceowner);
        set_expiration(db, trx);
        {
            BOOST_TEST_MESSAGE("Send account_role_delete_operation");

            account_role_delete_operation op;
            op.owner = resourceowner_id;
            op.account_role_id = account_role_id_type(0);

            trx.operations.push_back(op);
            sign(trx, resourceowner_private_key);
            PUSH_TX(db, trx);
            trx.clear();
        }
        const auto &idx = db.get_index_type<account_role_index>().indices().get<by_id>();
        BOOST_REQUIRE(idx.size() == 0);
    }
    FC_LOG_AND_RETHROW()
}

BOOST_AUTO_TEST_CASE(nft_metadata_create_mint_with_account_role_test)
{
    try
    {
        BOOST_TEST_MESSAGE("nft_metadata_create_mint_with_account_role_test");
        INVOKE(account_role_create_test);
        GET_ACTOR(alice);
        GET_ACTOR(bob);
        GET_ACTOR(resourceowner);

        {
            BOOST_TEST_MESSAGE("Send nft_metadata_create_operation");

            nft_metadata_create_operation op;
            op.owner = resourceowner_id;
            op.name = "NFT Test";
            op.symbol = "NFT";
            op.base_uri = "http://nft.example.com";
            op.revenue_partner = resourceowner_id;
            op.revenue_split = 1000;
            op.is_transferable = true;
            op.is_sellable = true;
            op.account_role = account_role_id_type(0);

            trx.operations.push_back(op);
            sign(trx, resourceowner_private_key);
            PUSH_TX(db, trx);
            trx.clear();
        }
        generate_block();

        BOOST_TEST_MESSAGE("Check nft_metadata_create_operation results");

        const auto &nftmd_idx = db.get_index_type<nft_metadata_index>().indices().get<by_id>();
        BOOST_REQUIRE(nftmd_idx.size() == 1);
        auto nftmd_obj = nftmd_idx.begin();
        BOOST_REQUIRE(nftmd_obj != nftmd_idx.end());
        BOOST_CHECK(nftmd_obj->owner == resourceowner_id);
        BOOST_CHECK(nftmd_obj->name == "NFT Test");
        BOOST_CHECK(nftmd_obj->symbol == "NFT");
        BOOST_CHECK(nftmd_obj->base_uri == "http://nft.example.com");
        BOOST_CHECK(nftmd_obj->account_role == account_role_id_type(0));

        {
            BOOST_TEST_MESSAGE("Send nft_mint_operation");

            nft_mint_operation op;
            op.payer = resourceowner_id;
            op.nft_metadata_id = nftmd_obj->id;
            op.owner = alice_id;
            op.approved = alice_id;

            trx.operations.push_back(op);
            sign(trx, resourceowner_private_key);
            PUSH_TX(db, trx);
            trx.clear();
        }

        BOOST_TEST_MESSAGE("Check nft_mint_operation results");

        const auto &nft_idx = db.get_index_type<nft_index>().indices().get<by_id>();
        BOOST_REQUIRE(nft_idx.size() == 1);
        auto nft_obj = nft_idx.begin();
        BOOST_REQUIRE(nft_obj != nft_idx.end());
        BOOST_CHECK(nft_obj->owner == alice_id);
        BOOST_CHECK(nft_obj->approved == alice_id);
    }
    FC_LOG_AND_RETHROW()
}

BOOST_AUTO_TEST_CASE(nft_safe_transfer_account_role_test)
{
    try
    {
        BOOST_TEST_MESSAGE("nft_safe_transfer_account_role_test");
        INVOKE(nft_metadata_create_mint_with_account_role_test);
        GET_ACTOR(alice);
        GET_ACTOR(bob);
        GET_ACTOR(charlie);

        {
            BOOST_TEST_MESSAGE("Send nft_safe_transfer_from_operation");

            nft_safe_transfer_from_operation op;
            op.operator_ = alice_id;
            op.from = alice_id;
            op.to = bob_id;
            op.token_id = nft_id_type(0);
            op.data = "data";

            trx.operations.push_back(op);
            sign(trx, alice_private_key);
            PUSH_TX(db, trx);
            trx.clear();
        }
        generate_block();

        BOOST_TEST_MESSAGE("Check nft_safe_transfer_from_operation results");

        const auto &nft_idx = db.get_index_type<nft_index>().indices().get<by_id>();
        BOOST_REQUIRE(nft_idx.size() == 1);
        auto nft_obj = nft_idx.begin();
        BOOST_REQUIRE(nft_obj != nft_idx.end());
        BOOST_CHECK(nft_obj->owner == bob_id);

        {
            BOOST_TEST_MESSAGE("Send nft_safe_transfer_from_operation");

            nft_safe_transfer_from_operation op;
            op.operator_ = bob_id;
            op.from = bob_id;
            op.to = charlie_id;
            op.token_id = nft_id_type(0);
            op.data = "data";

            trx.operations.push_back(op);
            sign(trx, bob_private_key);
            // Charlie is not whitelisted by resource creator
            BOOST_CHECK_THROW(PUSH_TX(db, trx), fc::exception);
            trx.clear();
        }
    }
    FC_LOG_AND_RETHROW()
}

BOOST_AUTO_TEST_CASE(nft_offer_bid_account_role_test)
{
    try
    {
        BOOST_TEST_MESSAGE("nft_offer_bid_account_role_test");
        INVOKE(nft_metadata_create_mint_with_account_role_test);
        GET_ACTOR(alice);
        GET_ACTOR(bob);
        GET_ACTOR(charlie);

        {
            BOOST_TEST_MESSAGE("Send create_offer");

            offer_operation offer_op;
            offer_op.item_ids.emplace(nft_id_type(0));
            offer_op.issuer = alice_id;
            offer_op.buying_item = false;
            offer_op.maximum_price = asset(10000);
            offer_op.minimum_price = asset(10);
            offer_op.offer_expiration_date = db.head_block_time() + fc::seconds(15);

            trx.operations.push_back(offer_op);
            sign(trx, alice_private_key);
            PUSH_TX(db, trx);
            trx.clear();
        }
        // Charlie tries to bid but fails.
        {
            BOOST_TEST_MESSAGE("Send create_bid by charlie");
            bid_operation bid_op;
            bid_op.offer_id = offer_id_type(0);
            // Buy it now price
            bid_op.bid_price = asset(10000);
            bid_op.bidder = charlie_id;
            trx.operations.push_back(bid_op);
            sign(trx, charlie_private_key);
            // Charlie is not whitelisting to perform bid_operation by resource/metadata owner
            BOOST_CHECK_THROW(PUSH_TX(db, trx), fc::exception);
            trx.clear();
        }
        // Bob succeeds in bidding.
        {
            BOOST_TEST_MESSAGE("Send create_bid by bob");
            bid_operation bid_op;
            bid_op.offer_id = offer_id_type(0);
            // Buy it now price
            bid_op.bid_price = asset(10000);
            bid_op.bidder = bob_id;
            trx.operations.push_back(bid_op);
            sign(trx, bob_private_key);
            // Bob is whitelisted in account role created by resource/metadata owner
            PUSH_TX(db, trx);
            trx.clear();
        }
        generate_block();

        BOOST_TEST_MESSAGE("Check offer results");

        const auto &nft_idx = db.get_index_type<nft_index>().indices().get<by_id>();
        BOOST_REQUIRE(nft_idx.size() == 1);
        auto nft_obj = nft_idx.begin();
        BOOST_REQUIRE(nft_obj != nft_idx.end());
        BOOST_CHECK(nft_obj->owner == bob_id);
        // Charlie tries to bid (buy offer) but fails
        {
            BOOST_TEST_MESSAGE("Send create_offer");

            offer_operation offer_op;
            offer_op.item_ids.emplace(nft_id_type(0));
            offer_op.issuer = charlie_id;
            offer_op.buying_item = true;
            offer_op.maximum_price = asset(10000);
            offer_op.minimum_price = asset(10);
            offer_op.offer_expiration_date = db.head_block_time() + fc::seconds(15);

            trx.operations.push_back(offer_op);
            sign(trx, charlie_private_key);
            // Charlie is not whitelisting to perform offer_operation by resource/metadata owner
            BOOST_CHECK_THROW(PUSH_TX(db, trx), fc::exception);
            trx.clear();
        }
    }
    FC_LOG_AND_RETHROW()
}

BOOST_AUTO_TEST_SUITE_END()