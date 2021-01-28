/*
 * Copyright (c) 2017 PBSA, Inc., and contributors.
 *
 * The MIT License
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */

#include <boost/test/unit_test.hpp>

#include <graphene/chain/database.hpp>

#include <fc/crypto/digest.hpp>
#include <fc/crypto/elliptic.hpp>
#include <fc/reflect/variant.hpp>

#include "../common/database_fixture.hpp"

#include <graphene/chain/hardfork.hpp>
#include <graphene/chain/nft_object.hpp>

using namespace graphene::chain;
using namespace graphene::chain::test;

BOOST_FIXTURE_TEST_SUITE(nft_lottery_tests, database_fixture)

BOOST_AUTO_TEST_CASE(create_lottery_nft_md_test)
{
    try
    {
        generate_blocks(HARDFORK_NFT_TIME);
        generate_block();
        generate_block();
        set_expiration(db, trx);
        // Lottery Options
        nft_metadata_id_type test_nft_md_id = db.get_index<nft_metadata_object>().get_next_id();
        nft_lottery_options lottery_options;
        lottery_options.benefactors.push_back(nft_lottery_benefactor(account_id_type(), 25 * GRAPHENE_1_PERCENT));
        lottery_options.end_date = db.head_block_time() + fc::minutes(5);
        lottery_options.ticket_price = asset(100);
        lottery_options.winning_tickets = {5 * GRAPHENE_1_PERCENT, 5 * GRAPHENE_1_PERCENT, 5 * GRAPHENE_1_PERCENT, 10 * GRAPHENE_1_PERCENT, 10 * GRAPHENE_1_PERCENT, 10 * GRAPHENE_1_PERCENT, 10 * GRAPHENE_1_PERCENT, 10 * GRAPHENE_1_PERCENT, 10 * GRAPHENE_1_PERCENT};
        //lottery_options.winning_tickets = { 75 * GRAPHENE_1_PERCENT };
        lottery_options.is_active = test_nft_md_id.instance.value % 2 ? false : true;
        lottery_options.ending_on_soldout = true;

        {
            BOOST_TEST_MESSAGE("Send nft_metadata_create_operation");
            nft_metadata_create_operation op;
            op.owner = account_id_type();
            op.symbol = "NFTLOTTERY" + std::to_string(test_nft_md_id.instance.value);
            op.base_uri = "http://nft.example.com";
            op.is_transferable = true;
            op.name = "NFTLOTTERY" + std::to_string(test_nft_md_id.instance.value);
            op.max_supply = 200;
            op.lottery_options = lottery_options;

            trx.operations.push_back(std::move(op));
            PUSH_TX(db, trx, ~0);
        }
        generate_block();

        BOOST_TEST_MESSAGE("Check nft_metadata_create_operation results");

        const auto &obj = test_nft_md_id(db);
        BOOST_CHECK(obj.owner == account_id_type());
        BOOST_CHECK(obj.name == "NFTLOTTERY" + std::to_string(test_nft_md_id.instance.value));
        BOOST_CHECK(obj.symbol == "NFTLOTTERY" + std::to_string(test_nft_md_id.instance.value));
        BOOST_CHECK(obj.base_uri == "http://nft.example.com");
        BOOST_CHECK(obj.max_supply == share_type(200));
        BOOST_CHECK(obj.is_lottery());
        BOOST_CHECK(obj.get_token_current_supply(db) == share_type(0));
        BOOST_CHECK(obj.get_lottery_jackpot(db) == asset());
        BOOST_CHECK(obj.lottery_data->lottery_balance_id(db).sweeps_tickets_sold == share_type(0));
    }
    catch (fc::exception &e)
    {
        edump((e.to_detail_string()));
        throw;
    }
}

BOOST_AUTO_TEST_CASE(lottery_idx_test)
{
    try
    {
        // generate loterries with different end_dates and is_active_flag
        for (int i = 0; i < 26; ++i)
        {
            generate_blocks(30);
            graphene::chain::test::set_expiration(db, trx);
            nft_metadata_id_type test_nft_md_id = db.get_index<nft_metadata_object>().get_next_id();
            INVOKE(create_lottery_nft_md_test);
            auto test_nft_md_obj = test_nft_md_id(db);
        }

        auto &test_nft_md_idx = db.get_index_type<nft_metadata_index>().indices().get<active_nft_lotteries>();
        auto test_itr = test_nft_md_idx.begin();
        bool met_not_active = false;
        // check sorting
        while (test_itr != test_nft_md_idx.end())
        {
            if (!met_not_active && (!test_itr->is_lottery() || !test_itr->lottery_data->lottery_options.is_active))
                met_not_active = true;
            FC_ASSERT(!met_not_active || met_not_active && (!test_itr->is_lottery() || !test_itr->lottery_data->lottery_options.is_active), "MET ACTIVE LOTTERY AFTER NOT ACTIVE");
            ++test_itr;
        }
    }
    catch (fc::exception &e)
    {
        edump((e.to_detail_string()));
        throw;
    }
}

