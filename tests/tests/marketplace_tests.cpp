#include <boost/test/unit_test.hpp>

#include "../common/database_fixture.hpp"

#include <graphene/chain/hardfork.hpp>
#include <graphene/chain/nft_object.hpp>
#include <graphene/chain/offer_object.hpp>

using namespace graphene::chain;
using namespace graphene::chain::test;

BOOST_FIXTURE_TEST_SUITE(marketplace_tests, database_fixture)
offer_id_type buy_offer;
offer_id_type sell_offer;
BOOST_AUTO_TEST_CASE(nft_metadata_create_test)
{

    BOOST_TEST_MESSAGE("nft_metadata_create_test");
    generate_blocks(HARDFORK_NFT_TIME);
    generate_block();
    set_expiration(db, trx);

    ACTORS((mdowner));

    generate_block();
    set_expiration(db, trx);

    {
        BOOST_TEST_MESSAGE("Send nft_metadata_create_operation");

        nft_metadata_create_operation op;
        op.owner = mdowner_id;
        op.name = "NFT Test";
        op.symbol = "NFT";
        op.base_uri = "http://nft.example.com";
        op.revenue_partner = mdowner_id;
        op.revenue_split = 1000;

        trx.operations.push_back(op);
        sign(trx, mdowner_private_key);
        PUSH_TX(db, trx, ~0);
    }
    generate_block();

    BOOST_TEST_MESSAGE("Check nft_metadata_create_operation results");

    const auto &idx = db.get_index_type<nft_metadata_index>().indices().get<by_id>();
    BOOST_REQUIRE(idx.size() == 1);
    auto obj = idx.begin();
    BOOST_REQUIRE(obj != idx.end());
    BOOST_CHECK(obj->owner == mdowner_id);
    BOOST_CHECK(obj->name == "NFT Test");
    BOOST_CHECK(obj->symbol == "NFT");
    BOOST_CHECK(obj->base_uri == "http://nft.example.com");
}

BOOST_AUTO_TEST_CASE(nft_mint_test)
{

    BOOST_TEST_MESSAGE("nft_mint_test");

    INVOKE(nft_metadata_create_test);
    set_expiration(db, trx);

    ACTORS((alice)(bob)(charlie)(operator1)(operator2));
    upgrade_to_lifetime_member(alice);
    upgrade_to_lifetime_member(bob);
    upgrade_to_lifetime_member(charlie);
    transfer(committee_account, alice_id, asset(1000 * GRAPHENE_BLOCKCHAIN_PRECISION));
    transfer(committee_account, bob_id, asset(1000 * GRAPHENE_BLOCKCHAIN_PRECISION));
    transfer(committee_account, charlie_id, asset(1000 * GRAPHENE_BLOCKCHAIN_PRECISION));

    GET_ACTOR(mdowner);

    generate_block();
    set_expiration(db, trx);

    {
        BOOST_TEST_MESSAGE("Send nft_mint_operation");

        const auto &idx = db.get_index_type<nft_metadata_index>().indices().get<by_id>();
        BOOST_REQUIRE(idx.size() == 1);
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
        trx.clear();
    }
    generate_block();

    BOOST_TEST_MESSAGE("Check nft_mint_operation results");

    const auto &idx = db.get_index_type<nft_index>().indices().get<by_id>();
    BOOST_REQUIRE(idx.size() == 1);
    auto obj = idx.begin();
    BOOST_REQUIRE(obj != idx.end());
    BOOST_CHECK(obj->owner == alice_id);
    BOOST_CHECK(obj->approved_operators.size() == 2);
    BOOST_CHECK(obj->approved_operators.at(0) == operator1_id);
    BOOST_CHECK(obj->approved_operators.at(1) == operator2_id);

    {
        const auto &idx = db.get_index_type<nft_metadata_index>().indices().get<by_id>();
        BOOST_REQUIRE(idx.size() == 1);
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
        trx.clear();
    }
    generate_block();
    BOOST_REQUIRE(idx.size() == 2);
    obj = idx.begin();
    BOOST_REQUIRE(obj != idx.end());
    BOOST_CHECK(obj->owner == alice_id);
    BOOST_CHECK(obj->approved_operators.size() == 2);
    BOOST_CHECK(obj->approved_operators.at(0) == operator1_id);
    BOOST_CHECK(obj->approved_operators.at(1) == operator2_id);
    const auto &nft2 = nft_id_type(1)(db);
    BOOST_CHECK(nft2.owner == alice_id);
    BOOST_CHECK(nft2.approved_operators.size() == 2);
    BOOST_CHECK(nft2.approved_operators.at(0) == operator1_id);
    BOOST_CHECK(nft2.approved_operators.at(1) == operator2_id);
}

