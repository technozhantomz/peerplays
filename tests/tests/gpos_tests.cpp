/*
 * Copyright (c) 2018 oxarbitrage and contributors.
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
#include <graphene/chain/hardfork.hpp>
#include <graphene/chain/database.hpp>

#include <graphene/chain/balance_object.hpp>
#include <graphene/chain/vesting_balance_object.hpp>
#include <graphene/chain/witness_object.hpp>
#include <graphene/chain/committee_member_object.hpp>
#include <graphene/chain/worker_object.hpp>

#include "../common/database_fixture.hpp"

#include <graphene/app/database_api.hpp>

using namespace graphene::chain;
using namespace graphene::chain::test;

struct gpos_fixture: database_fixture
{
   const worker_object& create_worker( const account_id_type owner, const share_type daily_pay,
                                       const fc::microseconds& duration ) {
      worker_create_operation op;
      op.owner = owner;
      op.daily_pay = daily_pay;
      op.initializer = vesting_balance_worker_initializer(1);
      op.work_begin_date = db.head_block_time();
      op.work_end_date = op.work_begin_date + duration;
      trx.operations.push_back(op);
      set_expiration(db, trx);
      trx.validate();
      processed_transaction ptx = db.push_transaction(trx, ~0);
      trx.clear();
      return db.get<worker_object>(ptx.operation_results[0].get<object_id_type>());
   }
   const vesting_balance_object& create_vesting(const account_id_type owner, const asset amount,
                                                const vesting_balance_type type)
   {
      vesting_balance_create_operation op;
      op.creator = owner;
      op.owner = owner;
      op.amount = amount;
      op.balance_type = type;

      trx.operations.push_back(op);
      set_expiration(db, trx);
      processed_transaction ptx = PUSH_TX(db, trx, ~0);
      trx.clear();
      return db.get<vesting_balance_object>(ptx.operation_results[0].get<object_id_type>());
   }

   void withdraw_gpos_vesting(const vesting_balance_id_type v_bid, const account_id_type owner, const asset amount,
                              /*const vesting_balance_type type, */const fc::ecc::private_key& key)
   {
      vesting_balance_withdraw_operation op;
      op.vesting_balance = v_bid;
      op.owner = owner;
      op.amount = amount;
      //op.balance_type = type;
      
      trx.operations.push_back(op);
      set_expiration(db, trx);
      trx.validate();
      sign(trx, key);
      PUSH_TX(db, trx);
      trx.clear();
   }

   void update_payout_interval(std::string asset_name, fc::time_point start, uint32_t interval)
   {
      auto dividend_holder_asset_object = get_asset(asset_name);
      asset_update_dividend_operation op;
      op.issuer = dividend_holder_asset_object.issuer;
      op.asset_to_update = dividend_holder_asset_object.id;
      op.new_options.next_payout_time = start;
      op.new_options.payout_interval = interval;
      trx.operations.push_back(op);
      set_expiration(db, trx);
      PUSH_TX(db, trx, ~0);
      trx.operations.clear();
   }

   void update_gpos_global(uint32_t vesting_period, uint32_t vesting_subperiod, fc::time_point_sec period_start)
   {
      db.modify(db.get_global_properties(), [vesting_period, vesting_subperiod, period_start](global_property_object& p) {
         p.parameters.extensions.value.gpos_period = vesting_period;
         p.parameters.extensions.value.gpos_subperiod = vesting_subperiod;
         p.parameters.extensions.value.gpos_period_start =  period_start.sec_since_epoch();
         p.parameters.extensions.value.gpos_vesting_lockin_period = vesting_subperiod;
      });
      BOOST_CHECK_EQUAL(db.get_global_properties().parameters.gpos_period(), vesting_period);
      BOOST_CHECK_EQUAL(db.get_global_properties().parameters.gpos_subperiod(), vesting_subperiod);
      BOOST_CHECK_EQUAL(db.get_global_properties().parameters.gpos_period_start(), period_start.sec_since_epoch());
      BOOST_CHECK_EQUAL(db.get_global_properties().parameters.gpos_vesting_lockin_period(), vesting_subperiod);
   }

   void update_maintenance_interval(uint32_t new_interval)
   {
      db.modify(db.get_global_properties(), [new_interval](global_property_object& p) {
         p.parameters.maintenance_interval = new_interval;
      });
      BOOST_CHECK_EQUAL(db.get_global_properties().parameters.maintenance_interval, new_interval);
   }

   void vote_for(const account_id_type account_id, const vote_id_type vote_for, const fc::ecc::private_key& key)
   {
      account_update_operation op;
      op.account = account_id;
      op.new_options = account_id(db).options;
      op.new_options->votes.insert(vote_for);
      op.extensions.value.update_last_voting_time = true;
      trx.operations.push_back(op);
      set_expiration(db, trx);
      trx.validate();
      sign(trx, key);
      PUSH_TX(db, trx);
      trx.clear();
   }
   void fill_reserve_pool(const account_id_type account_id, asset amount)
   {
      asset_reserve_operation op;
      op.payer = account_id;
      op.amount_to_reserve = amount;
      trx.operations.push_back(op);
      trx.validate();
      set_expiration(db, trx);
      PUSH_TX( db, trx, ~0 );
      trx.clear();
   }

   void advance_x_maint(int periods)
   {
      for(int i=0; i<periods; i++)
         generate_blocks(db.get_dynamic_global_properties().next_maintenance_time);
   }

};

BOOST_FIXTURE_TEST_SUITE( gpos_tests, gpos_fixture )

BOOST_AUTO_TEST_CASE(gpos_vesting_type)
{
   ACTORS((alice)(bob));
   try
   {
      const auto& core = asset_id_type()(db);

      // send some asset to alice and bob
      transfer( committee_account, alice_id, core.amount( 1000 ) );
      transfer( committee_account, bob_id, core.amount( 1000 ) );

      generate_block();

      // gpos balance creation is not allowed before HF
      vesting_balance_create_operation op;
      op.creator = alice_id;
      op.owner = alice_id;
      op.amount = core.amount(100);
      op.balance_type = vesting_balance_type::gpos;

      trx.operations.push_back(op);
      set_expiration(db, trx);
      GRAPHENE_REQUIRE_THROW( PUSH_TX(db, trx, ~0), fc::exception );
      trx.clear();

      // pass hardfork
      generate_blocks( HARDFORK_GPOS_TIME );
      generate_block();

      // repeat operation
      trx.operations.push_back(op);
      set_expiration(db, trx);
      processed_transaction ptx = PUSH_TX(db, trx, ~0);
      trx.clear();

      generate_block();

      auto alice_vesting = db.get<vesting_balance_object>(ptx.operation_results[0].get<object_id_type>());

      // check created vesting amount and policy
      BOOST_CHECK_EQUAL(alice_vesting.balance.amount.value, 100);
      BOOST_CHECK_EQUAL(alice_vesting.policy.get<linear_vesting_policy>().vesting_duration_seconds,
                        db.get_global_properties().parameters.gpos_subperiod());
      BOOST_CHECK_EQUAL(alice_vesting.policy.get<linear_vesting_policy>().vesting_cliff_seconds,
                        db.get_global_properties().parameters.gpos_subperiod());

      // bob creates a gpos vesting with his custom policy
      {
         vesting_balance_create_operation op;
         op.creator = bob_id;
         op.owner = bob_id;
         op.amount = core.amount(200);
         op.balance_type = vesting_balance_type::gpos;
         op.policy = cdd_vesting_policy_initializer{ 60*60*24 };

         trx.operations.push_back(op);
         set_expiration(db, trx);
         ptx = PUSH_TX(db, trx, ~0);
         trx.clear();
      }
      auto bob_vesting = db.get<vesting_balance_object>(ptx.operation_results[0].get<object_id_type>());

      generate_block();

      // policy is not the one defined by the user but default
      BOOST_CHECK_EQUAL(bob_vesting.balance.amount.value, 200);
      BOOST_CHECK_EQUAL(bob_vesting.policy.get<linear_vesting_policy>().vesting_duration_seconds,
                        db.get_global_properties().parameters.gpos_subperiod());
      BOOST_CHECK_EQUAL(bob_vesting.policy.get<linear_vesting_policy>().vesting_cliff_seconds,
                        db.get_global_properties().parameters.gpos_subperiod());

   }
   catch (fc::exception& e)
   {
      edump((e.to_detail_string()));
      throw;
   }
}