BOOST_AUTO_TEST_CASE(tickets_purchase_test)
{
    try
    {
        nft_metadata_id_type test_nft_md_id = db.get_index<nft_metadata_object>().get_next_id();
        nft_id_type test_nft_id = db.get_index<nft_object>().get_next_id();
        INVOKE(create_lottery_nft_md_test);
        auto &test_nft_md_obj = test_nft_md_id(db);

        nft_lottery_token_purchase_operation tpo;
        tpo.fee = asset();
        tpo.buyer = account_id_type();
        tpo.lottery_id = test_nft_md_obj.id;
        tpo.tickets_to_buy = 1;
        tpo.amount = asset(100);
        trx.operations.push_back(std::move(tpo));
        set_expiration(db, trx);
        PUSH_TX(db, trx, ~0);
        generate_block();
        trx.operations.clear();
        auto &test_nft_ticket = test_nft_id(db);
        BOOST_CHECK(test_nft_md_obj.get_token_current_supply(db) == tpo.tickets_to_buy);
        BOOST_CHECK(test_nft_ticket.owner == tpo.buyer);
        BOOST_CHECK(test_nft_ticket.nft_metadata_id == test_nft_md_obj.id);
    }
    catch (fc::exception &e)
    {
        edump((e.to_detail_string()));
        throw;
    }
}

BOOST_AUTO_TEST_CASE(tickets_purchase_fail_test)
{
    try
    {
        nft_metadata_id_type test_nft_md_id = db.get_index<nft_metadata_object>().get_next_id();
        INVOKE(create_lottery_nft_md_test);
        auto &test_nft_md_obj = test_nft_md_id(db);

        nft_lottery_token_purchase_operation tpo;
        tpo.fee = asset();
        tpo.buyer = account_id_type();
        tpo.lottery_id = test_nft_md_obj.id;
        tpo.tickets_to_buy = 2;
        tpo.amount = asset(100);
        trx.operations.push_back(tpo);
        BOOST_REQUIRE_THROW(PUSH_TX(db, trx, ~0), fc::exception); // amount/tickets_to_buy != price
        trx.operations.clear();

        tpo.amount = asset(205);
        trx.operations.push_back(tpo);
        BOOST_REQUIRE_THROW(PUSH_TX(db, trx, ~0), fc::exception); // amount/tickets_to_buy != price

        tpo.amount = asset(200, asset_id_type(1));
        trx.operations.push_back(tpo);
        BOOST_REQUIRE_THROW(PUSH_TX(db, trx, ~0), fc::exception); // trying to buy in other asset
    }
    catch (fc::exception &e)
    {
        edump((e.to_detail_string()));
        throw;
    }
}

BOOST_AUTO_TEST_CASE(lottery_end_by_stage_test)
{
    try
    {
        nft_metadata_id_type test_nft_md_id = db.get_index<nft_metadata_object>().get_next_id();
        INVOKE(create_lottery_nft_md_test);
        auto test_nft_md_obj = test_nft_md_id(db);
        for (int i = 1; i < 17; ++i)
        {
            if (i == 4 || i == 1 || i == 16 || i == 15)
                continue;
            if (i != 0)
                transfer(account_id_type(), account_id_type(i), asset(100000));
            nft_lottery_token_purchase_operation tpo;
            tpo.fee = asset();
            tpo.buyer = account_id_type(i);
            tpo.lottery_id = test_nft_md_obj.id;
            tpo.tickets_to_buy = i;
            tpo.amount = asset(100 * (i));
            trx.operations.push_back(std::move(tpo));
            set_expiration(db, trx);
            PUSH_TX(db, trx, ~0);
            generate_block();
            trx.operations.clear();
        }

        test_nft_md_obj = test_nft_md_id(db);
        uint64_t benefactor_balance_before_end = db.get_balance(account_id_type(), asset_id_type()).amount.value;
        uint64_t jackpot = test_nft_md_obj.get_lottery_jackpot(db).amount.value;
        uint16_t winners_part = 0;
        for (uint16_t win : test_nft_md_obj.lottery_data->lottery_options.winning_tickets)
            winners_part += win;

        uint16_t participants_percents_sum = 0;
        auto participants = test_nft_md_obj.distribute_winners_part(db);
        for (auto p : participants)
        {
            for (auto e : p.second)
            {
                participants_percents_sum += e;
            }
        }

        test_nft_md_obj = test_nft_md_id(db);
        BOOST_CHECK(participants_percents_sum == winners_part);
        BOOST_CHECK(test_nft_md_obj.get_lottery_jackpot(db).amount.value == (jackpot * (GRAPHENE_100_PERCENT - winners_part) / (double)GRAPHENE_100_PERCENT) + jackpot * winners_part * SWEEPS_DEFAULT_DISTRIBUTION_PERCENTAGE / (double)GRAPHENE_100_PERCENT / (double)GRAPHENE_100_PERCENT);
        test_nft_md_obj.distribute_benefactors_part(db);
        test_nft_md_obj = test_nft_md_id(db);
        BOOST_CHECK(test_nft_md_obj.get_lottery_jackpot(db).amount.value == jackpot * SWEEPS_DEFAULT_DISTRIBUTION_PERCENTAGE / (double)GRAPHENE_100_PERCENT * winners_part / (double)GRAPHENE_100_PERCENT);
        test_nft_md_obj.distribute_sweeps_holders_part(db);
        test_nft_md_obj = test_nft_md_id(db);
        BOOST_CHECK(test_nft_md_obj.get_lottery_jackpot(db).amount.value == 0);

        uint64_t benefactor_recieved = db.get_balance(account_id_type(), asset_id_type()).amount.value - benefactor_balance_before_end;
        BOOST_CHECK(jackpot * test_nft_md_obj.lottery_data->lottery_options.benefactors[0].share / GRAPHENE_100_PERCENT == benefactor_recieved);
    }
    catch (fc::exception &e)
    {
        edump((e.to_detail_string()));
        throw;
    }
}