BOOST_AUTO_TEST_CASE(create_sell_offer_test)
{
    try
    {
        INVOKE(nft_mint_test);
        GET_ACTOR(alice);
        GET_ACTOR(bob);
        GET_ACTOR(operator1);
        GET_ACTOR(operator2);
        const asset_object &bitusd = create_bitasset("STUB");
        {
            offer_operation offer_op;
            offer_op.item_ids.emplace(nft_id_type(0));
            offer_op.item_ids.emplace(nft_id_type(1));
            offer_op.issuer = alice_id;
            offer_op.buying_item = false;
            offer_op.maximum_price = asset(10000);
            offer_op.minimum_price = asset(10);
            offer_op.offer_expiration_date = db.head_block_time() + fc::seconds(15);
            trx.operations.push_back(offer_op);
            auto op = trx.operations.back().get<offer_operation>();
            REQUIRE_THROW_WITH_VALUE(op, offer_expiration_date, db.head_block_time());
            REQUIRE_THROW_WITH_VALUE(op, issuer, bob_id);
            // positive prices
            REQUIRE_OP_VALIDATION_FAILURE(op, minimum_price, asset(-1));
            REQUIRE_OP_VALIDATION_FAILURE(op, maximum_price, asset(-1));
            REQUIRE_OP_VALIDATION_FAILURE(op, fee, asset(-1));
            // min price > max price check
            REQUIRE_OP_VALIDATION_FAILURE(op, maximum_price, asset(1));
            // different asset for min/max
            REQUIRE_OP_VALIDATION_FAILURE(op, minimum_price, asset(1, bitusd.id));

            trx.clear();
            trx.operations.push_back(offer_op);
            sign(trx, alice_private_key);
            PUSH_TX(db, trx);
            trx.clear();
            //generate_block();

            const auto &idx = db.get_index_type<offer_index>().indices().get<by_id>();
            BOOST_REQUIRE(idx.size() == 1);
            const offer_object &d = offer_id_type(0)(db);

            BOOST_CHECK(d.space_id == protocol_ids);
            BOOST_CHECK(d.type_id == offer_object_type);
            //empty bid
            BOOST_CHECK(!d.bid_price);
            BOOST_CHECK(!d.bidder);
            // data integrity
            BOOST_CHECK(d.issuer == alice_id);
            BOOST_CHECK(d.maximum_price == asset(10000));
            BOOST_CHECK(d.minimum_price == asset(10));
            BOOST_CHECK(d.buying_item == false);
            BOOST_CHECK(db.item_locked(nft_id_type(0)) == true);
            BOOST_CHECK(db.item_locked(nft_id_type(1)) == true);
            sell_offer = d.id;
        }
    }
    catch (fc::exception &e)
    {
        edump((e.to_detail_string()));
        throw;
    }
}

BOOST_AUTO_TEST_CASE(buy_bid_for_sell_offer_test)
{
    try
    {
        INVOKE(create_sell_offer_test);
        GET_ACTOR(alice);
        GET_ACTOR(bob);
        GET_ACTOR(operator1);

        const auto &offer_obj = sell_offer(db);

        bid_operation bid_op;
        bid_op.offer_id = offer_obj.id;
        bid_op.bid_price = asset(offer_obj.minimum_price.amount + 1, offer_obj.minimum_price.asset_id);
        bid_op.bidder = bob_id;
        trx.operations.push_back(bid_op);

        asset exp_delta_bidder = -bid_op.bid_price;
        int64_t bidder_balance = get_balance(bob_id(db), asset_id_type()(db));

        auto op = trx.operations.back().get<bid_operation>();
        // Positive asset values
        REQUIRE_THROW_WITH_VALUE(op, bid_price, asset(-1, asset_id_type()));
        // Max price limit
        REQUIRE_THROW_WITH_VALUE(op, bid_price, asset(offer_obj.maximum_price.amount + 1, offer_obj.minimum_price.asset_id));
        // Min Price Limit
        REQUIRE_THROW_WITH_VALUE(op, bid_price, asset(offer_obj.minimum_price.amount - 1, offer_obj.minimum_price.asset_id));
        // Invalid offer
        REQUIRE_THROW_WITH_VALUE(op, offer_id, offer_id_type(6));
        // Owner bidder
        REQUIRE_THROW_WITH_VALUE(op, bidder, alice_id);
        // Operator bidder
        REQUIRE_THROW_WITH_VALUE(op, bidder, operator1_id);
        // Different asset
        REQUIRE_THROW_WITH_VALUE(op, bid_price, asset(50, asset_id_type(1)));

        trx.clear();
        trx.operations.push_back(bid_op);
        sign(trx, bob_private_key);
        PUSH_TX(db, trx);
        trx.clear();

        BOOST_CHECK_EQUAL(get_balance(bob_id(db), asset_id_type()(db)),
                          (bidder_balance + exp_delta_bidder.amount).value);
        //not empty bid
        BOOST_CHECK(offer_obj.bid_price);
        BOOST_CHECK(offer_obj.bidder);
        // data integrity
        BOOST_CHECK(offer_obj.bidder == bob_id);
        BOOST_CHECK(offer_obj.issuer == alice_id);
        BOOST_CHECK(offer_obj.maximum_price == asset(10000));
        BOOST_CHECK(offer_obj.minimum_price == asset(10));
        BOOST_CHECK(offer_obj.bid_price == bid_op.bid_price);
        BOOST_CHECK(db.item_locked(nft_id_type(0)));
        BOOST_CHECK(db.item_locked(nft_id_type(1)));
    }
    catch (fc::exception &e)
    {
        edump((e.to_detail_string()));
        throw;
    }
}