BOOST_AUTO_TEST_CASE( dividends )
{
   ACTORS((alice)(bob));
   try
   {
      // move to 1 week before hardfork
      generate_blocks( HARDFORK_GPOS_TIME - fc::days(7) );
      generate_block();

      const auto& core = asset_id_type()(db);

      // all core coins are in the committee_account
      BOOST_CHECK_EQUAL(get_balance(committee_account(db), core), 1000000000000000);

      // transfer half of the total stake to alice so not all the dividends will go to the committee_account
      transfer( committee_account, alice_id, core.amount( 500000000000000 ) );
      generate_block();

      // send some to bob
      transfer( committee_account, bob_id, core.amount( 1000 ) );
      generate_block();

      // committee balance
      BOOST_CHECK_EQUAL(get_balance(committee_account(db), core), 499999999999000);

      // alice balance
      BOOST_CHECK_EQUAL(get_balance(alice_id(db), core), 500000000000000);

      // bob balance
      BOOST_CHECK_EQUAL(get_balance(bob_id(db), core), 1000);

      // get core asset object
      const auto& dividend_holder_asset_object = get_asset(GRAPHENE_SYMBOL);

      // by default core token pays dividends once per month
      const auto& dividend_data = dividend_holder_asset_object.dividend_data(db);
      BOOST_CHECK_EQUAL(*dividend_data.options.payout_interval, 2592000); //  30 days

      // update the payout interval for speed purposes of the test
      update_payout_interval(core.symbol, db.head_block_time() + fc::minutes(1), 60 * 60 * 24); // 1 day

      generate_block();

      BOOST_CHECK_EQUAL(*dividend_data.options.payout_interval, 86400); // 1 day now

      // get the dividend distribution account
      const account_object& dividend_distribution_account = dividend_data.dividend_distribution_account(db);

      // transfering some coins to distribution account.
      // simulating the blockchain haves some dividends to pay.
      transfer( committee_account, dividend_distribution_account.id, core.amount( 100 ) );
      generate_block();

      // committee balance
      BOOST_CHECK_EQUAL(get_balance(committee_account(db), core), 499999999998900 );

      // distribution account balance
      BOOST_CHECK_EQUAL(get_balance(dividend_distribution_account, core), 100);

      // get when is the next payout time as we need to advance there
      auto next_payout_time = dividend_data.options.next_payout_time;

      // advance to next payout
      generate_blocks(*next_payout_time);
      wdump((*next_payout_time));

      // advance to next maint after payout time arrives
      generate_blocks(db.get_dynamic_global_properties().next_maintenance_time);

      // check balances now, dividends are paid "normally"
      BOOST_CHECK_EQUAL(get_balance(committee_account(db), core), 499999999998949 );
      BOOST_CHECK_EQUAL(get_balance(alice_id(db), core), 500000000000050 );
      BOOST_CHECK_EQUAL(get_balance(bob_id(db), core), 1000 );
      BOOST_CHECK_EQUAL(get_balance(dividend_distribution_account, core), 1);

      // advance to hardfork
      generate_blocks( HARDFORK_GPOS_TIME );

      // advance to next maint
      generate_blocks(db.get_dynamic_global_properties().next_maintenance_time);

      // send 99 to the distribution account so it will have 100 PPY again to share
      transfer( committee_account, dividend_distribution_account.id, core.amount( 99 ) );
      generate_block();

      // get when is the next payout time as we need to advance there
      next_payout_time = dividend_data.options.next_payout_time;

      // advance to next payout
      generate_blocks(*next_payout_time);

      // advance to next maint
      generate_blocks(db.get_dynamic_global_properties().next_maintenance_time);

      // make sure no dividends were paid "normally"
      BOOST_CHECK_EQUAL(get_balance(committee_account(db), core), 499999999998850 );
      BOOST_CHECK_EQUAL(get_balance(alice_id(db), core), 500000000000050 );
      BOOST_CHECK_EQUAL(get_balance(bob_id(db), core), 1000 );
      BOOST_CHECK_EQUAL(get_balance(dividend_distribution_account, core), 100);

      // create vesting balance
      create_vesting(bob_id, core.amount(100), vesting_balance_type::gpos);

      // need to vote to get paid
      auto witness1 = witness_id_type(1)(db);
      vote_for(bob_id, witness1.vote_id, bob_private_key);

      generate_block();

      // check balances
      BOOST_CHECK_EQUAL(get_balance(bob_id(db), core), 900 );
      BOOST_CHECK_EQUAL(get_balance(dividend_distribution_account, core), 100);

      // advance to next payout
      generate_blocks(*next_payout_time);

      // advance to next maint
      generate_blocks(db.get_dynamic_global_properties().next_maintenance_time);

      // check balances, dividends paid to bob
      BOOST_CHECK_EQUAL(get_balance(bob_id(db), core), 1000 );
      BOOST_CHECK_EQUAL(get_balance(dividend_distribution_account, core), 0);
   }
   catch (fc::exception& e)
   {
      edump((e.to_detail_string()));
      throw;
   }
}