BOOST_AUTO_TEST_CASE(lottery_end_by_stage_with_fractional_test)
{

    try
    {
        nft_metadata_id_type test_nft_md_id = db.get_index<nft_metadata_object>().get_next_id();
        INVOKE(create_lottery_nft_md_test);
        db.modify(test_nft_md_id(db), [&](nft_metadata_object &obj) {
            obj.lottery_data->lottery_options.is_active = true;
        });
        auto test_nft_md_obj = test_nft_md_id(db);

        for (int i = 1; i < 17; ++i)
        {
            if (i == 4)
                continue;
            if (i != 0)
                transfer(account_id_type(), account_id_type(i), asset(100000));
            nft_lottery_token_purchase_operation tpo;
            tpo.fee = asset();
            tpo.buyer = account_id_type(i);
            tpo.lottery_id = test_nft_md_obj.id;
            tpo.tickets_to_buy = i;
            tpo.amount = asset(100 * (i));
            trx.operations.push_back(std::move(tpo));
            set_expiration(db, trx);
            PUSH_TX(db, trx, ~0);
            generate_block();
            trx.operations.clear();
        }
        test_nft_md_obj = test_nft_md_id(db);
        uint64_t benefactor_balance_before_end = db.get_balance(account_id_type(), asset_id_type()).amount.value;
        uint64_t jackpot = test_nft_md_obj.get_lottery_jackpot(db).amount.value;
        uint16_t winners_part = 0;
        for (uint16_t win : test_nft_md_obj.lottery_data->lottery_options.winning_tickets)
            winners_part += win;

        uint16_t participants_percents_sum = 0;
        auto participants = test_nft_md_obj.distribute_winners_part(db);
        for (auto p : participants)
            for (auto e : p.second)
                participants_percents_sum += e;

        BOOST_CHECK(participants_percents_sum == winners_part);
        // balance should be bigger than expected because of rouning during distribution
        test_nft_md_obj = test_nft_md_id(db);
        BOOST_CHECK(test_nft_md_obj.get_lottery_jackpot(db).amount.value > (jackpot * (GRAPHENE_100_PERCENT - winners_part) / (double)GRAPHENE_100_PERCENT) + jackpot * winners_part * SWEEPS_DEFAULT_DISTRIBUTION_PERCENTAGE / (double)GRAPHENE_100_PERCENT / (double)GRAPHENE_100_PERCENT);
        test_nft_md_obj.distribute_benefactors_part(db);
        test_nft_md_obj = test_nft_md_id(db);
        BOOST_CHECK(test_nft_md_obj.get_lottery_jackpot(db).amount.value > jackpot * SWEEPS_DEFAULT_DISTRIBUTION_PERCENTAGE / (double)GRAPHENE_100_PERCENT * winners_part / (double)GRAPHENE_100_PERCENT);
        test_nft_md_obj.distribute_sweeps_holders_part(db);
        test_nft_md_obj = test_nft_md_id(db);
        // but at the end is always equals 0
        BOOST_CHECK(test_nft_md_obj.get_lottery_jackpot(db).amount.value == 0);

        uint64_t benefactor_recieved = db.get_balance(account_id_type(), asset_id_type()).amount.value - benefactor_balance_before_end;
        test_nft_md_obj = test_nft_md_id(db);
        BOOST_CHECK(jackpot * test_nft_md_obj.lottery_data->lottery_options.benefactors[0].share / GRAPHENE_100_PERCENT == benefactor_recieved);
    }
    catch (fc::exception &e)
    {
        edump((e.to_detail_string()));
        throw;
    }
}