BOOST_AUTO_TEST_CASE(second_buy_bid_for_sell_offer_test)
{
    try
    {
        INVOKE(buy_bid_for_sell_offer_test);
        GET_ACTOR(alice);
        GET_ACTOR(bob);
        GET_ACTOR(charlie);
        GET_ACTOR(operator1);

        int64_t bob_balance = get_balance(bob_id(db), asset_id_type()(db));
        int64_t charlie_balance = get_balance(charlie_id(db), asset_id_type()(db));
        const auto &offer_obj = sell_offer(db);

        bid_operation bid_op;
        bid_op.offer_id = offer_obj.id;
        bid_op.bid_price = asset((*offer_obj.bid_price).amount + 1, offer_obj.minimum_price.asset_id);
        bid_op.bidder = charlie_id;
        trx.operations.push_back(bid_op);

        asset bid = bid_op.bid_price;
        asset exp_delta_bidder1 = *offer_obj.bid_price;
        asset exp_delta_bidder2 = -bid;

        auto op = trx.operations.back().get<bid_operation>();
        // Not a better bid than previous
        REQUIRE_THROW_WITH_VALUE(op, bid_price, asset((*offer_obj.bid_price).amount, offer_obj.minimum_price.asset_id));

        trx.clear();
        trx.operations.push_back(bid_op);
        sign(trx, charlie_private_key);
        PUSH_TX(db, trx);
        trx.clear();

        BOOST_CHECK_EQUAL(get_balance(bob_id(db), asset_id_type()(db)),
                          (bob_balance + exp_delta_bidder1.amount).value);
        BOOST_CHECK_EQUAL(get_balance(charlie_id(db), asset_id_type()(db)),
                          (charlie_balance + exp_delta_bidder2.amount).value);

        //not empty bid
        BOOST_CHECK(offer_obj.bid_price);
        BOOST_CHECK(offer_obj.bidder);

        // data integrity
        BOOST_CHECK(offer_obj.bidder == charlie_id);
        BOOST_CHECK(offer_obj.issuer == alice_id);
        BOOST_CHECK(offer_obj.maximum_price == asset(10000));
        BOOST_CHECK(offer_obj.minimum_price == asset(10));
        BOOST_CHECK(offer_obj.bid_price == bid);
        BOOST_CHECK(db.item_locked(nft_id_type(0)));
        BOOST_CHECK(db.item_locked(nft_id_type(1)));
    }
    catch (fc::exception &e)
    {
        edump((e.to_detail_string()));
        throw;
    }
}

BOOST_AUTO_TEST_CASE(best_buy_bid_for_sell_offer)
{
    try
    {
        INVOKE(second_buy_bid_for_sell_offer_test);
        GET_ACTOR(alice);
        GET_ACTOR(bob);
        GET_ACTOR(charlie);
        GET_ACTOR(operator1);
        GET_ACTOR(mdowner);

        int64_t bob_balance = get_balance(bob_id(db), asset_id_type()(db));
        int64_t alice_balance = get_balance(alice_id(db), asset_id_type()(db));
        int64_t charlie_balance = get_balance(charlie_id(db), asset_id_type()(db));
        int64_t mdowner_balance = get_balance(mdowner_id(db), asset_id_type()(db));
        const auto &offer_obj = sell_offer(db);

        bid_operation bid_op;
        bid_op.offer_id = offer_obj.id;
        bid_op.bid_price = asset(offer_obj.maximum_price.amount, offer_obj.minimum_price.asset_id);
        bid_op.bidder = bob_id;

        asset bid = bid_op.bid_price;
        asset exp_delta_bidder1 = *offer_obj.bid_price;
        asset exp_delta_bidder2 = -bid;

        trx.operations.push_back(bid_op);
        sign(trx, bob_private_key);
        PUSH_TX(db, trx);
        trx.clear();
        // Check balances
        BOOST_CHECK_EQUAL(get_balance(bob_id(db), asset_id_type()(db)),
                          (bob_balance + exp_delta_bidder2.amount).value);
        BOOST_CHECK_EQUAL(get_balance(charlie_id(db), asset_id_type()(db)),
                          (charlie_balance + exp_delta_bidder1.amount).value);
        //not empty bid
        BOOST_CHECK(offer_obj.bid_price);
        BOOST_CHECK(offer_obj.bidder);
        // data integrity
        BOOST_CHECK(offer_obj.bidder == bob_id);
        BOOST_CHECK(offer_obj.issuer == alice_id);
        BOOST_CHECK(offer_obj.maximum_price == asset(10000));
        BOOST_CHECK(offer_obj.minimum_price == asset(10));
        BOOST_CHECK(offer_obj.bid_price == bid);
        BOOST_CHECK(db.item_locked(nft_id_type(0)));
        BOOST_CHECK(db.item_locked(nft_id_type(1)));
        auto cached_offer_obj = offer_obj;
        // Generate a block and offer should be finalized with bid
        generate_block();
        int64_t partner_fee = 2 * static_cast<int64_t>((0.1 * (*cached_offer_obj.bid_price).amount.value)/2);
        BOOST_CHECK_EQUAL(get_balance(alice_id(db), asset_id_type()(db)),
                          (alice_balance + cached_offer_obj.maximum_price.amount).value - partner_fee);
        BOOST_CHECK_EQUAL(get_balance(mdowner_id(db), asset_id_type()(db)),
                          mdowner_balance + partner_fee);
        const auto &oidx = db.get_index_type<offer_index>().indices().get<by_id>();
        const auto &ohidx = db.get_index_type<offer_history_index>().indices().get<by_id>();
        BOOST_REQUIRE(oidx.size() == 0);
        BOOST_REQUIRE(ohidx.size() == 1);
        BOOST_CHECK(db.item_locked(nft_id_type(0)) == false);
        BOOST_CHECK(db.item_locked(nft_id_type(1)) == false);
        BOOST_CHECK((nft_id_type(0)(db).owner == bob_id) && (nft_id_type(1)(db).owner == bob_id));
        // Get offer history object
        const auto &history_obj = offer_history_id_type(0)(db);
        // History object data check
        BOOST_CHECK(cached_offer_obj.bid_price == history_obj.bid_price);
        BOOST_CHECK(cached_offer_obj.bidder == history_obj.bidder);
        BOOST_CHECK(cached_offer_obj.buying_item == history_obj.buying_item);
        BOOST_CHECK(cached_offer_obj.issuer == history_obj.issuer);
        BOOST_CHECK(cached_offer_obj.maximum_price == history_obj.maximum_price);
        BOOST_CHECK(cached_offer_obj.minimum_price == history_obj.minimum_price);
        BOOST_CHECK(cached_offer_obj.offer_expiration_date == history_obj.offer_expiration_date);
        BOOST_CHECK(cached_offer_obj.item_ids == history_obj.item_ids);
        BOOST_CHECK(result_type::Expired == history_obj.result);
    }
    catch (fc::exception &e)
    {
        edump((e.to_detail_string()));
        throw;
    }
}