BOOST_AUTO_TEST_CASE( gpos_basic_dividend_distribution_to_core_asset )
{
   using namespace graphene;
   ACTORS((alice)(bob)(carol)(dave));
   try {
      const auto& core = asset_id_type()(db);
      BOOST_TEST_MESSAGE("Creating test asset");
      {
         asset_create_operation creator;
         creator.issuer = account_id_type();
         creator.fee = asset();
         creator.symbol = "TESTB";
         creator.common_options.max_supply = 100000000;
         creator.precision = 2;
         creator.common_options.market_fee_percent = GRAPHENE_MAX_MARKET_FEE_PERCENT/100; /*1%*/
         creator.common_options.issuer_permissions = UIA_ASSET_ISSUER_PERMISSION_MASK;
         creator.common_options.flags = charge_market_fee;
         creator.common_options.core_exchange_rate = price({asset(2),asset(1,asset_id_type(1))});
         trx.operations.push_back(std::move(creator));
         set_expiration(db, trx);
         PUSH_TX( db, trx, ~0 );
         trx.operations.clear();
      }
      
      // pass hardfork
      generate_blocks( HARDFORK_GPOS_TIME );
      generate_block();

      const auto& dividend_holder_asset_object = asset_id_type(0)(db);
      const auto& dividend_data = dividend_holder_asset_object.dividend_data(db);
      const account_object& dividend_distribution_account = dividend_data.dividend_distribution_account(db);
      const account_object& alice = get_account("alice");
      const account_object& bob = get_account("bob");
      const account_object& carol = get_account("carol");
      const account_object& dave = get_account("dave");
      const auto& test_asset_object = get_asset("TESTB");

      auto issue_asset_to_account = [&](const asset_object& asset_to_issue, const account_object& destination_account, int64_t amount_to_issue)
      {
         asset_issue_operation op;
         op.issuer = asset_to_issue.issuer;
         op.asset_to_issue = asset(amount_to_issue, asset_to_issue.id);
         op.issue_to_account = destination_account.id;
         trx.operations.push_back( op );
         set_expiration(db, trx);
         PUSH_TX( db, trx, ~0 );
         trx.operations.clear();
      };

      auto verify_pending_balance = [&](const account_object& holder_account_obj, const asset_object& payout_asset_obj, int64_t expected_balance) {
         int64_t pending_balance = get_dividend_pending_payout_balance(dividend_holder_asset_object.id,
                                                                       holder_account_obj.id,
                                                                       payout_asset_obj.id);
         BOOST_CHECK_EQUAL(pending_balance, expected_balance);
      };

      auto advance_to_next_payout_time = [&]() {
         // Advance to the next upcoming payout time
         BOOST_REQUIRE(dividend_data.options.next_payout_time);
         fc::time_point_sec next_payout_scheduled_time = *dividend_data.options.next_payout_time;
         idump((next_payout_scheduled_time));
         // generate blocks up to the next scheduled time
         generate_blocks(next_payout_scheduled_time);
         // if the scheduled time fell on a maintenance interval, then we should have paid out.
         // if not, we need to advance to the next maintenance interval to trigger the payout
         if (dividend_data.options.next_payout_time)
         {
            // we know there was a next_payout_time set when we entered this, so if
            // it has been cleared, we must have already processed payouts, no need to
            // further advance time.
            BOOST_REQUIRE(dividend_data.options.next_payout_time);
            if (*dividend_data.options.next_payout_time == next_payout_scheduled_time)
               generate_blocks(db.get_dynamic_global_properties().next_maintenance_time);
            generate_block();   // get the maintenance skip slots out of the way
         }
         idump((db.head_block_time()));
      };

      // the first test will be testing pending balances, so we need to hit a
      // maintenance interval that isn't the payout interval.  Payout is
      // every 3 days, maintenance interval is every 1 day.
      advance_to_next_payout_time();

      // Set up the first test, issue alice, bob, and carol, and dave each 1/4 of the total
      // supply of the core asset.
      // Then deposit 400 TEST in the distribution account, and see that they
      // each are credited 100 TEST.
      transfer( committee_account(db), alice, asset( 250000000000000 ) );
      transfer( committee_account(db), bob,   asset( 250000000000000 ) );
      transfer( committee_account(db), carol, asset( 250000000000000 ) );
      transfer( committee_account(db), dave,  asset( 250000000000000 ) );

      // create vesting balance
      // bob has not vested anything
      create_vesting(alice_id, core.amount(25000000), vesting_balance_type::gpos);
      create_vesting(carol_id, core.amount(25000000), vesting_balance_type::gpos);
      create_vesting(dave_id, core.amount(25000000), vesting_balance_type::gpos);

      // need to vote to get paid
      // carol doesn't participate in voting
      auto witness1 = witness_id_type(1)(db);
      vote_for(alice_id, witness1.vote_id, alice_private_key);
      vote_for(bob_id, witness1.vote_id, bob_private_key);
      vote_for(dave_id, witness1.vote_id, dave_private_key);

      // issuing 30000 TESTB to the dividend account
      // alice and dave should receive 10000 TESTB as they have gpos vesting and 
      // participated in voting
      // bob should not receive any TESTB as he doesn't have gpos vested
      // carol should not receive any TESTB as she doesn't participated in voting
      // remaining 10000 TESTB should be deposited in commitee_accoount. 
      BOOST_TEST_MESSAGE("Issuing 30000 TESTB to the dividend account");
      issue_asset_to_account(test_asset_object, dividend_distribution_account, 30000);

      generate_block();

      BOOST_TEST_MESSAGE( "Generating blocks until next maintenance interval" );
      generate_blocks(db.get_dynamic_global_properties().next_maintenance_time);
      generate_block();   // get the maintenance skip slots out of the way

      verify_pending_balance(alice, test_asset_object, 10000);
      verify_pending_balance(bob, test_asset_object, 0);
      verify_pending_balance(carol, test_asset_object, 0);
      verify_pending_balance(dave, test_asset_object, 10000);

      advance_to_next_payout_time();

      generate_blocks(db.get_dynamic_global_properties().next_maintenance_time);
      generate_block();   // get the maintenance skip slots out of the way

      auto verify_dividend_payout_operations = [&](const account_object& destination_account, const asset& expected_payout)
      {
         BOOST_TEST_MESSAGE("Verifying the virtual op was created");
         const account_transaction_history_index& hist_idx = db.get_index_type<account_transaction_history_index>();
         auto account_history_range = hist_idx.indices().get<by_seq>().equal_range(boost::make_tuple(destination_account.id));
         BOOST_REQUIRE(account_history_range.first != account_history_range.second);
         const operation_history_object& history_object = std::prev(account_history_range.second)->operation_id(db);
         const asset_dividend_distribution_operation& distribution_operation = history_object.op.get<asset_dividend_distribution_operation>();
         BOOST_CHECK(distribution_operation.account_id == destination_account.id);
         BOOST_CHECK(std::find(distribution_operation.amounts.begin(), distribution_operation.amounts.end(), expected_payout)
            != distribution_operation.amounts.end());
      };

      BOOST_TEST_MESSAGE("Verifying the payouts");
      BOOST_CHECK_EQUAL(get_balance(alice, test_asset_object), 10000);
      verify_dividend_payout_operations(alice, asset(10000, test_asset_object.id));
      verify_pending_balance(alice, test_asset_object, 0);

      BOOST_CHECK_EQUAL(get_balance(bob, test_asset_object), 0);
      verify_pending_balance(bob, test_asset_object, 0);

      BOOST_CHECK_EQUAL(get_balance(carol, test_asset_object), 0);
      verify_pending_balance(carol, test_asset_object, 0);

      BOOST_CHECK_EQUAL(get_balance(dave, test_asset_object), 10000);
      verify_dividend_payout_operations(dave, asset(10000, test_asset_object.id));
      verify_pending_balance(dave, test_asset_object, 0);

      BOOST_CHECK_EQUAL(get_balance(account_id_type(0)(db), test_asset_object), 10000);
   } catch(fc::exception& e) {
      edump((e.to_detail_string()));
      throw;
   }   
}