BOOST_AUTO_TEST_CASE(lottery_end_test)
{
    try
    {
        nft_metadata_id_type test_nft_md_id = db.get_index<nft_metadata_object>().get_next_id();
        INVOKE(create_lottery_nft_md_test);
        auto test_nft_md_obj = test_nft_md_id(db);
        for (int i = 1; i < 17; ++i)
        {
            if (i == 4 || i == 1 || i == 16 || i == 15)
                continue;
            if (i != 0)
                transfer(account_id_type(), account_id_type(i), asset(100000));
            nft_lottery_token_purchase_operation tpo;
            tpo.fee = asset();
            tpo.buyer = account_id_type(i);
            tpo.lottery_id = test_nft_md_obj.id;
            tpo.tickets_to_buy = i;
            tpo.amount = asset(100 * (i));
            trx.operations.push_back(std::move(tpo));
            set_expiration(db, trx);
            PUSH_TX(db, trx, ~0);
            trx.operations.clear();
        }
        generate_block();
        test_nft_md_obj = test_nft_md_id(db);
        uint64_t creator_balance_before_end = db.get_balance(account_id_type(), asset_id_type()).amount.value;
        uint64_t jackpot = test_nft_md_obj.get_lottery_jackpot(db).amount.value;
        uint16_t winners_part = 0;
        for (uint8_t win : test_nft_md_obj.lottery_data->lottery_options.winning_tickets)
            winners_part += win;

        while (db.head_block_time() < (test_nft_md_obj.lottery_data->lottery_options.end_date + fc::seconds(30)))
            generate_block();
        test_nft_md_obj = test_nft_md_id(db);
        BOOST_CHECK(test_nft_md_obj.get_lottery_jackpot(db).amount.value == 0);
        uint64_t creator_recieved = db.get_balance(account_id_type(), asset_id_type()).amount.value - creator_balance_before_end;
        BOOST_CHECK(jackpot * test_nft_md_obj.lottery_data->lottery_options.benefactors[0].share / GRAPHENE_100_PERCENT == creator_recieved);
    }
    catch (fc::exception &e)
    {
        edump((e.to_detail_string()));
        throw;
    }
}

BOOST_AUTO_TEST_CASE(claim_sweeps_vesting_balance_test)
{
    try
    {
        nft_metadata_id_type test_nft_md_id = db.get_index<nft_metadata_object>().get_next_id();
        INVOKE(lottery_end_test);
        auto test_nft_md_obj = test_nft_md_id(db);
        account_id_type benefactor = test_nft_md_obj.lottery_data->lottery_options.benefactors[0].id;
        const auto &svbo_index = db.get_index_type<sweeps_vesting_balance_index>().indices().get<by_owner>();
        auto benefactor_svbo = svbo_index.find(benefactor);
        BOOST_CHECK(benefactor_svbo != svbo_index.end());

        auto balance_before_claim = db.get_balance(benefactor, SWEEPS_DEFAULT_DISTRIBUTION_ASSET);
        auto available_for_claim = benefactor_svbo->available_for_claim();
        sweeps_vesting_claim_operation claim;
        claim.account = benefactor;
        claim.amount_to_claim = available_for_claim;
        trx.clear();
        graphene::chain::test::set_expiration(db, trx);
        trx.operations.push_back(claim);
        PUSH_TX(db, trx, ~0);
        generate_block();

        BOOST_CHECK(db.get_balance(benefactor, SWEEPS_DEFAULT_DISTRIBUTION_ASSET) - balance_before_claim == available_for_claim);
        benefactor_svbo = svbo_index.find(benefactor);
        BOOST_CHECK(benefactor_svbo->available_for_claim().amount == 0);
    }
    catch (fc::exception &e)
    {
        edump((e.to_detail_string()));
        throw;
    }
}