BOOST_AUTO_TEST_CASE(expire_with_bid_for_sell_offer_test)
{
    INVOKE(second_buy_bid_for_sell_offer_test);
    GET_ACTOR(alice);
    GET_ACTOR(charlie);
    GET_ACTOR(mdowner);
    int64_t alice_balance = get_balance(alice_id(db), asset_id_type()(db));
    int64_t mdowner_balance = get_balance(mdowner_id(db), asset_id_type()(db));
    const auto &offer_obj = sell_offer(db);
    auto cached_offer_obj = offer_obj;
    generate_blocks(5);
    int64_t partner_fee = 2 * static_cast<int64_t>((0.1 * (*cached_offer_obj.bid_price).amount.value)/2);
    BOOST_CHECK_EQUAL(get_balance(mdowner_id(db), asset_id_type()(db)),
                        mdowner_balance + partner_fee);
    BOOST_CHECK_EQUAL(get_balance(alice_id(db), asset_id_type()(db)),
                      (alice_balance + (*cached_offer_obj.bid_price).amount).value - partner_fee);
    const auto &oidx = db.get_index_type<offer_index>().indices().get<by_id>();
    const auto &ohidx = db.get_index_type<offer_history_index>().indices().get<by_id>();
    BOOST_REQUIRE(oidx.size() == 0);
    BOOST_REQUIRE(ohidx.size() == 1);
    BOOST_CHECK(db.item_locked(nft_id_type(0)) == false);
    BOOST_CHECK(db.item_locked(nft_id_type(1)) == false);
    BOOST_CHECK((nft_id_type(0)(db).owner == charlie_id) && (nft_id_type(1)(db).owner == charlie_id));
    // Get offer history object
    const auto &history_obj = offer_history_id_type(0)(db);
    // History object data check
    BOOST_CHECK(cached_offer_obj.bid_price == history_obj.bid_price);
    BOOST_CHECK(cached_offer_obj.bidder == history_obj.bidder);
    BOOST_CHECK(cached_offer_obj.buying_item == history_obj.buying_item);
    BOOST_CHECK(cached_offer_obj.issuer == history_obj.issuer);
    BOOST_CHECK(cached_offer_obj.maximum_price == history_obj.maximum_price);
    BOOST_CHECK(cached_offer_obj.minimum_price == history_obj.minimum_price);
    BOOST_CHECK(cached_offer_obj.offer_expiration_date == history_obj.offer_expiration_date);
    BOOST_CHECK(cached_offer_obj.item_ids == history_obj.item_ids);
    BOOST_CHECK(result_type::Expired == history_obj.result);
}

BOOST_AUTO_TEST_CASE(expire_no_bid_for_sell_offer_test)
{
    INVOKE(create_sell_offer_test);
    GET_ACTOR(alice);
    int64_t alice_balance = get_balance(alice_id(db), asset_id_type()(db));
    const auto &offer_obj = sell_offer(db);
    auto cached_offer_obj = offer_obj;
    generate_blocks(5);
    BOOST_CHECK_EQUAL(get_balance(alice_id(db), asset_id_type()(db)),
                      alice_balance);
    const auto &oidx = db.get_index_type<offer_index>().indices().get<by_id>();
    const auto &ohidx = db.get_index_type<offer_history_index>().indices().get<by_id>();
    BOOST_REQUIRE(oidx.size() == 0);
    BOOST_REQUIRE(ohidx.size() == 1);
    BOOST_CHECK(db.item_locked(nft_id_type(0)) == false);
    BOOST_CHECK(db.item_locked(nft_id_type(1)) == false);
    BOOST_CHECK((nft_id_type(0)(db).owner == alice_id) && (nft_id_type(1)(db).owner == alice_id));
    // Get offer history object
    const auto &history_obj = offer_history_id_type(0)(db);
    // History object data check
    BOOST_CHECK(cached_offer_obj.bid_price == history_obj.bid_price);
    BOOST_CHECK(cached_offer_obj.bidder == history_obj.bidder);
    BOOST_CHECK(cached_offer_obj.buying_item == history_obj.buying_item);
    BOOST_CHECK(cached_offer_obj.issuer == history_obj.issuer);
    BOOST_CHECK(cached_offer_obj.maximum_price == history_obj.maximum_price);
    BOOST_CHECK(cached_offer_obj.minimum_price == history_obj.minimum_price);
    BOOST_CHECK(cached_offer_obj.offer_expiration_date == history_obj.offer_expiration_date);
    BOOST_CHECK(cached_offer_obj.item_ids == history_obj.item_ids);
    BOOST_CHECK(result_type::ExpiredNoBid == history_obj.result);
}