BOOST_AUTO_TEST_CASE( votes_on_gpos_activation )
{
   ACTORS((alice)(bob));
   try {
      const auto& core = asset_id_type()(db);

      // send some asset to alice and bob
      transfer( committee_account, alice_id, core.amount( 1000 ) );
      transfer( committee_account, bob_id, core.amount( 1000 ) );
      generate_block();

      // update default gpos
      auto now = db.head_block_time();
      // 5184000 = 60x60x24x6 = 6 days
      // 864000 = 60x60x24x1 = 1 days
      update_gpos_global(518400, 86400, HARDFORK_GPOS_TIME);

      BOOST_CHECK_EQUAL(db.get_global_properties().parameters.gpos_period(), 518400);
      BOOST_CHECK_EQUAL(db.get_global_properties().parameters.gpos_subperiod(), 86400);
      BOOST_CHECK_EQUAL(db.get_global_properties().parameters.gpos_period_start(), HARDFORK_GPOS_TIME.sec_since_epoch());
      // no votes for witness 1
      auto witness1 = witness_id_type(1)(db);
      BOOST_CHECK_EQUAL(witness1.total_votes, 0);

      // no votes for witness 2
      auto witness2 = witness_id_type(2)(db);
      BOOST_CHECK_EQUAL(witness2.total_votes, 0);

      // vote for witness1 and witness2 - this before GPOS period starts
      vote_for(alice_id, witness1.vote_id, alice_private_key);
      vote_for(bob_id, witness2.vote_id, bob_private_key);

      // go to maint
      generate_blocks(db.get_dynamic_global_properties().next_maintenance_time);

      // vote is the same as amount in the first subperiod since voting
      witness1 = witness_id_type(1)(db);
      witness2 = witness_id_type(2)(db);
      BOOST_CHECK_EQUAL(witness1.total_votes, 1000);
      BOOST_CHECK_EQUAL(witness2.total_votes, 1000);
      
      update_maintenance_interval(3600);  //update maintenance interval to 1hr to evaluate sub-periods
      BOOST_CHECK_EQUAL(db.get_global_properties().parameters.maintenance_interval, 3600);

      // move to hardfork
      generate_blocks( HARDFORK_GPOS_TIME );
      generate_block();

      witness1 = witness_id_type(1)(db);
      witness2 = witness_id_type(2)(db);
      BOOST_CHECK_EQUAL(witness1.total_votes, 1000);
      BOOST_CHECK_EQUAL(witness2.total_votes, 1000);

      // add some vesting to alice and don't add anything for Bob
      create_vesting(alice_id, core.amount(99), vesting_balance_type::gpos);
      generate_block();
      vote_for(alice_id, witness1.vote_id, alice_private_key);
      generate_block();

      advance_x_maint(1);
      witness1 = witness_id_type(1)(db);
      witness2 = witness_id_type(2)(db);
      //System needs to consider votes based on both regular balance + GPOS balance for 1/2 sub-period on GPOS activation
      BOOST_CHECK_EQUAL(witness1.total_votes, 1000);
      BOOST_CHECK_EQUAL(witness2.total_votes, 1000);

      advance_x_maint(6);
      witness1 = witness_id_type(1)(db);
      witness2 = witness_id_type(2)(db);
      BOOST_CHECK_EQUAL(witness1.total_votes, 1000);
      BOOST_CHECK_EQUAL(witness2.total_votes, 1000);

      advance_x_maint(5);
      generate_block();
      witness1 = witness_id_type(1)(db);
      witness2 = witness_id_type(2)(db);
      //Since Alice has votes, votes should be based on GPOS balance i.e 99
      //Since Bob not voted after GPOS activation, witness2 votes should be 0 after crossing 1/2 sub-period(12 maintanence intervals in this case)
      BOOST_CHECK_EQUAL(witness1.total_votes, 99);
      BOOST_CHECK_EQUAL(witness2.total_votes, 0);

   }
   catch (fc::exception &e) {
      edump((e.to_detail_string()));
      throw;
   }
}

BOOST_AUTO_TEST_CASE( voting )
{
   ACTORS((alice)(bob));
   try {
      // move to hardfork
      generate_blocks( HARDFORK_GPOS_TIME );
      generate_block();

      auto now = HARDFORK_GPOS_TIME;
      const auto& core = asset_id_type()(db);

      // send some asset to alice and bob
      transfer( committee_account, alice_id, core.amount( 1000 ) );
      transfer( committee_account, bob_id, core.amount( 1000 ) );
      generate_block();

      // default maintenance_interval is 1 day
      BOOST_CHECK_EQUAL(db.get_global_properties().parameters.maintenance_interval, 86400);

      // add some vesting to alice and bob
      create_vesting(alice_id, core.amount(100), vesting_balance_type::gpos);
      create_vesting(bob_id, core.amount(100), vesting_balance_type::gpos);
      generate_block();

      // default gpos values
      BOOST_CHECK_EQUAL(db.get_global_properties().parameters.gpos_period(), 15552000);
      BOOST_CHECK_EQUAL(db.get_global_properties().parameters.gpos_subperiod(), 2592000);
      BOOST_CHECK_EQUAL(db.get_global_properties().parameters.gpos_period_start(), HARDFORK_GPOS_TIME.sec_since_epoch());

      // update default gpos for test speed
      // 5184000 = 60x60x24x60 = 60 days
      // 864000 = 60x60x24x10 = 10 days
      update_gpos_global(5184000, 864000, now);

      BOOST_CHECK_EQUAL(db.get_global_properties().parameters.gpos_period(), 5184000);
      BOOST_CHECK_EQUAL(db.get_global_properties().parameters.gpos_subperiod(), 864000);
      BOOST_CHECK_EQUAL(db.get_global_properties().parameters.gpos_period_start(), now.sec_since_epoch());
      // end global changes

      generate_block();

      // no votes for witness 1
      auto witness1 = witness_id_type(1)(db);
      BOOST_CHECK_EQUAL(witness1.total_votes, 0);

      // no votes for witness 2
      auto witness2 = witness_id_type(2)(db);
      BOOST_CHECK_EQUAL(witness2.total_votes, 0);

      // vote for witness1 and witness2 - sub-period 1
      vote_for(alice_id, witness1.vote_id, alice_private_key);
      vote_for(bob_id, witness2.vote_id, bob_private_key);

      // go to maint
      generate_blocks(db.get_dynamic_global_properties().next_maintenance_time);

      // need to consider both gpos and regular balance for first 1/2 sub period
      witness1 = witness_id_type(1)(db);
      witness2 = witness_id_type(2)(db);
      BOOST_CHECK_EQUAL(witness1.total_votes, 1000);
      BOOST_CHECK_EQUAL(witness2.total_votes, 1000);

      advance_x_maint(6);

      witness1 = witness_id_type(1)(db);
      witness2 = witness_id_type(2)(db);
      BOOST_CHECK_EQUAL(witness1.total_votes, 100);
      BOOST_CHECK_EQUAL(witness2.total_votes, 100);

      advance_x_maint(4);

      //Bob votes for witness2 - sub-period 2
      vote_for(bob_id, witness2.vote_id, bob_private_key);
      // go to maint
      generate_blocks(db.get_dynamic_global_properties().next_maintenance_time);
      // vote decay as time pass
      witness1 = witness_id_type(1)(db);
      witness2 = witness_id_type(2)(db);

      BOOST_CHECK_EQUAL(witness1.total_votes, 83);
      BOOST_CHECK_EQUAL(witness2.total_votes, 100);
      
      advance_x_maint(10);
      //Bob votes for witness2 - sub-period 3
      vote_for(bob_id, witness2.vote_id, bob_private_key);
      generate_blocks(db.get_dynamic_global_properties().next_maintenance_time);
      // decay more
      witness1 = witness_id_type(1)(db);
      witness2 = witness_id_type(2)(db);
      BOOST_CHECK_EQUAL(witness1.total_votes, 66);
      BOOST_CHECK_EQUAL(witness2.total_votes, 100);

      advance_x_maint(10);
      
      // Bob votes for witness2 - sub-period 4
      vote_for(bob_id, witness2.vote_id, bob_private_key);
      generate_blocks(db.get_dynamic_global_properties().next_maintenance_time);
      // decay more
      witness1 = witness_id_type(1)(db);
      witness2 = witness_id_type(2)(db);
      BOOST_CHECK_EQUAL(witness1.total_votes, 50);
      BOOST_CHECK_EQUAL(witness2.total_votes, 100);

      advance_x_maint(10);
      
      // Bob votes for witness2 - sub-period 5
      vote_for(bob_id, witness2.vote_id, bob_private_key);
      generate_blocks(db.get_dynamic_global_properties().next_maintenance_time);
      // decay more
      witness1 = witness_id_type(1)(db);
      witness2 = witness_id_type(2)(db);

      BOOST_CHECK_EQUAL(witness1.total_votes, 33);
      BOOST_CHECK_EQUAL(witness2.total_votes, 100);

      advance_x_maint(10);
      
      // Bob votes for witness2 - sub-period 6
      vote_for(bob_id, witness2.vote_id, bob_private_key);
      generate_blocks(db.get_dynamic_global_properties().next_maintenance_time);
      // decay more
      witness1 = witness_id_type(1)(db);
      witness2 = witness_id_type(2)(db);
      BOOST_CHECK_EQUAL(witness1.total_votes, 16);
      BOOST_CHECK_EQUAL(witness2.total_votes, 100);

      // we are still in gpos period 1
      BOOST_CHECK_EQUAL(db.get_global_properties().parameters.gpos_period_start(), now.sec_since_epoch());

      advance_x_maint(5);
      // a new GPOS period is in but vote from user is before the start. Whoever votes in 6th sub-period, votes will carry
      now = db.head_block_time();
      BOOST_CHECK_EQUAL(db.get_global_properties().parameters.gpos_period_start(), HARDFORK_GPOS_TIME.sec_since_epoch() + db.get_global_properties().parameters.gpos_period());

      generate_block();

      // we are in the second GPOS period, at subperiod 1,
      witness1 = witness_id_type(1)(db);
      witness2 = witness_id_type(2)(db);
      BOOST_CHECK_EQUAL(witness1.total_votes, 0);
      //It's critical here, since bob votes in 6th sub-period of last vesting period, witness2 should retain his votes
      BOOST_CHECK_EQUAL(witness2.total_votes, 100);   


      // lets vote here from alice to generate votes for witness 1
      //vote from bob to reatin VF 1
      vote_for(alice_id, witness1.vote_id, alice_private_key);
      vote_for(bob_id, witness2.vote_id, bob_private_key);
      generate_block();

      // go to maint
      generate_blocks(db.get_dynamic_global_properties().next_maintenance_time);

      witness1 = witness_id_type(1)(db);
      witness2 = witness_id_type(2)(db);

      BOOST_CHECK_EQUAL(witness1.total_votes, 100);
      BOOST_CHECK_EQUAL(witness2.total_votes, 100);

      advance_x_maint(10);

      witness1 = witness_id_type(1)(db);
      witness2 = witness_id_type(2)(db);

      BOOST_CHECK_EQUAL(witness1.total_votes, 83);
      BOOST_CHECK_EQUAL(witness2.total_votes, 83);

      vote_for(bob_id, witness2.vote_id, bob_private_key);
      generate_block();

      advance_x_maint(10);

      witness1 = witness_id_type(1)(db);
      witness2 = witness_id_type(2)(db);

      BOOST_CHECK_EQUAL(witness1.total_votes, 66);
      BOOST_CHECK_EQUAL(witness2.total_votes, 83);

      // alice votes again, now for witness 2, her vote worth 100 now
      vote_for(alice_id, witness2.vote_id, alice_private_key);
      generate_blocks(db.get_dynamic_global_properties().next_maintenance_time);

      witness1 = witness_id_type(1)(db);
      witness2 = witness_id_type(2)(db);

      BOOST_CHECK_EQUAL(witness1.total_votes, 100);
      BOOST_CHECK_EQUAL(witness2.total_votes, 183);

   }
   catch (fc::exception &e) {
      edump((e.to_detail_string()));
      throw;
   }
}