BOOST_AUTO_TEST_CASE(more_winners_then_participants_test)
{
    try
    {
        nft_metadata_id_type test_nft_md_id = db.get_index<nft_metadata_object>().get_next_id();
        INVOKE(create_lottery_nft_md_test);
        auto test_nft_md_obj = test_nft_md_id(db);
        for (int i = 1; i < 4; ++i)
        {
            if (i == 4)
                continue;
            if (i != 0)
                transfer(account_id_type(), account_id_type(i), asset(1000000));
            nft_lottery_token_purchase_operation tpo;
            tpo.fee = asset();
            tpo.buyer = account_id_type(i);
            tpo.lottery_id = test_nft_md_obj.id;
            tpo.tickets_to_buy = 1;
            tpo.amount = asset(100);
            trx.operations.push_back(std::move(tpo));
            set_expiration(db, trx);
            PUSH_TX(db, trx, ~0);
            trx.operations.clear();
        }
        generate_block();
        test_nft_md_obj = test_nft_md_id(db);
        auto holders = test_nft_md_obj.get_holders(db);
        auto participants = test_nft_md_obj.distribute_winners_part(db);
        test_nft_md_obj = test_nft_md_id(db);
        test_nft_md_obj.distribute_benefactors_part(db);
        test_nft_md_obj = test_nft_md_id(db);
        test_nft_md_obj.distribute_sweeps_holders_part(db);
        test_nft_md_obj = test_nft_md_id(db);
        generate_block();
        for (auto p : participants)
        {
            idump((get_operation_history(p.first)));
        }
        auto benefactor_history = get_operation_history(account_id_type());
        for (auto h : benefactor_history)
        {
            idump((h));
        }
    }
    catch (fc::exception &e)
    {
        edump((e.to_detail_string()));
        throw;
    }
}

BOOST_AUTO_TEST_CASE(ending_by_date_test)
{
    try
    {
        nft_metadata_id_type test_nft_md_id = db.get_index<nft_metadata_object>().get_next_id();
        INVOKE(create_lottery_nft_md_test);
        auto test_nft_md_obj = test_nft_md_id(db);
        for (int i = 1; i < 4; ++i)
        {
            if (i == 4)
                continue;
            if (i != 0)
                transfer(account_id_type(), account_id_type(i), asset(1000000));
            nft_lottery_token_purchase_operation tpo;
            tpo.fee = asset();
            tpo.buyer = account_id_type(i);
            tpo.lottery_id = test_nft_md_obj.id;
            tpo.tickets_to_buy = 1;
            tpo.amount = asset(100);
            trx.operations.push_back(std::move(tpo));
            set_expiration(db, trx);
            PUSH_TX(db, trx, ~0);
            trx.operations.clear();
        }
        generate_block();
        test_nft_md_obj = test_nft_md_id(db);
        auto holders = test_nft_md_obj.get_holders(db);
        idump((test_nft_md_obj.get_lottery_jackpot(db)));
        while (db.head_block_time() < (test_nft_md_obj.lottery_data->lottery_options.end_date + fc::seconds(30)))
            generate_block();
        test_nft_md_obj = test_nft_md_id(db);
        idump((test_nft_md_obj.get_lottery_jackpot(db)));
        vector<account_id_type> participants = {account_id_type(1), account_id_type(2), account_id_type(3)};
        for (auto p : participants)
        {
            idump((get_operation_history(p)));
        }
        auto benefactor_history = get_operation_history(account_id_type());
        for (auto h : benefactor_history)
        {
            if (h.op.which() == operation::tag<lottery_reward_operation>::value)
            {
                auto reward_op = h.op.get<lottery_reward_operation>();
                idump((reward_op));
                BOOST_CHECK(reward_op.is_benefactor_reward);
                BOOST_CHECK(reward_op.amount.amount.value == 75);
                BOOST_CHECK(reward_op.amount.asset_id == test_nft_md_obj.lottery_data->lottery_options.ticket_price.asset_id);
                break;
            }
        }
    }
    catch (fc::exception &e)
    {
        edump((e.to_detail_string()));
        throw;
    }
}

BOOST_AUTO_TEST_CASE(ending_by_participants_count_test)
{
    try
    {
        nft_metadata_id_type test_nft_md_id = db.get_index<nft_metadata_object>().get_next_id();
        INVOKE(create_lottery_nft_md_test);
        auto test_nft_md_obj = test_nft_md_id(db);
        FC_ASSERT(test_nft_md_obj.lottery_data->lottery_options.is_active);
        account_id_type buyer(3);
        transfer(account_id_type(), buyer, asset(10000000));
        nft_lottery_token_purchase_operation tpo;
        tpo.fee = asset();
        tpo.buyer = buyer;
        tpo.lottery_id = test_nft_md_obj.id;
        tpo.tickets_to_buy = 200;
        tpo.amount = asset(200 * 100);
        trx.operations.push_back(tpo);
        set_expiration(db, trx);
        PUSH_TX(db, trx, ~0);
        trx.operations.clear();
        generate_block();
        test_nft_md_obj = test_nft_md_id(db);
        FC_ASSERT(!test_nft_md_obj.lottery_data->lottery_options.is_active);
    }
    catch (fc::exception &e)
    {
        edump((e.to_detail_string()));
        throw;
    }
}