BOOST_AUTO_TEST_CASE(create_buy_offer_test)
{
    try
    {
        INVOKE(best_buy_bid_for_sell_offer);
        GET_ACTOR(alice);
        GET_ACTOR(bob);
        GET_ACTOR(operator1);
        GET_ACTOR(operator2);
        {
            int64_t alice_balance = get_balance(alice_id(db), asset_id_type()(db));
            offer_operation offer_op;
            offer_op.item_ids.emplace(nft_id_type(0));
            offer_op.item_ids.emplace(nft_id_type(1));
            offer_op.issuer = alice_id;
            offer_op.buying_item = true;
            offer_op.maximum_price = asset(11000);
            offer_op.minimum_price = asset(10);
            offer_op.offer_expiration_date = db.head_block_time() + fc::seconds(15);
            trx.operations.push_back(offer_op);
            auto op = trx.operations.back().get<offer_operation>();
            REQUIRE_THROW_WITH_VALUE(op, issuer, bob_id);

            trx.clear();
            trx.operations.push_back(offer_op);
            sign(trx, alice_private_key);
            PUSH_TX(db, trx);
            trx.clear();

            asset exp_delta_bidder2 = -offer_op.maximum_price;
            BOOST_CHECK_EQUAL(get_balance(alice_id(db), asset_id_type()(db)),
                              (alice_balance + exp_delta_bidder2.amount).value);

            const auto &idx = db.get_index_type<offer_index>().indices().get<by_id>();
            BOOST_REQUIRE(idx.size() == 1);
            const offer_object &d = offer_id_type(1)(db);

            BOOST_CHECK(d.space_id == protocol_ids);
            BOOST_CHECK(d.type_id == offer_object_type);
            // empty bid
            BOOST_CHECK(!d.bid_price);
            BOOST_CHECK(!d.bidder);
            // data integrity
            BOOST_CHECK(d.issuer == alice_id);
            BOOST_CHECK(d.maximum_price == asset(11000));
            BOOST_CHECK(d.minimum_price == asset(10));
            BOOST_CHECK(d.buying_item == true);
            BOOST_CHECK(db.item_locked(nft_id_type(0)) == false);
            BOOST_CHECK(db.item_locked(nft_id_type(1)) == false);
            buy_offer = d.id;
        }
    }
    catch (fc::exception &e)
    {
        edump((e.to_detail_string()));
        throw;
    }
}

BOOST_AUTO_TEST_CASE(sell_bid_for_buy_offer_test)
{
    try
    {
        INVOKE(create_buy_offer_test);
        GET_ACTOR(alice);
        GET_ACTOR(bob);
        GET_ACTOR(operator1);

        const auto &offer_obj = buy_offer(db);

        bid_operation bid_op;
        bid_op.offer_id = offer_obj.id;
        bid_op.bid_price = asset(offer_obj.minimum_price.amount + 2, offer_obj.minimum_price.asset_id);
        bid_op.bidder = bob_id;
        trx.operations.push_back(bid_op);

        auto op = trx.operations.back().get<bid_operation>();
        // Non Owner bidder
        REQUIRE_THROW_WITH_VALUE(op, bidder, alice_id);

        trx.clear();
        trx.operations.push_back(bid_op);
        sign(trx, bob_private_key);
        PUSH_TX(db, trx);
        trx.clear();

        //not empty bid
        BOOST_CHECK(offer_obj.bid_price);
        BOOST_CHECK(offer_obj.bidder);
        // data integrity
        BOOST_CHECK(offer_obj.bidder == bob_id);
        BOOST_CHECK(offer_obj.issuer == alice_id);
        BOOST_CHECK(offer_obj.maximum_price == asset(11000));
        BOOST_CHECK(offer_obj.minimum_price == asset(10));
        BOOST_CHECK(offer_obj.bid_price == bid_op.bid_price);
        BOOST_CHECK(db.item_locked(nft_id_type(0)));
        BOOST_CHECK(db.item_locked(nft_id_type(1)));
    }
    catch (fc::exception &e)
    {
        edump((e.to_detail_string()));
        throw;
    }
}

BOOST_AUTO_TEST_CASE(second_sell_bid_for_buy_offer_test)
{
    try
    {
        INVOKE(sell_bid_for_buy_offer_test);
        GET_ACTOR(alice);
        GET_ACTOR(bob);
        GET_ACTOR(charlie);
        GET_ACTOR(operator1);

        const auto &offer_obj = buy_offer(db);

        bid_operation bid_op;
        bid_op.offer_id = offer_obj.id;
        bid_op.bid_price = asset((*offer_obj.bid_price).amount - 1, offer_obj.minimum_price.asset_id);
        bid_op.bidder = bob_id;
        trx.operations.push_back(bid_op);

        auto op = trx.operations.back().get<bid_operation>();
        // Not a better bid than previous
        REQUIRE_THROW_WITH_VALUE(op, bid_price, asset((*offer_obj.bid_price).amount, offer_obj.minimum_price.asset_id));

        trx.clear();
        trx.operations.push_back(bid_op);
        sign(trx, bob_private_key);
        PUSH_TX(db, trx);
        trx.clear();

        //not empty bid
        BOOST_CHECK(offer_obj.bid_price);
        BOOST_CHECK(offer_obj.bidder);

        // data integrity
        BOOST_CHECK(offer_obj.bidder == bob_id);
        BOOST_CHECK(offer_obj.issuer == alice_id);
        BOOST_CHECK(offer_obj.maximum_price == asset(11000));
        BOOST_CHECK(offer_obj.minimum_price == asset(10));
        BOOST_CHECK(offer_obj.bid_price == bid_op.bid_price);
        BOOST_CHECK(db.item_locked(nft_id_type(0)));
        BOOST_CHECK(db.item_locked(nft_id_type(1)));
    }
    catch (fc::exception &e)
    {
        edump((e.to_detail_string()));
        throw;
    }
}