BOOST_AUTO_TEST_CASE( rolling_period_start )
{
   // period start rolls automatically after HF
   try {
      // update default gpos global parameters to make this thing faster
      update_gpos_global(518400, 86400, HARDFORK_GPOS_TIME);
      generate_blocks(HARDFORK_GPOS_TIME);
      update_maintenance_interval(3600);  //update maintenance interval to 1hr to evaluate sub-periods
      BOOST_CHECK_EQUAL(db.get_global_properties().parameters.maintenance_interval, 3600);

      auto vesting_period_1 = db.get_global_properties().parameters.gpos_period_start();
            
      auto now = db.head_block_time();
      // moving outside period:
      while( db.head_block_time() <= now + fc::days(6) )
      {
         generate_block();
      }
      generate_block();
      auto vesting_period_2 = db.get_global_properties().parameters.gpos_period_start();
     
     //difference between start of two consecutive vesting periods should be 6 days 
      BOOST_CHECK_EQUAL(vesting_period_1 + 518400, vesting_period_2); 
   }
   catch (fc::exception &e) {
      edump((e.to_detail_string()));
      throw;
   }
}

BOOST_AUTO_TEST_CASE( worker_dividends_voting )
{
   try {
      // advance to HF
      fc::time_point_sec GPOS_HARDFORK_TIME =
              fc::time_point_sec(1581976800); // Use mainnet GPOS hardfork time

      generate_blocks(GPOS_HARDFORK_TIME);
      generate_block();

      // update default gpos global parameters to 4 days
      auto now = db.head_block_time();
      update_gpos_global(345600, 86400, now);

      generate_block();
      set_expiration(db, trx);
      const auto& core = asset_id_type()(db);

      // get core asset object
      const auto& dividend_holder_asset_object = get_asset(GRAPHENE_SYMBOL);

      // by default core token pays dividends once per month
      const auto& dividend_data = dividend_holder_asset_object.dividend_data(db);
      BOOST_CHECK_EQUAL(*dividend_data.options.payout_interval, 2592000); //  30 days

      // update the payout interval to 1 day for speed purposes of the test
      update_payout_interval(core.symbol, db.head_block_time() + fc::minutes(1), 60 * 60 * 24); // 1 day

      generate_block();

      // get the dividend distribution account
      const account_object& dividend_distribution_account = dividend_data.dividend_distribution_account(db);

      // transfering some coins to distribution account.
      transfer( committee_account, dividend_distribution_account.id, core.amount( 100 ) );
      generate_block();

      ACTORS((nathan)(voter1)(voter2)(voter3));

      transfer( committee_account, nathan_id, core.amount( 1000 ) );
      transfer( committee_account, voter1_id, core.amount( 1000 ) );
      transfer( committee_account, voter2_id, core.amount( 1000 ) );

      generate_block();

      upgrade_to_lifetime_member(nathan_id);

      auto worker = create_worker(nathan_id, 10, fc::days(6));

      // add some vesting to voter1
      create_vesting(voter1_id, core.amount(100), vesting_balance_type::gpos);

      // add some vesting to voter2
      create_vesting(voter2_id, core.amount(100), vesting_balance_type::gpos);

      generate_block();

      // vote for worker
      vote_for(voter1_id, worker.vote_for, voter1_private_key);

      // first maint pass, coefficient will be 1
      generate_blocks(GPOS_HARDFORK_TIME + fc::hours(12)); //forward 1/2 sub-period so that it consider only gpos votes
      worker = worker_id_type()(db);
      BOOST_CHECK_EQUAL(worker.total_votes_for, 100);

      // here dividends are paid to voter1 and voter2
      // voter1 get paid full dividend share as coefficent is at 1 here
      BOOST_CHECK_EQUAL(get_balance(voter1_id(db), core), 950);

      // voter2 didnt voted so he dont get paid
      BOOST_CHECK_EQUAL(get_balance(voter2_id(db), core), 900);

      // send some asset to the reserve pool so the worker can get paid
      fill_reserve_pool(account_id_type(), asset(GRAPHENE_MAX_SHARE_SUPPLY/2));

      BOOST_CHECK_EQUAL(worker_id_type()(db).worker.get<vesting_balance_worker_type>().balance(db).balance.amount.value, 0);
      BOOST_CHECK_EQUAL(worker.worker.get<vesting_balance_worker_type>().balance(db).balance.amount.value, 0);

      generate_blocks(db.get_dynamic_global_properties().next_maintenance_time);

      // worker is getting paid
      BOOST_CHECK_EQUAL(worker_id_type()(db).worker.get<vesting_balance_worker_type>().balance(db).balance.amount.value, 5);
      BOOST_CHECK_EQUAL(worker.worker.get<vesting_balance_worker_type>().balance(db).balance.amount.value, 5);

      // second maint pass, coefficient will be 0.75
      worker = worker_id_type()(db);
      BOOST_CHECK_EQUAL(worker.total_votes_for, 75);

      // more decay
      generate_blocks(db.get_dynamic_global_properties().next_maintenance_time);

      worker = worker_id_type()(db);
      BOOST_CHECK_EQUAL(worker.total_votes_for, 50);

      transfer( committee_account, dividend_distribution_account.id, core.amount( 100 ) );
      generate_block();

      BOOST_CHECK_EQUAL(get_balance(committee_account(db), core), 499999999996850);

      // more decay
      generate_blocks(db.get_dynamic_global_properties().next_maintenance_time);

      worker = worker_id_type()(db);
      BOOST_CHECK_EQUAL(worker.total_votes_for, 25);

      // here voter1 get paid again but less money by vesting coefficient
      BOOST_CHECK_EQUAL(get_balance(voter1_id(db), core), 962);
      BOOST_CHECK_EQUAL(get_balance(voter2_id(db), core), 900);

      // remaining dividends not paid by coeffcient are sent to committee account
      BOOST_CHECK_EQUAL(get_balance(committee_account(db), core), 499999999996938);
   }
   catch (fc::exception &e) {
      edump((e.to_detail_string()));
      throw;
   }
}