BOOST_AUTO_TEST_CASE(try_to_end_empty_lottery_test)
{
    try
    {
        nft_metadata_id_type test_nft_md_id = db.get_index<nft_metadata_object>().get_next_id();
        INVOKE(create_lottery_nft_md_test);
        auto test_nft_md_obj = test_nft_md_id(db);
        while (db.head_block_time() < (test_nft_md_obj.lottery_data->lottery_options.end_date + fc::seconds(30)))
            generate_block();
        test_nft_md_obj = test_nft_md_id(db);
        BOOST_CHECK(!test_nft_md_obj.lottery_data->lottery_options.is_active);
    }
    catch (fc::exception &e)
    {
        edump((e.to_detail_string()));
        throw;
    }
}

BOOST_AUTO_TEST_CASE(lottery_winner_ticket_id_test)
{
    try
    {
        nft_metadata_id_type test_nft_md_id = db.get_index<nft_metadata_object>().get_next_id();
        INVOKE(create_lottery_nft_md_test);
        auto test_nft_md_obj = test_nft_md_id(db);
        for (int i = 1; i < 4; ++i)
        {
            transfer(account_id_type(), account_id_type(i), asset(2000000));
        }
        for (int i = 1; i < 4; ++i)
        {
            if (i == 4)
                continue;
            nft_lottery_token_purchase_operation tpo;
            tpo.buyer = account_id_type(i);
            tpo.lottery_id = test_nft_md_obj.id;
            tpo.tickets_to_buy = 1;
            tpo.amount = asset(100);
            trx.operations.push_back(std::move(tpo));
            set_expiration(db, trx);
            PUSH_TX(db, trx, ~0);
            trx.operations.clear();
        }

        for (int i = 1; i < 4; ++i)
        {
            if (i == 4)
                continue;
            nft_lottery_token_purchase_operation tpo;
            tpo.buyer = account_id_type(i);
            tpo.lottery_id = test_nft_md_obj.id;
            tpo.tickets_to_buy = 1;
            tpo.amount = asset(100);
            trx.operations.push_back(std::move(tpo));
            set_expiration(db, trx);
            PUSH_TX(db, trx, ~0);
            trx.operations.clear();
        }
        generate_block();
        test_nft_md_obj = test_nft_md_id(db);
        uint64_t creator_balance_before_end = db.get_balance(account_id_type(), asset_id_type()).amount.value;
        uint64_t jackpot = test_nft_md_obj.get_lottery_jackpot(db).amount.value;
        uint16_t winners_part = 0;
        for (uint8_t win : test_nft_md_obj.lottery_data->lottery_options.winning_tickets)
            winners_part += win;

        while (db.head_block_time() < (test_nft_md_obj.lottery_data->lottery_options.end_date))
            generate_block();
        auto op_history = get_operation_history(account_id_type(1)); //Can observe operation 79 to verify winner ticket number
        for (auto h : op_history)
        {
            idump((h));
        }
        test_nft_md_obj = test_nft_md_id(db);
        BOOST_CHECK(test_nft_md_obj.get_lottery_jackpot(db).amount.value == 0);
        uint64_t creator_recieved = db.get_balance(account_id_type(), asset_id_type()).amount.value - creator_balance_before_end;
        BOOST_CHECK(jackpot * test_nft_md_obj.lottery_data->lottery_options.benefactors[0].share / GRAPHENE_100_PERCENT == creator_recieved);
    }
    catch (fc::exception &e)
    {
        edump((e.to_detail_string()));
        throw;
    }
}