BOOST_AUTO_TEST_CASE(best_sell_bid_for_buy_offer)
{
    try
    {
        INVOKE(second_sell_bid_for_buy_offer_test);
        GET_ACTOR(alice);
        GET_ACTOR(bob);
        GET_ACTOR(mdowner);

        int64_t bob_balance = get_balance(bob_id(db), asset_id_type()(db));
        int64_t alice_balance = get_balance(alice_id(db), asset_id_type()(db));
        int64_t mdowner_balance = get_balance(mdowner_id(db), asset_id_type()(db));
        const auto &offer_obj = buy_offer(db);

        bid_operation bid_op;
        bid_op.offer_id = offer_obj.id;
        bid_op.bid_price = asset(offer_obj.minimum_price.amount, offer_obj.minimum_price.asset_id);
        bid_op.bidder = bob_id;

        asset bid = bid_op.bid_price;
        asset exp_delta_bidder1 = offer_obj.minimum_price;
        asset exp_delta_bidder2 = offer_obj.maximum_price - offer_obj.minimum_price;

        trx.operations.push_back(bid_op);
        sign(trx, bob_private_key);
        PUSH_TX(db, trx);
        trx.clear();

        //not empty bid
        BOOST_CHECK(offer_obj.bid_price);
        BOOST_CHECK(offer_obj.bidder);
        // data integrity
        BOOST_CHECK(offer_obj.bidder == bob_id);
        BOOST_CHECK(offer_obj.issuer == alice_id);
        BOOST_CHECK(offer_obj.maximum_price == asset(11000));
        BOOST_CHECK(offer_obj.minimum_price == asset(10));
        BOOST_CHECK(offer_obj.bid_price == bid);
        BOOST_CHECK(db.item_locked(nft_id_type(0)));
        BOOST_CHECK(db.item_locked(nft_id_type(1)));
        auto cached_offer_obj = offer_obj;
        // Generate a block and offer should be finalized with bid
        generate_block();
        // Check balances
        int64_t partner_fee = 2 * static_cast<int64_t>((0.1 * (*cached_offer_obj.bid_price).amount.value)/2);
        BOOST_CHECK_EQUAL(get_balance(mdowner_id(db), asset_id_type()(db)),
                            mdowner_balance + partner_fee);
        BOOST_CHECK_EQUAL(get_balance(bob_id(db), asset_id_type()(db)),
                          (bob_balance + exp_delta_bidder1.amount).value - partner_fee);
        BOOST_CHECK_EQUAL(get_balance(alice_id(db), asset_id_type()(db)),
                          (alice_balance + exp_delta_bidder2.amount).value);
        const auto &oidx = db.get_index_type<offer_index>().indices().get<by_id>();
        const auto &ohidx = db.get_index_type<offer_history_index>().indices().get<by_id>();
        BOOST_REQUIRE(oidx.size() == 0);
        BOOST_REQUIRE(ohidx.size() == 2);
        BOOST_CHECK(db.item_locked(nft_id_type(0)) == false);
        BOOST_CHECK(db.item_locked(nft_id_type(1)) == false);
        BOOST_CHECK((nft_id_type(0)(db).owner == alice_id) && (nft_id_type(1)(db).owner == alice_id));
        // Get offer history object
        const auto &history_obj = offer_history_id_type(1)(db);
        // History object data check
        BOOST_CHECK(cached_offer_obj.bid_price == history_obj.bid_price);
        BOOST_CHECK(cached_offer_obj.bidder == history_obj.bidder);
        BOOST_CHECK(cached_offer_obj.buying_item == history_obj.buying_item);
        BOOST_CHECK(cached_offer_obj.issuer == history_obj.issuer);
        BOOST_CHECK(cached_offer_obj.maximum_price == history_obj.maximum_price);
        BOOST_CHECK(cached_offer_obj.minimum_price == history_obj.minimum_price);
        BOOST_CHECK(cached_offer_obj.offer_expiration_date == history_obj.offer_expiration_date);
        BOOST_CHECK(cached_offer_obj.item_ids == history_obj.item_ids);
        BOOST_CHECK(result_type::Expired == history_obj.result);
    }
    catch (fc::exception &e)
    {
        edump((e.to_detail_string()));
        throw;
    }
}