BOOST_AUTO_TEST_CASE( account_multiple_vesting )
{
   try {
      // advance to HF
      fc::time_point_sec GPOS_HARDFORK_TIME =
              fc::time_point_sec(1581976800); // Use mainnet GPOS hardfork time

      generate_blocks(GPOS_HARDFORK_TIME);
      generate_block();
      set_expiration(db, trx);

      // update default gpos global parameters to 4 days
      auto now = db.head_block_time();
      update_gpos_global(345600, 86400, now);

      ACTORS((sam)(patty));

      const auto& core = asset_id_type()(db);

      transfer( committee_account, sam_id, core.amount( 300 ) );
      transfer( committee_account, patty_id, core.amount( 100 ) );

      // add some vesting to sam
      create_vesting(sam_id, core.amount(100), vesting_balance_type::gpos);

      // have another balance with 200 more
      create_vesting(sam_id, core.amount(200), vesting_balance_type::gpos);

      // patty also have vesting balance
      create_vesting(patty_id, core.amount(100), vesting_balance_type::gpos);

      // get core asset object
      const auto& dividend_holder_asset_object = get_asset(GRAPHENE_SYMBOL);
      const auto& dividend_data = dividend_holder_asset_object.dividend_data(db);

      // update the payout interval
      update_payout_interval(core.symbol, db.head_block_time() + fc::minutes(1), 60 * 60 * 24); // 1 day

      // get the dividend distribution account
      const account_object& dividend_distribution_account = dividend_data.dividend_distribution_account(db);

      // transfering some coins to distribution account.
      transfer( committee_account, dividend_distribution_account.id, core.amount( 100 ) );
      generate_block();

      // vote for a votable object
      auto witness1 = witness_id_type(1)(db);
      vote_for(sam_id, witness1.vote_id, sam_private_key);
      vote_for(patty_id, witness1.vote_id, patty_private_key);

      generate_blocks(GPOS_HARDFORK_TIME + fc::hours(12)); //forward 1/2 sub-period so that it consider only gpos votes

      // amount in vested balanced will sum up as voting power
      witness1 = witness_id_type(1)(db);
      BOOST_CHECK_EQUAL(witness1.total_votes, 400);

      // sam get paid dividends
      BOOST_CHECK_EQUAL(get_balance(sam_id(db), core), 75);

      // patty also
      BOOST_CHECK_EQUAL(get_balance(patty_id(db), core), 25);

      // total vote not decaying
      generate_blocks(db.get_dynamic_global_properties().next_maintenance_time);
      generate_block();

      witness1 = witness_id_type(1)(db);

      BOOST_CHECK_EQUAL(witness1.total_votes, 300);
   }
   catch (fc::exception &e) {
      edump((e.to_detail_string()));
      throw;
   }
}

BOOST_AUTO_TEST_CASE( Withdraw_gpos_vesting_balance )
{
   try {
      // advance to HF
      generate_blocks(HARDFORK_GPOS_TIME);
      generate_block();
      set_expiration(db, trx);

      // update default gpos global parameters to 4 days
      auto now = db.head_block_time();
      update_gpos_global(345600, 86400, now);

      ACTORS((alice)(bob));

      graphene::app::database_api db_api1(db);
      const auto& core = asset_id_type()(db);


      transfer( committee_account, alice_id, core.amount( 500 ) );
      transfer( committee_account, bob_id, core.amount( 99 ) );

      // add some vesting to Alice, Bob
      vesting_balance_object vbo1, vbo2;
      vbo1 = create_vesting(alice_id, core.amount(150), vesting_balance_type::gpos);
      vbo2 = create_vesting(bob_id, core.amount(99), vesting_balance_type::gpos);
      generate_blocks(db.get_dynamic_global_properties().next_maintenance_time);
     
      generate_block();

      generate_blocks(db.get_dynamic_global_properties().next_maintenance_time);
      generate_blocks(db.get_global_properties().parameters.gpos_vesting_lockin_period());
      BOOST_CHECK_EQUAL(get_balance(alice_id(db), core), 350);
      withdraw_gpos_vesting(vbo1.id, alice_id, core.amount(50), /*vesting_balance_type::gpos, */alice_private_key);
      withdraw_gpos_vesting(vbo2.id, bob_id, core.amount(99), /*vesting_balance_type::gpos, */bob_private_key);
      generate_block();
      // verify charles balance
      BOOST_CHECK_EQUAL(get_balance(alice_id(db), core), 400);
      BOOST_CHECK_EQUAL(get_balance(bob_id(db), core), 99);

      // Add more 50 and 73 vesting objects and withdraw 90 from 
      // total vesting balance of user
      vbo1 = create_vesting(alice_id, core.amount(50), vesting_balance_type::gpos);
      vbo2 = create_vesting(alice_id, core.amount(73), vesting_balance_type::gpos);
      generate_blocks(db.get_dynamic_global_properties().next_maintenance_time);
     
      generate_block();

      vector<vesting_balance_object> vbos = db_api1.get_vesting_balances("alice");
      asset total_vesting;
      for (const vesting_balance_object& vbo : vbos)
      {
         if (vbo.balance_type == vesting_balance_type::gpos && vbo.balance.asset_id == asset_id_type())
            total_vesting += vbo.balance;
      }
      // total vesting balance of alice
      BOOST_CHECK_EQUAL(total_vesting.amount.value, core.amount(223).amount.value);

      generate_blocks(db.get_dynamic_global_properties().next_maintenance_time);
      generate_blocks(db.get_global_properties().parameters.gpos_vesting_lockin_period());
      BOOST_CHECK_EQUAL(get_balance(alice_id(db), core), 277);
      withdraw_gpos_vesting(vbo1.id, alice_id, core.amount(90), /*vesting_balance_type::gpos,*/ alice_private_key);
      generate_block();
      // verify alice balance
      BOOST_CHECK_EQUAL(get_balance(alice_id(db), core), 367);

      // verify remaining vesting balance
      vbos = db_api1.get_vesting_balances("alice");
      asset remaining_vesting;
      for (const vesting_balance_object& vbo : vbos)
      {
         if (vbo.balance_type == vesting_balance_type::gpos && vbo.balance.asset_id == asset_id_type())
            remaining_vesting += vbo.balance;
      }
      // remaining vesting balance of alice
      BOOST_CHECK_EQUAL(remaining_vesting.amount.value, core.amount(133).amount.value);
   }
   catch (fc::exception &e) {
      edump((e.to_detail_string()));
      throw;
   }
}