BOOST_AUTO_TEST_CASE(create_lottery_nft_md_delete_tickets_on_draw_test)
{
    try
    {
        generate_blocks(HARDFORK_NFT_TIME);
        generate_block();
        generate_block();
        set_expiration(db, trx);
        // Lottery Options
        nft_metadata_id_type test_nft_md_id = db.get_index<nft_metadata_object>().get_next_id();
        nft_lottery_options lottery_options;
        lottery_options.benefactors.push_back(nft_lottery_benefactor(account_id_type(), 25 * GRAPHENE_1_PERCENT));
        lottery_options.end_date = db.head_block_time() + fc::minutes(5);
        lottery_options.ticket_price = asset(100);
        lottery_options.winning_tickets = {5 * GRAPHENE_1_PERCENT, 5 * GRAPHENE_1_PERCENT, 5 * GRAPHENE_1_PERCENT, 10 * GRAPHENE_1_PERCENT, 10 * GRAPHENE_1_PERCENT, 10 * GRAPHENE_1_PERCENT, 10 * GRAPHENE_1_PERCENT, 10 * GRAPHENE_1_PERCENT, 10 * GRAPHENE_1_PERCENT};
        //lottery_options.winning_tickets = { 75 * GRAPHENE_1_PERCENT };
        lottery_options.is_active = test_nft_md_id.instance.value % 2 ? false : true;
        lottery_options.ending_on_soldout = true;
        lottery_options.delete_tickets_after_draw = true;

        {
            BOOST_TEST_MESSAGE("Send nft_metadata_create_operation");
            nft_metadata_create_operation op;
            op.owner = account_id_type();
            op.symbol = "NFTLOTTERY" + std::to_string(test_nft_md_id.instance.value);
            op.base_uri = "http://nft.example.com";
            op.is_transferable = true;
            op.name = "NFTLOTTERY" + std::to_string(test_nft_md_id.instance.value);
            op.max_supply = 200;
            op.lottery_options = lottery_options;

            trx.operations.push_back(std::move(op));
            PUSH_TX(db, trx, ~0);
            trx.operations.clear();
        }
        generate_block();

        BOOST_TEST_MESSAGE("Check nft_metadata_create_operation results");

        auto test_nft_md_obj = test_nft_md_id(db);
        BOOST_CHECK(test_nft_md_obj.owner == account_id_type());
        BOOST_CHECK(test_nft_md_obj.name == "NFTLOTTERY" + std::to_string(test_nft_md_id.instance.value));
        BOOST_CHECK(test_nft_md_obj.symbol == "NFTLOTTERY" + std::to_string(test_nft_md_id.instance.value));
        BOOST_CHECK(test_nft_md_obj.base_uri == "http://nft.example.com");
        BOOST_CHECK(test_nft_md_obj.max_supply == share_type(200));
        BOOST_CHECK(test_nft_md_obj.is_lottery());
        BOOST_CHECK(test_nft_md_obj.get_token_current_supply(db) == share_type(0));
        BOOST_CHECK(test_nft_md_obj.get_lottery_jackpot(db) == asset());
        BOOST_CHECK(test_nft_md_obj.lottery_data->lottery_balance_id(db).sweeps_tickets_sold == share_type(0));

        BOOST_CHECK(test_nft_md_obj.lottery_data->lottery_options.is_active);
        account_id_type buyer(3);
        transfer(account_id_type(), buyer, asset(10000000));
        {
            nft_lottery_token_purchase_operation tpo;
            tpo.fee = asset();
            tpo.buyer = buyer;
            tpo.lottery_id = test_nft_md_obj.id;
            tpo.tickets_to_buy = 199;
            tpo.amount = asset(199 * 100);
            trx.operations.push_back(tpo);
            set_expiration(db, trx);
            PUSH_TX(db, trx, ~0);
            trx.operations.clear();
        }
        generate_block();
        test_nft_md_obj = test_nft_md_id(db);
        BOOST_CHECK(test_nft_md_obj.lottery_data->lottery_options.is_active);
        BOOST_CHECK(test_nft_md_obj.get_token_current_supply(db) == 199);
        {
            nft_lottery_token_purchase_operation tpo;
            tpo.fee = asset();
            tpo.buyer = buyer;
            tpo.lottery_id = test_nft_md_obj.id;
            tpo.tickets_to_buy = 1;
            tpo.amount = asset(1 * 100);
            trx.operations.push_back(tpo);
            set_expiration(db, trx);
            PUSH_TX(db, trx, ~0);
            trx.operations.clear();
        }
        generate_block();
        test_nft_md_obj = test_nft_md_id(db);
        BOOST_CHECK(!test_nft_md_obj.lottery_data->lottery_options.is_active);
        BOOST_CHECK(test_nft_md_obj.get_token_current_supply(db) == 0);
    }
    catch (fc::exception &e)
    {
        edump((e.to_detail_string()));
        throw;
    }
}