BOOST_AUTO_TEST_CASE(expire_with_bid_for_buy_offer_test)
{
    INVOKE(second_sell_bid_for_buy_offer_test);
    GET_ACTOR(alice);
    GET_ACTOR(bob);
    GET_ACTOR(mdowner);
    int64_t alice_balance = get_balance(alice_id(db), asset_id_type()(db));
    int64_t bob_balance = get_balance(bob_id(db), asset_id_type()(db));
    int64_t mdowner_balance = get_balance(mdowner_id(db), asset_id_type()(db));
    const auto &offer_obj = buy_offer(db);
    auto cached_offer_obj = offer_obj;
    generate_blocks(5);
    int64_t partner_fee = 2 * static_cast<int64_t>((0.1 * (*cached_offer_obj.bid_price).amount.value)/2);
    BOOST_CHECK_EQUAL(get_balance(mdowner_id(db), asset_id_type()(db)),
                            mdowner_balance + partner_fee);
    BOOST_CHECK_EQUAL(get_balance(alice_id(db), asset_id_type()(db)),
                      (alice_balance + cached_offer_obj.maximum_price.amount - (*cached_offer_obj.bid_price).amount).value);
    BOOST_CHECK_EQUAL(get_balance(bob_id(db), asset_id_type()(db)),
                      (bob_balance + (*cached_offer_obj.bid_price).amount).value - partner_fee);
    const auto &oidx = db.get_index_type<offer_index>().indices().get<by_id>();
    const auto &ohidx = db.get_index_type<offer_history_index>().indices().get<by_id>();
    BOOST_REQUIRE(oidx.size() == 0);
    BOOST_REQUIRE(ohidx.size() == 2);
    BOOST_CHECK(db.item_locked(nft_id_type(0)) == false);
    BOOST_CHECK(db.item_locked(nft_id_type(1)) == false);
    BOOST_CHECK((nft_id_type(0)(db).owner == alice_id) && (nft_id_type(1)(db).owner == alice_id));
    // Get offer history object
    const auto &history_obj = offer_history_id_type(1)(db);
    // History object data check
    BOOST_CHECK(cached_offer_obj.bid_price == history_obj.bid_price);
    BOOST_CHECK(cached_offer_obj.bidder == history_obj.bidder);
    BOOST_CHECK(cached_offer_obj.buying_item == history_obj.buying_item);
    BOOST_CHECK(cached_offer_obj.issuer == history_obj.issuer);
    BOOST_CHECK(cached_offer_obj.maximum_price == history_obj.maximum_price);
    BOOST_CHECK(cached_offer_obj.minimum_price == history_obj.minimum_price);
    BOOST_CHECK(cached_offer_obj.offer_expiration_date == history_obj.offer_expiration_date);
    BOOST_CHECK(cached_offer_obj.item_ids == history_obj.item_ids);
    BOOST_CHECK(result_type::Expired == history_obj.result);
}

BOOST_AUTO_TEST_CASE(expire_no_bid_for_buy_offer_test)
{
    INVOKE(create_buy_offer_test);
    GET_ACTOR(alice);
    GET_ACTOR(bob);
    int64_t alice_balance = get_balance(alice_id(db), asset_id_type()(db));
    const auto &offer_obj = buy_offer(db);
    auto cached_offer_obj = offer_obj;
    generate_blocks(5);
    BOOST_CHECK_EQUAL(get_balance(alice_id(db), asset_id_type()(db)),
                      (alice_balance + cached_offer_obj.maximum_price.amount).value);
    const auto &oidx = db.get_index_type<offer_index>().indices().get<by_id>();
    const auto &ohidx = db.get_index_type<offer_history_index>().indices().get<by_id>();
    BOOST_REQUIRE(oidx.size() == 0);
    BOOST_REQUIRE(ohidx.size() == 2);
    BOOST_CHECK(db.item_locked(nft_id_type(0)) == false);
    BOOST_CHECK(db.item_locked(nft_id_type(1)) == false);
    BOOST_CHECK((nft_id_type(0)(db).owner == bob_id) && (nft_id_type(1)(db).owner == bob_id));
    // Get offer history object
    const auto &history_obj = offer_history_id_type(1)(db);
    // History object data check
    BOOST_CHECK(cached_offer_obj.bid_price == history_obj.bid_price);
    BOOST_CHECK(cached_offer_obj.bidder == history_obj.bidder);
    BOOST_CHECK(cached_offer_obj.buying_item == history_obj.buying_item);
    BOOST_CHECK(cached_offer_obj.issuer == history_obj.issuer);
    BOOST_CHECK(cached_offer_obj.maximum_price == history_obj.maximum_price);
    BOOST_CHECK(cached_offer_obj.minimum_price == history_obj.minimum_price);
    BOOST_CHECK(cached_offer_obj.offer_expiration_date == history_obj.offer_expiration_date);
    BOOST_CHECK(cached_offer_obj.item_ids == history_obj.item_ids);
    BOOST_CHECK(result_type::ExpiredNoBid == history_obj.result);
}

BOOST_AUTO_TEST_CASE(cancel_sell_offer_no_bid_test)
{
    try
    {
        INVOKE(create_sell_offer_test);
        GET_ACTOR(alice);
        GET_ACTOR(bob);
        GET_ACTOR(operator1);

        const auto &offer_obj = sell_offer(db);
        auto cached_offer_obj = offer_obj;

        cancel_offer_operation cancel_op;
        cancel_op.offer_id = offer_obj.id;
        // Add non-issuer
        cancel_op.issuer = bob_id;
        trx.clear();
        trx.operations.push_back(cancel_op);
        sign(trx, bob_private_key);
        BOOST_CHECK_THROW(PUSH_TX(db, trx), fc::exception);
        trx.clear();
        // Add issuer
        cancel_op.issuer = alice_id;
        trx.operations.push_back(cancel_op);
        sign(trx, alice_private_key);
        PUSH_TX(db, trx);

        const auto &oidx = db.get_index_type<offer_index>().indices().get<by_id>();
        const auto &ohidx = db.get_index_type<offer_history_index>().indices().get<by_id>();
        BOOST_REQUIRE(oidx.size() == 0);
        BOOST_REQUIRE(ohidx.size() == 1);
        BOOST_CHECK(db.item_locked(nft_id_type(0)) == false);
        BOOST_CHECK(db.item_locked(nft_id_type(1)) == false);
        // Get offer history object
        const auto &history_obj = offer_history_id_type(0)(db);
        // History object data check
        BOOST_CHECK(cached_offer_obj.bid_price == history_obj.bid_price);
        BOOST_CHECK(cached_offer_obj.bidder == history_obj.bidder);
        BOOST_CHECK(cached_offer_obj.buying_item == history_obj.buying_item);
        BOOST_CHECK(cached_offer_obj.issuer == history_obj.issuer);
        BOOST_CHECK(cached_offer_obj.maximum_price == history_obj.maximum_price);
        BOOST_CHECK(cached_offer_obj.minimum_price == history_obj.minimum_price);
        BOOST_CHECK(cached_offer_obj.offer_expiration_date == history_obj.offer_expiration_date);
        BOOST_CHECK(cached_offer_obj.item_ids == history_obj.item_ids);
        BOOST_CHECK(result_type::Cancelled == history_obj.result);

    }
    catch (fc::exception &e)
    {
        edump((e.to_detail_string()));
        throw;
    }
}