/*
BOOST_AUTO_TEST_CASE( competing_proposals )
{
   try {
      // advance to HF
      generate_blocks(HARDFORK_GPOS_TIME);
      generate_block();
      set_expiration(db, trx);

      ACTORS((voter1)(voter2)(worker1)(worker2));

      const auto& core = asset_id_type()(db);

      transfer( committee_account, worker1_id, core.amount( 1000 ) );
      transfer( committee_account, worker2_id, core.amount( 1000 ) );
      transfer( committee_account, voter1_id, core.amount( 1000 ) );
      transfer( committee_account, voter2_id, core.amount( 1000 ) );

      create_vesting(voter1_id, core.amount(200), vesting_balance_type::gpos);
      create_vesting(voter2_id, core.amount(300), vesting_balance_type::gpos);

      generate_block();

      auto now = db.head_block_time();
      update_gpos_global(518400, 86400, now);

      update_payout_interval(core.symbol, fc::time_point::now() + fc::minutes(1), 60 * 60 * 24); // 1 day

      upgrade_to_lifetime_member(worker1_id);
      upgrade_to_lifetime_member(worker2_id);

      // create 2 competing proposals asking a lot of token
      // todo: maybe a refund worker here so we can test with smaller numbers
      auto w1 = create_worker(worker1_id, 100000000000, fc::days(10));
      auto w1_id_instance = w1.id.instance();
      auto w2 = create_worker(worker2_id, 100000000000, fc::days(10));
      auto w2_id_instance = w2.id.instance();

      fill_reserve_pool(account_id_type(), asset(GRAPHENE_MAX_SHARE_SUPPLY/2));

      // vote for the 2 workers
      vote_for(voter1_id, w1.vote_for, voter1_private_key);
      vote_for(voter2_id, w2.vote_for, voter2_private_key);

      generate_blocks(db.get_dynamic_global_properties().next_maintenance_time);
      generate_block();

      w1 = worker_id_type(w1_id_instance)(db);
      w2 = worker_id_type(w2_id_instance)(db);

      generate_blocks(db.get_dynamic_global_properties().next_maintenance_time);
      generate_block();

      // only w2 is getting paid as it haves more votes and money is only enough for 1
      BOOST_CHECK_EQUAL(w1.worker.get<vesting_balance_worker_type>().balance(db).balance.amount.value, 0);
      BOOST_CHECK_EQUAL(w2.worker.get<vesting_balance_worker_type>().balance(db).balance.amount.value, 100000000000);

      generate_blocks(db.get_dynamic_global_properties().next_maintenance_time);
      generate_block();

      BOOST_CHECK_EQUAL(w1.worker.get<vesting_balance_worker_type>().balance(db).balance.amount.value, 0);
      BOOST_CHECK_EQUAL(w2.worker.get<vesting_balance_worker_type>().balance(db).balance.amount.value, 150000000000);

      generate_blocks(db.get_dynamic_global_properties().next_maintenance_time);
      generate_block();

      w1 = worker_id_type(w1_id_instance)(db);
      w2 = worker_id_type(w2_id_instance)(db);

      // as votes decay w1 is still getting paid as it always have more votes than w1
      BOOST_CHECK_EQUAL(w1.total_votes_for, 100);
      BOOST_CHECK_EQUAL(w2.total_votes_for, 150);

      BOOST_CHECK_EQUAL(w1.worker.get<vesting_balance_worker_type>().balance(db).balance.amount.value, 0);
      BOOST_CHECK_EQUAL(w2.worker.get<vesting_balance_worker_type>().balance(db).balance.amount.value, 200000000000);

      generate_blocks(db.get_dynamic_global_properties().next_maintenance_time);
      generate_block();

      w1 = worker_id_type(w1_id_instance)(db);
      w2 = worker_id_type(w2_id_instance)(db);

      BOOST_CHECK_EQUAL(w1.total_votes_for, 66);
      BOOST_CHECK_EQUAL(w2.total_votes_for, 100);

      // worker is sil getting paid as days pass
      BOOST_CHECK_EQUAL(w1.worker.get<vesting_balance_worker_type>().balance(db).balance.amount.value, 0);
      BOOST_CHECK_EQUAL(w2.worker.get<vesting_balance_worker_type>().balance(db).balance.amount.value, 250000000000);

      generate_blocks(db.get_dynamic_global_properties().next_maintenance_time);
      generate_block();

      w1 = worker_id_type(w1_id_instance)(db);
      w2 = worker_id_type(w2_id_instance)(db);

      BOOST_CHECK_EQUAL(w1.total_votes_for, 33);
      BOOST_CHECK_EQUAL(w2.total_votes_for, 50);

      BOOST_CHECK_EQUAL(w1.worker.get<vesting_balance_worker_type>().balance(db).balance.amount.value, 0);
      BOOST_CHECK_EQUAL(w2.worker.get<vesting_balance_worker_type>().balance(db).balance.amount.value, 300000000000);

      generate_blocks(db.get_dynamic_global_properties().next_maintenance_time);
      generate_block();

      w1 = worker_id_type(w1_id_instance)(db);
      w2 = worker_id_type(w2_id_instance)(db);

      // worker2 will not get paid anymore as it haves 0 votes
      BOOST_CHECK_EQUAL(w1.total_votes_for, 0);
      BOOST_CHECK_EQUAL(w2.total_votes_for, 0);

      BOOST_CHECK_EQUAL(w1.worker.get<vesting_balance_worker_type>().balance(db).balance.amount.value, 0);
      BOOST_CHECK_EQUAL(w2.worker.get<vesting_balance_worker_type>().balance(db).balance.amount.value, 300000000000);
   }
   catch (fc::exception &e) {
      edump((e.to_detail_string()));
      throw;
   }
}
*/
BOOST_AUTO_TEST_CASE( proxy_voting )
{
   ACTORS((alice)(bob));
   try {
      // move to hardfork
      generate_blocks( HARDFORK_GPOS_TIME );
      generate_block();

      // database api
      graphene::app::database_api db_api(db);

      const auto& core = asset_id_type()(db);

      // send some asset to alice and bob
      transfer( committee_account, alice_id, core.amount( 1000 ) );
      transfer( committee_account, bob_id, core.amount( 1000 ) );
      generate_block();

      // add some vesting to alice and bob
      create_vesting(alice_id, core.amount(100), vesting_balance_type::gpos);
      generate_block();

      // total balance is 100 rest of data at 0
      auto gpos_info = db_api.get_gpos_info(alice_id);
      BOOST_CHECK_EQUAL(gpos_info.vesting_factor, 0);
      BOOST_CHECK_EQUAL(gpos_info.award.amount.value, 0);
      BOOST_CHECK_EQUAL(gpos_info.total_amount.value, 100);

      create_vesting(bob_id, core.amount(100), vesting_balance_type::gpos);
      generate_block();

      gpos_info = db_api.get_gpos_info(bob_id);
      BOOST_CHECK_EQUAL(gpos_info.vesting_factor, 0);
      BOOST_CHECK_EQUAL(gpos_info.award.amount.value, 0);
      BOOST_CHECK_EQUAL(gpos_info.total_amount.value, 200);

      auto now = db.head_block_time();
      update_gpos_global(518400, 86400, now);

      BOOST_CHECK_EQUAL(db.get_global_properties().parameters.gpos_period(), 518400);
      BOOST_CHECK_EQUAL(db.get_global_properties().parameters.gpos_subperiod(), 86400);
      BOOST_CHECK_EQUAL(db.get_global_properties().parameters.gpos_period_start(), now.sec_since_epoch());

      // alice assign bob as voting account
      graphene::chain::account_update_operation op;
      op.account = alice_id;
      op.new_options = alice_id(db).options;
      op.new_options->voting_account = bob_id;
      trx.operations.push_back(op);
      set_expiration(db, trx);
      trx.validate();
      sign(trx, alice_private_key);
      PUSH_TX( db, trx, ~0 );
      trx.clear();

      generate_block();

      // vote for witness1
      auto witness1 = witness_id_type(1)(db);
      vote_for(bob_id, witness1.vote_id, bob_private_key);
      
      generate_blocks(db.get_dynamic_global_properties().next_maintenance_time);
      
      // check vesting factor of current subperiod
      BOOST_CHECK_EQUAL(db_api.get_gpos_info(alice_id).vesting_factor, 1);
      BOOST_CHECK_EQUAL(db_api.get_gpos_info(bob_id).vesting_factor, 1);
      
      generate_blocks(db.get_dynamic_global_properties().next_maintenance_time);
      generate_block();

      // GPOS 2nd subperiod started.
      // vesting factor decay
      BOOST_CHECK_EQUAL(db_api.get_gpos_info(alice_id).vesting_factor, 0.83333333333333337);
      BOOST_CHECK_EQUAL(db_api.get_gpos_info(bob_id).vesting_factor, 0.83333333333333337);
      
      generate_blocks(db.get_dynamic_global_properties().next_maintenance_time);
      generate_block();

      // GPOS 3rd subperiod started
      // vesting factor decay
      BOOST_CHECK_EQUAL(db_api.get_gpos_info(alice_id).vesting_factor, 0.66666666666666663);
      BOOST_CHECK_EQUAL(db_api.get_gpos_info(bob_id).vesting_factor, 0.66666666666666663);

      // vote for witness2
      auto witness2 = witness_id_type(2)(db);
      vote_for(bob_id, witness2.vote_id, bob_private_key);

      // vesting factor should be 1 for both alice and bob for the current subperiod
      BOOST_CHECK_EQUAL(db_api.get_gpos_info(alice_id).vesting_factor, 1);
      BOOST_CHECK_EQUAL(db_api.get_gpos_info(bob_id).vesting_factor, 1);

      generate_blocks(db.get_dynamic_global_properties().next_maintenance_time);
      generate_block();

      // vesting factor decay
      BOOST_CHECK_EQUAL(db_api.get_gpos_info(alice_id).vesting_factor, 0.83333333333333337);
      BOOST_CHECK_EQUAL(db_api.get_gpos_info(bob_id).vesting_factor, 0.83333333333333337);
   }
   catch (fc::exception &e) {
      edump((e.to_detail_string()));
      throw;
   }
}