BOOST_AUTO_TEST_CASE(create_lottery_nft_with_permission_test)
{
    try
    {
        generate_blocks(HARDFORK_NFT_TIME);
        generate_block();
        generate_block();
        set_expiration(db, trx);
        // Lottery Options
        nft_metadata_id_type test_nft_md_id = db.get_index<nft_metadata_object>().get_next_id();
        nft_lottery_options lottery_options;
        lottery_options.benefactors.push_back(nft_lottery_benefactor(account_id_type(), 25 * GRAPHENE_1_PERCENT));
        lottery_options.end_date = db.head_block_time() + fc::minutes(5);
        lottery_options.ticket_price = asset(100);
        lottery_options.winning_tickets = {5 * GRAPHENE_1_PERCENT, 5 * GRAPHENE_1_PERCENT, 5 * GRAPHENE_1_PERCENT, 10 * GRAPHENE_1_PERCENT, 10 * GRAPHENE_1_PERCENT, 10 * GRAPHENE_1_PERCENT, 10 * GRAPHENE_1_PERCENT, 10 * GRAPHENE_1_PERCENT, 10 * GRAPHENE_1_PERCENT};
        //lottery_options.winning_tickets = { 75 * GRAPHENE_1_PERCENT };
        lottery_options.is_active = test_nft_md_id.instance.value % 2 ? false : true;
        lottery_options.ending_on_soldout = true;
        lottery_options.delete_tickets_after_draw = true;

        account_id_type buyer1(3);
        account_id_type buyer2(6);
        generate_block();
        transfer(account_id_type(), buyer1, asset(1000000));
        transfer(account_id_type(), buyer2, asset(1000000));
        generate_block();

        {
            BOOST_TEST_MESSAGE("Send account_role_create_operation");

            account_role_create_operation op;
            op.owner = account_id_type();
            op.name = "Test Account Role";
            op.metadata = "{\"country\": \"earth\", \"race\": \"human\" }";

            int ops[] = {operation::tag<nft_lottery_token_purchase_operation>::value};
            op.allowed_operations.insert(ops, ops + 1);
            op.whitelisted_accounts.emplace(buyer1);
            op.valid_to = db.head_block_time() + 1000;

            trx.operations.push_back(op);
            PUSH_TX(db, trx, ~0);
            trx.operations.clear();
        }

        {
            BOOST_TEST_MESSAGE("Send nft_metadata_create_operation");
            nft_metadata_create_operation op;
            op.owner = account_id_type();
            op.symbol = "NFTLOTTERY" + std::to_string(test_nft_md_id.instance.value);
            op.base_uri = "http://nft.example.com";
            op.is_transferable = true;
            op.name = "NFTLOTTERY" + std::to_string(test_nft_md_id.instance.value);
            op.max_supply = 200;
            op.lottery_options = lottery_options;
            op.account_role = account_role_id_type(0);

            trx.operations.push_back(std::move(op));
            PUSH_TX(db, trx, ~0);
            trx.operations.clear();
        }
        generate_block();

        BOOST_TEST_MESSAGE("Check nft_metadata_create_operation results");

        auto test_nft_md_obj = test_nft_md_id(db);
        BOOST_CHECK(test_nft_md_obj.owner == account_id_type());
        BOOST_CHECK(test_nft_md_obj.name == "NFTLOTTERY" + std::to_string(test_nft_md_id.instance.value));
        BOOST_CHECK(test_nft_md_obj.symbol == "NFTLOTTERY" + std::to_string(test_nft_md_id.instance.value));
        BOOST_CHECK(test_nft_md_obj.base_uri == "http://nft.example.com");
        BOOST_CHECK(test_nft_md_obj.max_supply == share_type(200));
        BOOST_CHECK(test_nft_md_obj.is_lottery());
        BOOST_CHECK(test_nft_md_obj.get_token_current_supply(db) == share_type(0));
        BOOST_CHECK(test_nft_md_obj.get_lottery_jackpot(db) == asset());
        BOOST_CHECK(test_nft_md_obj.lottery_data->lottery_balance_id(db).sweeps_tickets_sold == share_type(0));
        BOOST_CHECK(test_nft_md_obj.account_role == account_role_id_type(0));

        BOOST_CHECK(test_nft_md_obj.lottery_data->lottery_options.is_active);

        {
            nft_lottery_token_purchase_operation tpo;
            tpo.fee = asset();
            tpo.buyer = buyer1;
            tpo.lottery_id = test_nft_md_obj.id;
            tpo.tickets_to_buy = 199;
            tpo.amount = asset(199 * 100);
            trx.operations.push_back(tpo);
            set_expiration(db, trx);
            PUSH_TX(db, trx, ~0);
            trx.operations.clear();
        }
        generate_block();
        test_nft_md_obj = test_nft_md_id(db);
        BOOST_CHECK(test_nft_md_obj.lottery_data->lottery_options.is_active);
        BOOST_CHECK(test_nft_md_obj.get_token_current_supply(db) == 199);
        {
            nft_lottery_token_purchase_operation tpo;
            tpo.fee = asset();
            tpo.buyer = buyer2;
            tpo.lottery_id = test_nft_md_obj.id;
            tpo.tickets_to_buy = 1;
            tpo.amount = asset(1 * 100);
            trx.operations.push_back(tpo);
            set_expiration(db, trx);
            BOOST_CHECK_THROW(PUSH_TX(db, trx, ~0), fc::exception);
            trx.operations.clear();
        }
        generate_block();
        test_nft_md_obj = test_nft_md_id(db);
        BOOST_CHECK(test_nft_md_obj.get_token_current_supply(db) == 199);
    }
    catch (fc::exception &e)
    {
        edump((e.to_detail_string()));
        throw;
    }
}

BOOST_AUTO_TEST_SUITE_END()