BOOST_AUTO_TEST_CASE(cancel_sell_offer_with_bid_test)
{
    try
    {
        INVOKE(buy_bid_for_sell_offer_test);
        GET_ACTOR(alice);
        GET_ACTOR(bob);
        GET_ACTOR(operator1);

        const auto &offer_obj = sell_offer(db);
        auto cached_offer_obj = offer_obj;
        int64_t bob_balance = get_balance(bob_id(db), asset_id_type()(db));

        cancel_offer_operation cancel_op;
        cancel_op.offer_id = offer_obj.id;
        // Add issuer
        cancel_op.issuer = alice_id;
        trx.clear();
        trx.operations.push_back(cancel_op);
        sign(trx, alice_private_key);
        PUSH_TX(db, trx);

        const auto &oidx = db.get_index_type<offer_index>().indices().get<by_id>();
        const auto &ohidx = db.get_index_type<offer_history_index>().indices().get<by_id>();
        BOOST_REQUIRE(oidx.size() == 0);
        BOOST_REQUIRE(ohidx.size() == 1);
        BOOST_CHECK(db.item_locked(nft_id_type(0)) == false);
        BOOST_CHECK(db.item_locked(nft_id_type(1)) == false);
        BOOST_CHECK_EQUAL(get_balance(bob_id(db), asset_id_type()(db)),
                    (bob_balance + (*cached_offer_obj.bid_price).amount).value);
        // Get offer history object
        const auto &history_obj = offer_history_id_type(0)(db);
        // History object data check
        BOOST_CHECK(cached_offer_obj.bid_price == history_obj.bid_price);
        BOOST_CHECK(cached_offer_obj.bidder == history_obj.bidder);
        BOOST_CHECK(cached_offer_obj.buying_item == history_obj.buying_item);
        BOOST_CHECK(cached_offer_obj.issuer == history_obj.issuer);
        BOOST_CHECK(cached_offer_obj.maximum_price == history_obj.maximum_price);
        BOOST_CHECK(cached_offer_obj.minimum_price == history_obj.minimum_price);
        BOOST_CHECK(cached_offer_obj.offer_expiration_date == history_obj.offer_expiration_date);
        BOOST_CHECK(cached_offer_obj.item_ids == history_obj.item_ids);
        BOOST_CHECK(result_type::Cancelled == history_obj.result);

    }
    catch (fc::exception &e)
    {
        edump((e.to_detail_string()));
        throw;
    }
}

BOOST_AUTO_TEST_CASE(cancel_buy_offer_with_bid_test)
{
    try
    {
        INVOKE(sell_bid_for_buy_offer_test);
        GET_ACTOR(alice);
        GET_ACTOR(bob);
        GET_ACTOR(operator1);

        const auto &offer_obj = buy_offer(db);
        auto cached_offer_obj = offer_obj;
        int64_t alice_balance = get_balance(alice_id(db), asset_id_type()(db));

        cancel_offer_operation cancel_op;
        cancel_op.offer_id = offer_obj.id;
        cancel_op.issuer = alice_id;

        trx.clear();
        trx.operations.push_back(cancel_op);
        sign(trx, alice_private_key);
        PUSH_TX(db, trx);
        trx.clear();

        generate_block();

        const auto &oidx = db.get_index_type<offer_index>().indices().get<by_id>();
        const auto &ohidx = db.get_index_type<offer_history_index>().indices().get<by_id>();
        BOOST_REQUIRE(oidx.size() == 0);
        BOOST_REQUIRE(ohidx.size() == 2);
        BOOST_CHECK(db.item_locked(nft_id_type(0)) == false);
        BOOST_CHECK(db.item_locked(nft_id_type(1)) == false);
        BOOST_CHECK_EQUAL(get_balance(alice_id(db), asset_id_type()(db)),
                      (alice_balance + cached_offer_obj.maximum_price.amount).value);
        // Get offer history object
        const auto &history_obj = offer_history_id_type(1)(db);
        // History object data check
        BOOST_CHECK(cached_offer_obj.bid_price == history_obj.bid_price);
        BOOST_CHECK(cached_offer_obj.bidder == history_obj.bidder);
        BOOST_CHECK(cached_offer_obj.buying_item == history_obj.buying_item);
        BOOST_CHECK(cached_offer_obj.issuer == history_obj.issuer);
        BOOST_CHECK(cached_offer_obj.maximum_price == history_obj.maximum_price);
        BOOST_CHECK(cached_offer_obj.minimum_price == history_obj.minimum_price);
        BOOST_CHECK(cached_offer_obj.offer_expiration_date == history_obj.offer_expiration_date);
        BOOST_CHECK(cached_offer_obj.item_ids == history_obj.item_ids);
        BOOST_CHECK(result_type::Cancelled == history_obj.result);

    }
    catch (fc::exception &e)
    {
        edump((e.to_detail_string()));
        throw;
    }
}

BOOST_AUTO_TEST_SUITE_END()