BOOST_AUTO_TEST_CASE( no_proposal )
{
   try {

   }
   catch (fc::exception &e) {
      edump((e.to_detail_string()));
      throw;
   }
}

BOOST_AUTO_TEST_CASE( database_api )
{
   ACTORS((alice)(bob));
   try {
      // move to hardfork
      generate_blocks( HARDFORK_GPOS_TIME );
      generate_block();

      // database api
      graphene::app::database_api db_api(db);

      const auto& core = asset_id_type()(db);

      // send some asset to alice and bob
      transfer( committee_account, alice_id, core.amount( 1000 ) );
      transfer( committee_account, bob_id, core.amount( 1000 ) );
      generate_block();

      // add some vesting to alice and bob
      create_vesting(alice_id, core.amount(100), vesting_balance_type::gpos);
      generate_block();

      // total balance is 100 rest of data at 0
      auto gpos_info = db_api.get_gpos_info(alice_id);
      BOOST_CHECK_EQUAL(gpos_info.vesting_factor, 0);
      BOOST_CHECK_EQUAL(gpos_info.award.amount.value, 0);
      BOOST_CHECK_EQUAL(gpos_info.total_amount.value, 100);

      create_vesting(bob_id, core.amount(100), vesting_balance_type::gpos);
      generate_block();

      // total gpos balance is now 200
      gpos_info = db_api.get_gpos_info(alice_id);
      BOOST_CHECK_EQUAL(gpos_info.total_amount.value, 200);

      // update default gpos and dividend interval to 10 days
      auto now = db.head_block_time();
      update_gpos_global(5184000, 864000, now); // 10 days subperiods
      update_payout_interval(core.symbol, now + fc::minutes(1), 60 * 60 * 24 * 10); // 10 days

      generate_block();

      // no votes for witness 1
      auto witness1 = witness_id_type(1)(db);
      BOOST_CHECK_EQUAL(witness1.total_votes, 0);

      // no votes for witness 2
      auto witness2 = witness_id_type(2)(db);
      BOOST_CHECK_EQUAL(witness2.total_votes, 0);

      // transfering some coins to distribution account.
      const auto& dividend_holder_asset_object = get_asset(GRAPHENE_SYMBOL);
      const auto& dividend_data = dividend_holder_asset_object.dividend_data(db);
      const account_object& dividend_distribution_account = dividend_data.dividend_distribution_account(db);
      transfer( committee_account, dividend_distribution_account.id, core.amount( 100 ) );
      generate_block();

      // award balance is now 100
      gpos_info = db_api.get_gpos_info(alice_id);
      BOOST_CHECK_EQUAL(gpos_info.vesting_factor, 0);
      BOOST_CHECK_EQUAL(gpos_info.award.amount.value, 100);
      BOOST_CHECK_EQUAL(gpos_info.total_amount.value, 200);

      // vote for witness1
      vote_for(alice_id, witness1.vote_id, alice_private_key);
      vote_for(bob_id, witness1.vote_id, bob_private_key);

      // go to maint
      generate_blocks(db.get_dynamic_global_properties().next_maintenance_time);

      // payment for alice and bob is done, distribution account is back in 0
      gpos_info = db_api.get_gpos_info(alice_id);
      BOOST_CHECK_EQUAL(gpos_info.vesting_factor, 1);
      BOOST_CHECK_EQUAL(gpos_info.award.amount.value, 0);
      BOOST_CHECK_EQUAL(gpos_info.total_amount.value, 200);

      advance_x_maint(10);

      // alice vesting coeffcient decay
      gpos_info = db_api.get_gpos_info(alice_id);
      BOOST_CHECK_EQUAL(gpos_info.vesting_factor, 0.83333333333333337);
      BOOST_CHECK_EQUAL(gpos_info.award.amount.value, 0);
      BOOST_CHECK_EQUAL(gpos_info.total_amount.value, 200);

      advance_x_maint(10);

      // vesting factor for alice decaying more
      gpos_info = db_api.get_gpos_info(alice_id);
      BOOST_CHECK_EQUAL(gpos_info.vesting_factor, 0.66666666666666663);
      BOOST_CHECK_EQUAL(gpos_info.award.amount.value, 0);
      BOOST_CHECK_EQUAL(gpos_info.total_amount.value, 200);
   }
   catch (fc::exception &e) {
      edump((e.to_detail_string()));
      throw;
   }
}

BOOST_AUTO_TEST_SUITE_END()
