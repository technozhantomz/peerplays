/*
 * Copyright (c) 2019 PBSA, and contributors.
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
#include "cli_fixture.hpp"

#include <fc/string.hpp>

#include <boost/test/unit_test.hpp>

#include <graphene/chain/config.hpp>
#include <graphene/chain/hardfork.hpp>

class son_test_helper
{
    cli_fixture& fixture_;

public:
    son_test_helper(cli_fixture& fixture):
        fixture_(fixture)
    {
        fixture_.init_nathan();
        fixture_.generate_blocks(HARDFORK_SON_FOR_ETHEREUM_TIME);
        fixture_.generate_block();
    }

    void create_son(const std::string& account_name, const std::string& son_url,
                    flat_map<sidechain_type, string>& sidechain_public_keys,
                    bool generate_maintenance = true)
    {
        graphene::wallet::brain_key_info bki;
        signed_transaction create_tx;
        signed_transaction transfer_tx;
        signed_transaction upgrade_tx;
        account_object son_account;
        son_object son_obj;

        // create son account
        bki = fixture_.con.wallet_api_ptr->suggest_brain_key();
        BOOST_CHECK(!bki.brain_priv_key.empty());
        create_tx = fixture_.con.wallet_api_ptr->create_account_with_brain_key(
              bki.brain_priv_key, account_name, "nathan", "nathan", true
        );

        fixture_.generate_block();

        // save the private key for this new account in the wallet file
        BOOST_CHECK(fixture_.con.wallet_api_ptr->import_key(account_name, bki.wif_priv_key));

        fixture_.generate_block();

        fixture_.con.wallet_api_ptr->save_wallet_file(fixture_.con.wallet_filename);

        // attempt to give son account some CORE tokens
        BOOST_TEST_MESSAGE("Transferring CORE tokens from Nathan to son account");
        transfer_tx = fixture_.con.wallet_api_ptr->transfer(
              "nathan", account_name, "65000", "1.3.0", "Here are some CORE token for your new account", true
        );

        fixture_.generate_block();

        // upgrade son account
        BOOST_TEST_MESSAGE("Upgrading son account to LTM");
        upgrade_tx = fixture_.con.wallet_api_ptr->upgrade_account(account_name, true);
        son_account = fixture_.con.wallet_api_ptr->get_account(account_name);

        // verify that the upgrade was successful
        BOOST_CHECK(son_account.is_lifetime_member());

        fixture_.generate_block();

        // create deposit vesting
        fixture_.con.wallet_api_ptr->create_vesting_balance(account_name,
                                                            "50", "1.3.0", vesting_balance_type::son, true);
        fixture_.generate_block();

        // create pay_vb vesting
        fixture_.con.wallet_api_ptr->create_vesting_balance(account_name, "1", "1.3.0", vesting_balance_type::normal, true);
        fixture_.generate_block();

        // check deposits are here
        auto deposits = fixture_.con.wallet_api_ptr->get_vesting_balances(account_name);
        BOOST_CHECK(deposits.size() >= 2);

        create_tx = fixture_.con.wallet_api_ptr->try_create_son(account_name, son_url,
                                                            sidechain_public_keys,
                                                            true);

        if (generate_maintenance)
            BOOST_CHECK(fixture_.generate_maintenance_block());
    }

};

///////////////////////
// SON CLI
///////////////////////
BOOST_FIXTURE_TEST_SUITE(son_cli, cli_fixture)

BOOST_AUTO_TEST_CASE( create_sons )
{
   BOOST_TEST_MESSAGE("SON cli wallet tests begin");
   try
   {
      flat_map<sidechain_type, string> sidechain_public_keys;

      son_test_helper sth(*this);

      sidechain_public_keys.clear();
      sidechain_public_keys[sidechain_type::bitcoin] = "bitcoin_address 1";
      sidechain_public_keys[sidechain_type::hive] = "hive account 1";
      sidechain_public_keys[sidechain_type::ethereum] = "ethereum address 1";
      sth.create_son("son1account", "http://son1", sidechain_public_keys);

      sidechain_public_keys.clear();
      sidechain_public_keys[sidechain_type::bitcoin] = "bitcoin_address 2";
      sidechain_public_keys[sidechain_type::hive] = "hive account 2";
      sidechain_public_keys[sidechain_type::ethereum] = "ethereum address 2";
      sth.create_son("son2account", "http://son2", sidechain_public_keys);

      auto son1_obj = con.wallet_api_ptr->get_son("son1account");
      BOOST_CHECK(son1_obj.son_account == con.wallet_api_ptr->get_account_id("son1account"));
      BOOST_CHECK_EQUAL(son1_obj.url, "http://son1");
      BOOST_CHECK_EQUAL(son1_obj.sidechain_public_keys[sidechain_type::bitcoin], "bitcoin_address 1");
      BOOST_CHECK_EQUAL(son1_obj.sidechain_public_keys[sidechain_type::hive], "hive account 1");
      BOOST_CHECK_EQUAL(son1_obj.sidechain_public_keys[sidechain_type::ethereum], "ethereum address 1");
      BOOST_REQUIRE(son1_obj.get_sidechain_vote_id(sidechain_type::bitcoin));
      BOOST_CHECK_EQUAL(son1_obj.get_sidechain_vote_id(sidechain_type::bitcoin)->instance(), 22);
      BOOST_REQUIRE(son1_obj.get_sidechain_vote_id(sidechain_type::hive));
      BOOST_CHECK_EQUAL(son1_obj.get_sidechain_vote_id(sidechain_type::hive)->instance(), 23);
      BOOST_REQUIRE(son1_obj.get_sidechain_vote_id(sidechain_type::ethereum));
      BOOST_CHECK_EQUAL(son1_obj.get_sidechain_vote_id(sidechain_type::ethereum)->instance(), 24);

      auto son2_obj = con.wallet_api_ptr->get_son("son2account");
      BOOST_CHECK(son2_obj.son_account == con.wallet_api_ptr->get_account_id("son2account"));
      BOOST_CHECK_EQUAL(son2_obj.url, "http://son2");
      BOOST_CHECK_EQUAL(son2_obj.sidechain_public_keys[sidechain_type::bitcoin], "bitcoin_address 2");
      BOOST_CHECK_EQUAL(son2_obj.sidechain_public_keys[sidechain_type::hive], "hive account 2");
      BOOST_CHECK_EQUAL(son2_obj.sidechain_public_keys[sidechain_type::ethereum], "ethereum address 2");
      BOOST_REQUIRE(son2_obj.get_sidechain_vote_id(sidechain_type::bitcoin));
      BOOST_CHECK_EQUAL(son2_obj.get_sidechain_vote_id(sidechain_type::bitcoin)->instance(), 25);
      BOOST_REQUIRE(son2_obj.get_sidechain_vote_id(sidechain_type::hive));
      BOOST_CHECK_EQUAL(son2_obj.get_sidechain_vote_id(sidechain_type::hive)->instance(), 26);
      BOOST_REQUIRE(son2_obj.get_sidechain_vote_id(sidechain_type::ethereum));
      BOOST_CHECK_EQUAL(son2_obj.get_sidechain_vote_id(sidechain_type::ethereum)->instance(), 27);

   } catch( fc::exception& e ) {
      BOOST_TEST_MESSAGE("SON cli wallet tests exception");
      edump((e.to_detail_string()));
      throw;
   }
   BOOST_TEST_MESSAGE("SON cli wallet tests end");
}

BOOST_AUTO_TEST_CASE( cli_update_son )
{
   try
   {
      BOOST_TEST_MESSAGE("Cli get_son and update_son Test");

      flat_map<sidechain_type, string> sidechain_public_keys;

      sidechain_public_keys.clear();
      sidechain_public_keys[sidechain_type::bitcoin] = "bitcoin_address 1";
      sidechain_public_keys[sidechain_type::hive] = "hive account 1";
      sidechain_public_keys[sidechain_type::ethereum] = "ethereum address 1";

      son_test_helper sth(*this);
      sth.create_son("sonmember", "http://sonmember", sidechain_public_keys);

      auto sonmember_acct = con.wallet_api_ptr->get_account("sonmember");

      // get_son
      auto son_data = con.wallet_api_ptr->get_son("sonmember");
      BOOST_CHECK(son_data.url == "http://sonmember");
      BOOST_CHECK(son_data.son_account == sonmember_acct.get_id());
      BOOST_CHECK_EQUAL(son_data.sidechain_public_keys[sidechain_type::bitcoin], "bitcoin_address 1");
      BOOST_CHECK_EQUAL(son_data.sidechain_public_keys[sidechain_type::hive], "hive account 1");
      BOOST_CHECK_EQUAL(son_data.sidechain_public_keys[sidechain_type::ethereum], "ethereum address 1");
      BOOST_REQUIRE(son_data.get_sidechain_vote_id(sidechain_type::bitcoin));
      BOOST_CHECK_EQUAL(son_data.get_sidechain_vote_id(sidechain_type::bitcoin)->instance(), 22);
      BOOST_REQUIRE(son_data.get_sidechain_vote_id(sidechain_type::hive));
      BOOST_CHECK_EQUAL(son_data.get_sidechain_vote_id(sidechain_type::hive)->instance(), 23);
      BOOST_REQUIRE(son_data.get_sidechain_vote_id(sidechain_type::ethereum));
      BOOST_CHECK_EQUAL(son_data.get_sidechain_vote_id(sidechain_type::ethereum)->instance(), 24);

      // update SON
      sidechain_public_keys.clear();
      sidechain_public_keys[sidechain_type::bitcoin] = "bitcoin_address 2";
      sidechain_public_keys[sidechain_type::hive] = "hive account 2";
      sidechain_public_keys[sidechain_type::ethereum] = "ethereum address 2";

      con.wallet_api_ptr->update_son("sonmember", "http://sonmember_updated", "", sidechain_public_keys, true);
      son_data = con.wallet_api_ptr->get_son("sonmember");
      BOOST_CHECK(son_data.url == "http://sonmember_updated");
      BOOST_CHECK_EQUAL(son_data.sidechain_public_keys[sidechain_type::bitcoin], "bitcoin_address 2");
      BOOST_CHECK_EQUAL(son_data.sidechain_public_keys[sidechain_type::hive], "hive account 2");
      BOOST_CHECK_EQUAL(son_data.sidechain_public_keys[sidechain_type::ethereum], "ethereum address 2");

      // update SON signing key
      sidechain_public_keys.clear();
      std::string new_key = GRAPHENE_ADDRESS_PREFIX + std::string("6Yaq5ZNTTkMM2kBBzV5jktr8ETsniCC3bnVD7eFmegRrLXfGGG");
      con.wallet_api_ptr->update_son("sonmember", "http://sonmember_updated2", new_key, sidechain_public_keys, true);
      son_data = con.wallet_api_ptr->get_son("sonmember");
      BOOST_CHECK(son_data.url == "http://sonmember_updated2");
      BOOST_CHECK(std::string(son_data.signing_key) == new_key);

   } catch( fc::exception& e ) {
      edump((e.to_detail_string()));
      throw;
   }
}

BOOST_AUTO_TEST_CASE( son_voting )
{
   BOOST_TEST_MESSAGE("SON Vote cli wallet tests begin");
   try
   {
      flat_map<sidechain_type, string> sidechain_public_keys;

      son_test_helper sth(*this);

      sidechain_public_keys.clear();
      sidechain_public_keys[sidechain_type::bitcoin] = "bitcoin_address 1";
      sidechain_public_keys[sidechain_type::hive] = "hive account 1";
      sidechain_public_keys[sidechain_type::ethereum] = "ethereum address 1";
      sth.create_son("son1account", "http://son1", sidechain_public_keys);

      sidechain_public_keys.clear();
      sidechain_public_keys[sidechain_type::bitcoin] = "bitcoin_address 2";
      sidechain_public_keys[sidechain_type::hive] = "hive account 2";
      sidechain_public_keys[sidechain_type::ethereum] = "ethereum address 2";
      sth.create_son("son2account", "http://son2", sidechain_public_keys);

      BOOST_CHECK(generate_maintenance_block());

      BOOST_TEST_MESSAGE("Voting for SONs");

      son_object son1_obj;
      son_object son2_obj;
      signed_transaction vote_son1_tx;
      signed_transaction vote_son2_tx;
      flat_map<sidechain_type, uint64_t> son1_start_votes, son1_end_votes;
      flat_map<sidechain_type, uint64_t> son2_start_votes, son2_end_votes;

      //! Get nathan account
      const auto nathan_account_object = con.wallet_api_ptr->get_account("nathan");

      son1_obj = con.wallet_api_ptr->get_son("son1account");
      son1_start_votes = son1_obj.total_votes;
      son2_obj = con.wallet_api_ptr->get_son("son2account");
      son2_start_votes = son2_obj.total_votes;

      con.wallet_api_ptr->create_vesting_balance("nathan", "1000", "1.3.0", vesting_balance_type::gpos, true);
      // Vote for a son1account
      BOOST_TEST_MESSAGE("Voting for son1account");
      vote_son1_tx = con.wallet_api_ptr->vote_for_son("nathan", "son1account", sidechain_type::bitcoin, true, true);
      vote_son1_tx = con.wallet_api_ptr->vote_for_son("nathan", "son1account", sidechain_type::hive, true, true);
      vote_son1_tx = con.wallet_api_ptr->vote_for_son("nathan", "son1account", sidechain_type::ethereum, true, true);
      BOOST_CHECK(generate_maintenance_block());

      // Verify that the vote is there
      son1_obj = con.wallet_api_ptr->get_son("son1account");
      son1_end_votes = son1_obj.total_votes;
      BOOST_CHECK(son1_end_votes[sidechain_type::bitcoin] > son1_start_votes[sidechain_type::bitcoin]);
      BOOST_CHECK(son1_end_votes[sidechain_type::hive] > son1_start_votes[sidechain_type::hive]);
      BOOST_CHECK(son1_end_votes[sidechain_type::ethereum] > son1_start_votes[sidechain_type::ethereum]);

      // Vote for a son2account
      BOOST_TEST_MESSAGE("Voting for son2account");
      vote_son2_tx = con.wallet_api_ptr->vote_for_son("nathan", "son2account", sidechain_type::bitcoin, true, true);
      vote_son2_tx = con.wallet_api_ptr->vote_for_son("nathan", "son2account", sidechain_type::hive, true, true);
      vote_son2_tx = con.wallet_api_ptr->vote_for_son("nathan", "son2account", sidechain_type::ethereum, true, true);
      BOOST_CHECK(generate_maintenance_block());

      // Verify that the vote is there
      son2_obj = con.wallet_api_ptr->get_son("son2account");
      son2_end_votes = son2_obj.total_votes;
      BOOST_CHECK(son2_end_votes[sidechain_type::bitcoin] > son2_start_votes[sidechain_type::bitcoin]);
      BOOST_CHECK(son2_end_votes[sidechain_type::hive] > son2_start_votes[sidechain_type::hive]);
      BOOST_CHECK(son2_end_votes[sidechain_type::ethereum] > son2_start_votes[sidechain_type::ethereum]);

      //! Check son1account voters
      auto voters_for_son1account = con.wallet_api_ptr->get_voters("son1account").voters_for_son;
      BOOST_REQUIRE(voters_for_son1account);
      BOOST_REQUIRE_EQUAL(voters_for_son1account->at(sidechain_type::bitcoin).voters.size(), 1);
      BOOST_CHECK_EQUAL((uint32_t)voters_for_son1account->at(sidechain_type::bitcoin).voters[0].instance, nathan_account_object.id.instance());
      BOOST_REQUIRE_EQUAL(voters_for_son1account->at(sidechain_type::hive).voters.size(), 1);
      BOOST_CHECK_EQUAL((uint32_t)voters_for_son1account->at(sidechain_type::hive).voters[0].instance, nathan_account_object.id.instance());
      BOOST_REQUIRE_EQUAL(voters_for_son1account->at(sidechain_type::ethereum).voters.size(), 1);
      BOOST_CHECK_EQUAL((uint32_t)voters_for_son1account->at(sidechain_type::ethereum).voters[0].instance, nathan_account_object.id.instance());

      //! Check son2account voters
      auto voters_for_son2account = con.wallet_api_ptr->get_voters("son2account").voters_for_son;
      BOOST_REQUIRE(voters_for_son2account);
      BOOST_REQUIRE_EQUAL(voters_for_son2account->at(sidechain_type::bitcoin).voters.size(), 1);
      BOOST_CHECK_EQUAL((uint32_t)voters_for_son2account->at(sidechain_type::bitcoin).voters[0].instance, nathan_account_object.id.instance());
      BOOST_REQUIRE_EQUAL(voters_for_son2account->at(sidechain_type::hive).voters.size(), 1);
      BOOST_CHECK_EQUAL((uint32_t)voters_for_son2account->at(sidechain_type::hive).voters[0].instance, nathan_account_object.id.instance());
      BOOST_REQUIRE_EQUAL(voters_for_son2account->at(sidechain_type::ethereum).voters.size(), 1);
      BOOST_CHECK_EQUAL((uint32_t)voters_for_son2account->at(sidechain_type::ethereum).voters[0].instance, nathan_account_object.id.instance());

      //! Check votes of nathan
      auto nathan_votes_for_son = con.wallet_api_ptr->get_votes("nathan").votes_for_sons;
      BOOST_REQUIRE(nathan_votes_for_son);
      BOOST_REQUIRE_EQUAL(nathan_votes_for_son->at(sidechain_type::bitcoin).size(), 2);
      BOOST_CHECK_EQUAL(nathan_votes_for_son->at(sidechain_type::bitcoin).at(0).id.instance(), son1_obj.id.instance());
      BOOST_CHECK_EQUAL(nathan_votes_for_son->at(sidechain_type::bitcoin).at(1).id.instance(), son2_obj.id.instance());
      BOOST_REQUIRE_EQUAL(nathan_votes_for_son->at(sidechain_type::hive).size(), 2);
      BOOST_CHECK_EQUAL(nathan_votes_for_son->at(sidechain_type::hive).at(0).id.instance(), son1_obj.id.instance());
      BOOST_CHECK_EQUAL(nathan_votes_for_son->at(sidechain_type::hive).at(1).id.instance(), son2_obj.id.instance());
      BOOST_REQUIRE_EQUAL(nathan_votes_for_son->at(sidechain_type::ethereum).size(), 2);
      BOOST_CHECK_EQUAL(nathan_votes_for_son->at(sidechain_type::ethereum).at(0).id.instance(), son1_obj.id.instance());
      BOOST_CHECK_EQUAL(nathan_votes_for_son->at(sidechain_type::ethereum).at(1).id.instance(), son2_obj.id.instance());

      // Withdraw vote for a son1account
      BOOST_TEST_MESSAGE("Withdraw vote for a son1account");
      vote_son1_tx = con.wallet_api_ptr->vote_for_son("nathan", "son1account", sidechain_type::bitcoin, false, true);
      vote_son1_tx = con.wallet_api_ptr->vote_for_son("nathan", "son1account", sidechain_type::hive, false, true);
      vote_son1_tx = con.wallet_api_ptr->vote_for_son("nathan", "son1account", sidechain_type::ethereum, false, true);
      BOOST_CHECK(generate_maintenance_block());

      // Verify that the vote is removed
      son1_obj = con.wallet_api_ptr->get_son("son1account");
      son1_end_votes = son1_obj.total_votes;
      BOOST_CHECK(son1_end_votes[sidechain_type::bitcoin] == son1_start_votes[sidechain_type::bitcoin]);
      BOOST_CHECK(son1_end_votes[sidechain_type::hive] == son1_start_votes[sidechain_type::hive]);
      BOOST_CHECK(son1_end_votes[sidechain_type::ethereum] == son1_start_votes[sidechain_type::ethereum]);

      //! Check son1account voters
      voters_for_son1account = con.wallet_api_ptr->get_voters("son1account").voters_for_son;
      BOOST_REQUIRE(voters_for_son1account);
      BOOST_CHECK_EQUAL(voters_for_son1account->at(sidechain_type::bitcoin).voters.size(), 0);
      BOOST_CHECK_EQUAL(voters_for_son1account->at(sidechain_type::hive).voters.size(), 0);
      BOOST_CHECK_EQUAL(voters_for_son1account->at(sidechain_type::ethereum).voters.size(), 0);

      //! Check votes of nathan
      nathan_votes_for_son = con.wallet_api_ptr->get_votes("nathan").votes_for_sons;
      BOOST_REQUIRE(nathan_votes_for_son);
      BOOST_REQUIRE_EQUAL(nathan_votes_for_son->at(sidechain_type::bitcoin).size(), 1);
      BOOST_CHECK_EQUAL(nathan_votes_for_son->at(sidechain_type::bitcoin).at(0).id.instance(), son2_obj.id.instance());
      BOOST_CHECK_EQUAL(nathan_votes_for_son->at(sidechain_type::hive).size(), 1);
      BOOST_CHECK_EQUAL(nathan_votes_for_son->at(sidechain_type::hive).at(0).id.instance(), son2_obj.id.instance());
      BOOST_CHECK_EQUAL(nathan_votes_for_son->at(sidechain_type::ethereum).size(), 1);
      BOOST_CHECK_EQUAL(nathan_votes_for_son->at(sidechain_type::ethereum).at(0).id.instance(), son2_obj.id.instance());

      // Withdraw vote for a son2account
      BOOST_TEST_MESSAGE("Withdraw vote for a son2account");
      vote_son2_tx = con.wallet_api_ptr->vote_for_son("nathan", "son2account", sidechain_type::bitcoin, false, true);
      vote_son2_tx = con.wallet_api_ptr->vote_for_son("nathan", "son2account", sidechain_type::hive, false, true);
      vote_son2_tx = con.wallet_api_ptr->vote_for_son("nathan", "son2account", sidechain_type::ethereum, false, true);
      BOOST_CHECK(generate_maintenance_block());

      // Verify that the vote is removed
      son2_obj = con.wallet_api_ptr->get_son("son2account");
      son2_end_votes = son2_obj.total_votes;
      BOOST_CHECK(son2_end_votes[sidechain_type::bitcoin] == son2_start_votes[sidechain_type::bitcoin]);
      BOOST_CHECK(son2_end_votes[sidechain_type::hive] == son2_start_votes[sidechain_type::hive]);
      BOOST_CHECK(son2_end_votes[sidechain_type::ethereum] == son2_start_votes[sidechain_type::ethereum]);

      //! Check son2account voters
      voters_for_son2account = con.wallet_api_ptr->get_voters("son2account").voters_for_son;
      BOOST_REQUIRE(voters_for_son2account);
      BOOST_CHECK_EQUAL(voters_for_son2account->at(sidechain_type::bitcoin).voters.size(), 0);
      BOOST_CHECK_EQUAL(voters_for_son2account->at(sidechain_type::hive).voters.size(), 0);
      BOOST_CHECK_EQUAL(voters_for_son2account->at(sidechain_type::ethereum).voters.size(), 0);

      //! Check votes of nathan
      nathan_votes_for_son = con.wallet_api_ptr->get_votes("nathan").votes_for_sons;
      BOOST_CHECK(!nathan_votes_for_son);

   } catch( fc::exception& e ) {
      BOOST_TEST_MESSAGE("SON cli wallet tests exception");
      edump((e.to_detail_string()));
      throw;
   }
   BOOST_TEST_MESSAGE("SON Vote cli wallet tests end");
}

BOOST_FIXTURE_TEST_CASE( select_top_fifteen_sons, cli_fixture )
{
   BOOST_TEST_MESSAGE("SON cli wallet tests begin");
   try
   {
      son_test_helper sth(*this);

      graphene::wallet::brain_key_info bki;
      signed_transaction create_tx;
      signed_transaction transfer_tx;
      signed_transaction upgrade_tx;
      signed_transaction vote_tx;
      account_object acc_before_upgrade, acc_after_upgrade;
      son_object son_obj;
      global_property_object gpo;

      gpo = con.wallet_api_ptr->get_global_properties();
      //! Set son number as 5 (as the begining son count)
      unsigned int son_number = 5;

      flat_map<sidechain_type, string> sidechain_public_keys;

      // create son accounts
      for(unsigned int i = 0; i < son_number + 1; i++)
      {
          sidechain_public_keys.clear();
          sidechain_public_keys[sidechain_type::bitcoin] = "bitcoin_address " + fc::to_pretty_string(i);
          sidechain_public_keys[sidechain_type::hive] = "hive account " + fc::to_pretty_string(i);
          sidechain_public_keys[sidechain_type::ethereum] = "ethereum address " + fc::to_pretty_string(i);
          sth.create_son("sonaccount" + fc::to_pretty_string(i),
                         "http://son" + fc::to_pretty_string(i),
                         sidechain_public_keys,
                         false);
      }
      BOOST_CHECK(generate_maintenance_block());

      BOOST_TEST_MESSAGE("Voting for SONs");
      for(unsigned int i = 0; i < son_number + 1; i++)
      {
          con.wallet_api_ptr->transfer(
              "nathan", "sonaccount" + fc::to_pretty_string(i), "1000", "1.3.0", "Here are some CORE tokens for your new account", true );
          con.wallet_api_ptr->create_vesting_balance("sonaccount" + fc::to_pretty_string(i), "500", "1.3.0", vesting_balance_type::gpos, true);

          std::string name = "sonaccount" + fc::to_pretty_string(i);
          vote_tx = con.wallet_api_ptr->vote_for_son(name, name, sidechain_type::bitcoin, true, true);
          vote_tx = con.wallet_api_ptr->vote_for_son(name, name, sidechain_type::hive, true, true);
          vote_tx = con.wallet_api_ptr->vote_for_son(name, name, sidechain_type::ethereum, true, true);
      }
      BOOST_CHECK(generate_maintenance_block());

      for(unsigned int i = 0; i < son_number; i++)
      {
          std::string name1 = "sonaccount" + fc::to_pretty_string(i);
          std::string name2 = "sonaccount" + fc::to_pretty_string(i + 1);
          vote_tx = con.wallet_api_ptr->vote_for_son(name1, name2, sidechain_type::bitcoin, true, true);
          vote_tx = con.wallet_api_ptr->vote_for_son(name1, name2, sidechain_type::hive, true, true);
          vote_tx = con.wallet_api_ptr->vote_for_son(name1, name2, sidechain_type::ethereum, true, true);
      }
      gpo = con.wallet_api_ptr->get_global_properties();
      BOOST_TEST_MESSAGE("gpo active_sons[bitcoin]: " << gpo.active_sons.at(sidechain_type::bitcoin).size());
      BOOST_TEST_MESSAGE("gpo active_sons[hive]: " << gpo.active_sons.at(sidechain_type::hive).size());
      BOOST_TEST_MESSAGE("gpo active_sons[ethereum]: " << gpo.active_sons.at(sidechain_type::ethereum).size());
      BOOST_CHECK(generate_maintenance_block());
      gpo = con.wallet_api_ptr->get_global_properties();
      BOOST_TEST_MESSAGE("gpo active_sons[bitcoin]: " << gpo.active_sons.at(sidechain_type::bitcoin).size());
      BOOST_TEST_MESSAGE("gpo active_sons[hive]: " << gpo.active_sons.at(sidechain_type::hive).size());
      BOOST_TEST_MESSAGE("gpo active_sons[ethereum]: " << gpo.active_sons.at(sidechain_type::ethereum).size());

      for(unsigned int i = 0; i < son_number - 1; i++)
      {
          std::string name1 = "sonaccount" + fc::to_pretty_string(i + 2);
          std::string name2 = "sonaccount" + fc::to_pretty_string(i);
          vote_tx = con.wallet_api_ptr->vote_for_son(name1, name2, sidechain_type::bitcoin, true, true);
          vote_tx = con.wallet_api_ptr->vote_for_son(name1, name2, sidechain_type::hive, true, true);
          vote_tx = con.wallet_api_ptr->vote_for_son(name1, name2, sidechain_type::ethereum, true, true);
      }
      gpo = con.wallet_api_ptr->get_global_properties();
      BOOST_TEST_MESSAGE("gpo active_sons[bitcoin]: " << gpo.active_sons.at(sidechain_type::bitcoin).size());
      BOOST_TEST_MESSAGE("gpo active_sons[hive]: " << gpo.active_sons.at(sidechain_type::hive).size());
      BOOST_TEST_MESSAGE("gpo active_sons[ethereum]: " << gpo.active_sons.at(sidechain_type::ethereum).size());
      BOOST_CHECK(generate_maintenance_block());
      gpo = con.wallet_api_ptr->get_global_properties();
      BOOST_TEST_MESSAGE("gpo active_sons[bitcoin]: " << gpo.active_sons.at(sidechain_type::bitcoin).size());
      BOOST_TEST_MESSAGE("gpo active_sons[hive]: " << gpo.active_sons.at(sidechain_type::hive).size());
      BOOST_TEST_MESSAGE("gpo active_sons[ethereum]: " << gpo.active_sons.at(sidechain_type::ethereum).size());

      for(unsigned int i = 0; i < son_number - 2; i++)
      {
          std::string name1 = "sonaccount" + fc::to_pretty_string(i + 3);
          std::string name2 = "sonaccount" + fc::to_pretty_string(i);
          vote_tx = con.wallet_api_ptr->vote_for_son(name1, name2, sidechain_type::bitcoin, true, true);
          vote_tx = con.wallet_api_ptr->vote_for_son(name1, name2, sidechain_type::hive, true, true);
          vote_tx = con.wallet_api_ptr->vote_for_son(name1, name2, sidechain_type::ethereum, true, true);
      }
      gpo = con.wallet_api_ptr->get_global_properties();
      BOOST_TEST_MESSAGE("gpo active_sons[bitcoin]: " << gpo.active_sons.at(sidechain_type::bitcoin).size());
      BOOST_TEST_MESSAGE("gpo active_sons[hive]: " << gpo.active_sons.at(sidechain_type::hive).size());
      BOOST_TEST_MESSAGE("gpo active_sons[ethereum]: " << gpo.active_sons.at(sidechain_type::ethereum).size());
      BOOST_CHECK(generate_maintenance_block());

      BOOST_CHECK(gpo.active_sons.at(sidechain_type::bitcoin).size() == son_number);
      BOOST_CHECK(gpo.active_sons.at(sidechain_type::hive).size() == son_number);
      BOOST_CHECK(gpo.active_sons.at(sidechain_type::ethereum).size() == son_number);

   } catch( fc::exception& e ) {
      BOOST_TEST_MESSAGE("SON cli wallet tests exception");
      edump((e.to_detail_string()));
      throw;
   }
   BOOST_TEST_MESSAGE("SON cli wallet tests end");
}

BOOST_AUTO_TEST_CASE( list_son )
{
   BOOST_TEST_MESSAGE("List SONs cli wallet tests begin");
   try
   {
      flat_map<sidechain_type, string> sidechain_public_keys;

      son_test_helper sth(*this);

      sidechain_public_keys.clear();
      sidechain_public_keys[sidechain_type::bitcoin] = "bitcoin_address 1";
      sidechain_public_keys[sidechain_type::hive] = "hive account 1";
      sidechain_public_keys[sidechain_type::ethereum] = "ethereum address 1";
      sth.create_son("son1account", "http://son1", sidechain_public_keys);

      sidechain_public_keys.clear();
      sidechain_public_keys[sidechain_type::bitcoin] = "bitcoin_address 2";
      sidechain_public_keys[sidechain_type::hive] = "hive account 2";
      sidechain_public_keys[sidechain_type::ethereum] = "ethereum address 2";
      sth.create_son("son2account", "http://son2", sidechain_public_keys);

      auto res = con.wallet_api_ptr->list_sons("", 100);
      BOOST_REQUIRE(res.size() == 2);
      BOOST_CHECK(res.find("son1account") != res.end());
      BOOST_CHECK(res.find("son2account") != res.end());

   } catch( fc::exception& e ) {
      BOOST_TEST_MESSAGE("SON cli wallet tests exception");
      edump((e.to_detail_string()));
      throw;
   }
   BOOST_TEST_MESSAGE("List SONs cli wallet tests end");
}

BOOST_AUTO_TEST_CASE( update_son_votes_test )
{
   BOOST_TEST_MESSAGE("SON update_son_votes cli wallet tests begin");
   try
   {
      flat_map<sidechain_type, string> sidechain_public_keys;

      son_test_helper sth(*this);

      sidechain_public_keys.clear();
      sidechain_public_keys[sidechain_type::bitcoin] = "bitcoin_address 1";
      sidechain_public_keys[sidechain_type::hive] = "hive account 1";
      sidechain_public_keys[sidechain_type::ethereum] = "ethereum address 1";
      sth.create_son("son1account", "http://son1", sidechain_public_keys);

      sidechain_public_keys.clear();
      sidechain_public_keys[sidechain_type::bitcoin] = "bitcoin_address 2";
      sidechain_public_keys[sidechain_type::hive] = "hive account 2";
      sidechain_public_keys[sidechain_type::ethereum] = "ethereum address 2";
      sth.create_son("son2account", "http://son2", sidechain_public_keys);

      BOOST_CHECK(generate_maintenance_block());

      BOOST_TEST_MESSAGE("Vote for 2 accounts with update_son_votes");

      son_object son1_obj;
      son_object son2_obj;
      flat_map<sidechain_type, uint64_t> son1_start_votes, son1_end_votes;
      flat_map<sidechain_type, uint64_t> son2_start_votes, son2_end_votes;

      //! Get nathan account
      const auto nathan_account_object = con.wallet_api_ptr->get_account("nathan");

      // Get votes at start
      son1_obj = con.wallet_api_ptr->get_son("son1account");
      son1_start_votes = son1_obj.total_votes;
      son2_obj = con.wallet_api_ptr->get_son("son2account");
      son2_start_votes = son2_obj.total_votes;

      std::vector<std::string> accepted;
      std::vector<std::string> rejected;
      signed_transaction update_votes_tx;

      // Vote for both SONs
      accepted.clear();
      rejected.clear();
      accepted.push_back("son1account");
      accepted.push_back("son2account");
      con.wallet_api_ptr->create_vesting_balance("nathan", "1000", "1.3.0", vesting_balance_type::gpos, true);
      update_votes_tx = con.wallet_api_ptr->update_son_votes("nathan", accepted, rejected,
                                                             sidechain_type::bitcoin, 2, true);
      update_votes_tx = con.wallet_api_ptr->update_son_votes("nathan", accepted, rejected,
                                                             sidechain_type::hive, 2, true);
      update_votes_tx = con.wallet_api_ptr->update_son_votes("nathan", accepted, rejected,
                                                             sidechain_type::ethereum, 2, true);
      generate_block();
      BOOST_CHECK(generate_maintenance_block());

      // Verify the votes
      son1_obj = con.wallet_api_ptr->get_son("son1account");
      son1_end_votes = son1_obj.total_votes;
      BOOST_CHECK(son1_end_votes[sidechain_type::bitcoin] > son1_start_votes[sidechain_type::bitcoin]);
      BOOST_CHECK(son1_end_votes[sidechain_type::hive] > son1_start_votes[sidechain_type::hive]);
      BOOST_CHECK(son1_end_votes[sidechain_type::ethereum] > son1_start_votes[sidechain_type::ethereum]);
      son1_start_votes = son1_end_votes;
      son2_obj = con.wallet_api_ptr->get_son("son2account");
      son2_end_votes = son2_obj.total_votes;
      BOOST_CHECK(son2_end_votes[sidechain_type::bitcoin] > son2_start_votes[sidechain_type::bitcoin]);
      BOOST_CHECK(son2_end_votes[sidechain_type::hive] > son2_start_votes[sidechain_type::hive]);
      BOOST_CHECK(son2_end_votes[sidechain_type::ethereum] > son2_start_votes[sidechain_type::ethereum]);
      son2_start_votes = son2_end_votes;

      //! Check son1account voters
      auto voters_for_son1account = con.wallet_api_ptr->get_voters("son1account").voters_for_son;
      BOOST_REQUIRE(voters_for_son1account);
      BOOST_REQUIRE_EQUAL(voters_for_son1account->at(sidechain_type::bitcoin).voters.size(), 1);
      BOOST_CHECK_EQUAL((uint32_t)voters_for_son1account->at(sidechain_type::bitcoin).voters[0].instance, nathan_account_object.id.instance());
      BOOST_REQUIRE_EQUAL(voters_for_son1account->at(sidechain_type::hive).voters.size(), 1);
      BOOST_CHECK_EQUAL((uint32_t)voters_for_son1account->at(sidechain_type::hive).voters[0].instance, nathan_account_object.id.instance());
      BOOST_REQUIRE_EQUAL(voters_for_son1account->at(sidechain_type::ethereum).voters.size(), 1);
      BOOST_CHECK_EQUAL((uint32_t)voters_for_son1account->at(sidechain_type::ethereum).voters[0].instance, nathan_account_object.id.instance());

      //! Check son2account voters
      auto voters_for_son2account = con.wallet_api_ptr->get_voters("son2account").voters_for_son;
      BOOST_REQUIRE(voters_for_son2account);
      BOOST_REQUIRE_EQUAL(voters_for_son2account->at(sidechain_type::bitcoin).voters.size(), 1);
      BOOST_CHECK_EQUAL((uint32_t)voters_for_son2account->at(sidechain_type::bitcoin).voters[0].instance, nathan_account_object.id.instance());
      BOOST_REQUIRE_EQUAL(voters_for_son2account->at(sidechain_type::hive).voters.size(), 1);
      BOOST_CHECK_EQUAL((uint32_t)voters_for_son2account->at(sidechain_type::hive).voters[0].instance, nathan_account_object.id.instance());
      BOOST_REQUIRE_EQUAL(voters_for_son2account->at(sidechain_type::ethereum).voters.size(), 1);
      BOOST_CHECK_EQUAL((uint32_t)voters_for_son2account->at(sidechain_type::ethereum).voters[0].instance, nathan_account_object.id.instance());

      //! Check votes of nathan
      auto nathan_votes_for_son = con.wallet_api_ptr->get_votes("nathan").votes_for_sons;
      BOOST_REQUIRE(nathan_votes_for_son);
      BOOST_REQUIRE_EQUAL(nathan_votes_for_son->at(sidechain_type::bitcoin).size(), 2);
      BOOST_CHECK_EQUAL(nathan_votes_for_son->at(sidechain_type::bitcoin).at(0).id.instance(), son1_obj.id.instance());
      BOOST_CHECK_EQUAL(nathan_votes_for_son->at(sidechain_type::bitcoin).at(1).id.instance(), son2_obj.id.instance());
      BOOST_REQUIRE_EQUAL(nathan_votes_for_son->at(sidechain_type::hive).size(), 2);
      BOOST_CHECK_EQUAL(nathan_votes_for_son->at(sidechain_type::hive).at(0).id.instance(), son1_obj.id.instance());
      BOOST_CHECK_EQUAL(nathan_votes_for_son->at(sidechain_type::hive).at(1).id.instance(), son2_obj.id.instance());
      BOOST_REQUIRE_EQUAL(nathan_votes_for_son->at(sidechain_type::ethereum).size(), 2);
      BOOST_CHECK_EQUAL(nathan_votes_for_son->at(sidechain_type::ethereum).at(0).id.instance(), son1_obj.id.instance());
      BOOST_CHECK_EQUAL(nathan_votes_for_son->at(sidechain_type::ethereum).at(1).id.instance(), son2_obj.id.instance());

      // Withdraw vote for SON 1
      accepted.clear();
      rejected.clear();
      rejected.push_back("son1account");
      con.wallet_api_ptr->create_vesting_balance("nathan", "1000", "1.3.0", vesting_balance_type::gpos, true);
      update_votes_tx = con.wallet_api_ptr->update_son_votes("nathan", accepted, rejected,
                                                             sidechain_type::bitcoin, 1, true);
      update_votes_tx = con.wallet_api_ptr->update_son_votes("nathan", accepted, rejected,
                                                             sidechain_type::hive, 1, true);
      update_votes_tx = con.wallet_api_ptr->update_son_votes("nathan", accepted, rejected,
                                                             sidechain_type::ethereum, 1, true);
      BOOST_CHECK(generate_maintenance_block());

      // Verify the votes
      son1_obj = con.wallet_api_ptr->get_son("son1account");
      son1_end_votes = son1_obj.total_votes;
      BOOST_CHECK(son1_end_votes[sidechain_type::bitcoin] < son1_start_votes[sidechain_type::bitcoin]);
      BOOST_CHECK(son1_end_votes[sidechain_type::hive] < son1_start_votes[sidechain_type::hive]);
      BOOST_CHECK(son1_end_votes[sidechain_type::ethereum] < son1_start_votes[sidechain_type::ethereum]);
      son1_start_votes = son1_end_votes;
      son2_obj = con.wallet_api_ptr->get_son("son2account");
      // voice distribution changed, SON2 now has all voices
      son2_end_votes = son2_obj.total_votes;
      BOOST_CHECK(son2_end_votes[sidechain_type::bitcoin] > son2_start_votes[sidechain_type::bitcoin]); // nathan spent funds for vb, it has different voting power
      BOOST_CHECK(son2_end_votes[sidechain_type::hive] > son2_start_votes[sidechain_type::hive]);
      BOOST_CHECK(son2_end_votes[sidechain_type::ethereum] > son2_start_votes[sidechain_type::ethereum]);
      son2_start_votes = son2_end_votes;

      //! Check son1account voters
      voters_for_son1account = con.wallet_api_ptr->get_voters("son1account").voters_for_son;
      BOOST_REQUIRE(voters_for_son1account);
      BOOST_CHECK_EQUAL(voters_for_son1account->at(sidechain_type::bitcoin).voters.size(), 0);
      BOOST_CHECK_EQUAL(voters_for_son1account->at(sidechain_type::hive).voters.size(), 0);
      BOOST_CHECK_EQUAL(voters_for_son1account->at(sidechain_type::ethereum).voters.size(), 0);

      //! Check votes of nathan
      nathan_votes_for_son = con.wallet_api_ptr->get_votes("nathan").votes_for_sons;
      BOOST_REQUIRE(nathan_votes_for_son);
      BOOST_REQUIRE_EQUAL(nathan_votes_for_son->at(sidechain_type::bitcoin).size(), 1);
      BOOST_CHECK_EQUAL(nathan_votes_for_son->at(sidechain_type::bitcoin).at(0).id.instance(), son2_obj.id.instance());
      BOOST_REQUIRE_EQUAL(nathan_votes_for_son->at(sidechain_type::hive).size(), 1);
      BOOST_CHECK_EQUAL(nathan_votes_for_son->at(sidechain_type::hive).at(0).id.instance(), son2_obj.id.instance());
      BOOST_REQUIRE_EQUAL(nathan_votes_for_son->at(sidechain_type::ethereum).size(), 1);
      BOOST_CHECK_EQUAL(nathan_votes_for_son->at(sidechain_type::ethereum).at(0).id.instance(), son2_obj.id.instance());

      // Try to reject incorrect SON
      accepted.clear();
      rejected.clear();
      rejected.push_back("son1accnt");
      BOOST_CHECK_THROW(update_votes_tx = con.wallet_api_ptr->update_son_votes("nathan", accepted, rejected,
                                                                               sidechain_type::bitcoin, 1, true), fc::exception);
      BOOST_CHECK_THROW(update_votes_tx = con.wallet_api_ptr->update_son_votes("nathan", accepted, rejected,
                                                                               sidechain_type::hive, 1, true), fc::exception);
      BOOST_CHECK_THROW(update_votes_tx = con.wallet_api_ptr->update_son_votes("nathan", accepted, rejected,
                                                                               sidechain_type::ethereum, 1, true), fc::exception);
      generate_block();

      // Verify the votes
      son1_obj = con.wallet_api_ptr->get_son("son1account");
      son1_end_votes = son1_obj.total_votes;
      BOOST_CHECK(son1_end_votes[sidechain_type::bitcoin] == son1_start_votes[sidechain_type::bitcoin]);
      BOOST_CHECK(son1_end_votes[sidechain_type::hive] == son1_start_votes[sidechain_type::hive]);
      BOOST_CHECK(son1_end_votes[sidechain_type::ethereum] == son1_start_votes[sidechain_type::ethereum]);
      son1_start_votes = son1_end_votes;
      son2_obj = con.wallet_api_ptr->get_son("son2account");
      son2_end_votes = son2_obj.total_votes;
      BOOST_CHECK(son2_end_votes[sidechain_type::bitcoin] == son2_start_votes[sidechain_type::bitcoin]);
      BOOST_CHECK(son2_end_votes[sidechain_type::hive] == son2_start_votes[sidechain_type::hive]);
      BOOST_CHECK(son2_end_votes[sidechain_type::ethereum] == son2_start_votes[sidechain_type::ethereum]);
      son2_start_votes = son2_end_votes;

      //! Check votes of nathan
      nathan_votes_for_son = con.wallet_api_ptr->get_votes("nathan").votes_for_sons;
      BOOST_REQUIRE(nathan_votes_for_son);
      BOOST_REQUIRE_EQUAL(nathan_votes_for_son->at(sidechain_type::bitcoin).size(), 1);
      BOOST_CHECK_EQUAL(nathan_votes_for_son->at(sidechain_type::bitcoin).at(0).id.instance(), son2_obj.id.instance());
      BOOST_REQUIRE_EQUAL(nathan_votes_for_son->at(sidechain_type::hive).size(), 1);
      BOOST_CHECK_EQUAL(nathan_votes_for_son->at(sidechain_type::hive).at(0).id.instance(), son2_obj.id.instance());
      BOOST_REQUIRE_EQUAL(nathan_votes_for_son->at(sidechain_type::ethereum).size(), 1);
      BOOST_CHECK_EQUAL(nathan_votes_for_son->at(sidechain_type::ethereum).at(0).id.instance(), son2_obj.id.instance());

      // Reject SON2
      accepted.clear();
      rejected.clear();
      rejected.push_back("son2account");
      update_votes_tx = con.wallet_api_ptr->update_son_votes("nathan", accepted, rejected,
                                                             sidechain_type::bitcoin, 0, true);
      update_votes_tx = con.wallet_api_ptr->update_son_votes("nathan", accepted, rejected,
                                                             sidechain_type::hive, 0, true);
      update_votes_tx = con.wallet_api_ptr->update_son_votes("nathan", accepted, rejected,
                                                             sidechain_type::ethereum, 0, true);
      BOOST_CHECK(generate_maintenance_block());

      // Verify the votes
      son1_obj = con.wallet_api_ptr->get_son("son1account");
      son1_end_votes = son1_obj.total_votes;
      BOOST_CHECK(son1_end_votes[sidechain_type::bitcoin] == son1_start_votes[sidechain_type::bitcoin]);
      BOOST_CHECK(son1_end_votes[sidechain_type::hive] == son1_start_votes[sidechain_type::hive]);
      BOOST_CHECK(son1_end_votes[sidechain_type::ethereum] == son1_start_votes[sidechain_type::ethereum]);
      son1_start_votes = son1_end_votes;
      son2_obj = con.wallet_api_ptr->get_son("son2account");
      son2_end_votes = son2_obj.total_votes;
      BOOST_CHECK(son2_end_votes[sidechain_type::bitcoin] < son2_start_votes[sidechain_type::bitcoin]);
      BOOST_CHECK(son2_end_votes[sidechain_type::hive] < son2_start_votes[sidechain_type::hive]);
      BOOST_CHECK(son2_end_votes[sidechain_type::ethereum] < son2_start_votes[sidechain_type::ethereum]);
      son2_start_votes = son2_end_votes;

      //! Check son2account voters
      voters_for_son2account = con.wallet_api_ptr->get_voters("son2account").voters_for_son;
      BOOST_REQUIRE(voters_for_son2account);
      BOOST_CHECK_EQUAL(voters_for_son2account->at(sidechain_type::bitcoin).voters.size(), 0);
      BOOST_CHECK_EQUAL(voters_for_son2account->at(sidechain_type::hive).voters.size(), 0);
      BOOST_CHECK_EQUAL(voters_for_son2account->at(sidechain_type::ethereum).voters.size(), 0);

      //! Check votes of nathan
      nathan_votes_for_son = con.wallet_api_ptr->get_votes("nathan").votes_for_sons;
      BOOST_REQUIRE(!nathan_votes_for_son);

      // Try to accept and reject the same SON
      accepted.clear();
      rejected.clear();
      rejected.push_back("son1accnt");
      accepted.push_back("son1accnt");
      BOOST_REQUIRE_THROW(update_votes_tx = con.wallet_api_ptr->update_son_votes("nathan", accepted, rejected,
                                                                                 sidechain_type::bitcoin, 1, true), fc::exception);
      BOOST_REQUIRE_THROW(update_votes_tx = con.wallet_api_ptr->update_son_votes("nathan", accepted, rejected,
                                                                                 sidechain_type::hive, 1, true), fc::exception);
      BOOST_REQUIRE_THROW(update_votes_tx = con.wallet_api_ptr->update_son_votes("nathan", accepted, rejected,
                                                                                 sidechain_type::ethereum, 1, true), fc::exception);
      BOOST_CHECK(generate_maintenance_block());

      // Verify the votes
      son1_obj = con.wallet_api_ptr->get_son("son1account");
      son1_end_votes = son1_obj.total_votes;
      BOOST_CHECK(son1_end_votes[sidechain_type::bitcoin] == son1_start_votes[sidechain_type::bitcoin]);
      BOOST_CHECK(son1_end_votes[sidechain_type::hive] == son1_start_votes[sidechain_type::hive]);
      BOOST_CHECK(son1_end_votes[sidechain_type::ethereum] == son1_start_votes[sidechain_type::ethereum]);
      son1_start_votes = son1_end_votes;
      son2_obj = con.wallet_api_ptr->get_son("son2account");
      son2_end_votes = son2_obj.total_votes;
      BOOST_CHECK(son2_end_votes[sidechain_type::bitcoin] == son2_start_votes[sidechain_type::bitcoin]);
      BOOST_CHECK(son2_end_votes[sidechain_type::hive] == son2_start_votes[sidechain_type::hive]);
      BOOST_CHECK(son2_end_votes[sidechain_type::ethereum] == son2_start_votes[sidechain_type::ethereum]);
      son2_start_votes = son2_end_votes;

      //! Check votes of nathan
      nathan_votes_for_son = con.wallet_api_ptr->get_votes("nathan").votes_for_sons;
      BOOST_REQUIRE(!nathan_votes_for_son);

      // Try to accept and reject empty lists
      accepted.clear();
      rejected.clear();
      BOOST_REQUIRE_THROW(update_votes_tx = con.wallet_api_ptr->update_son_votes("nathan", accepted, rejected,
                                                                                 sidechain_type::bitcoin, 1, true), fc::exception);
      BOOST_REQUIRE_THROW(update_votes_tx = con.wallet_api_ptr->update_son_votes("nathan", accepted, rejected,
                                                                                 sidechain_type::hive, 1, true), fc::exception);
      BOOST_REQUIRE_THROW(update_votes_tx = con.wallet_api_ptr->update_son_votes("nathan", accepted, rejected,
                                                                                 sidechain_type::ethereum, 1, true), fc::exception);
      BOOST_CHECK(generate_maintenance_block());

      // Verify the votes
      son1_obj = con.wallet_api_ptr->get_son("son1account");
      son1_end_votes = son1_obj.total_votes;
      BOOST_CHECK(son1_end_votes[sidechain_type::bitcoin] == son1_start_votes[sidechain_type::bitcoin]);
      BOOST_CHECK(son1_end_votes[sidechain_type::hive] == son1_start_votes[sidechain_type::hive]);
      BOOST_CHECK(son1_end_votes[sidechain_type::bitcoin] == son1_start_votes[sidechain_type::bitcoin]);
      son1_start_votes = son1_end_votes;
      son2_obj = con.wallet_api_ptr->get_son("son2account");
      son2_end_votes = son2_obj.total_votes;
      BOOST_CHECK(son2_end_votes[sidechain_type::bitcoin] == son2_start_votes[sidechain_type::bitcoin]);
      BOOST_CHECK(son2_end_votes[sidechain_type::hive] == son2_start_votes[sidechain_type::hive]);
      BOOST_CHECK(son2_end_votes[sidechain_type::ethereum] == son2_start_votes[sidechain_type::ethereum]);
      son2_start_votes = son2_end_votes;

      //! Check votes of nathan
      nathan_votes_for_son = con.wallet_api_ptr->get_votes("nathan").votes_for_sons;
      BOOST_REQUIRE(!nathan_votes_for_son);

   } catch( fc::exception& e ) {
      BOOST_TEST_MESSAGE("SON cli wallet tests exception");
      edump((e.to_detail_string()));
      throw;
   }
   BOOST_TEST_MESSAGE("SON update_son_votes cli wallet tests end");
}

BOOST_AUTO_TEST_CASE( related_functions )
{
   BOOST_TEST_MESSAGE("SON-related functions cli wallet tests begin");
   try
   {
      global_property_object gpo = con.wallet_api_ptr->get_global_properties();
      BOOST_CHECK(gpo.active_sons.at(sidechain_type::bitcoin).size() == 0);
      BOOST_CHECK(gpo.active_sons.at(sidechain_type::hive).size() == 0);
      BOOST_CHECK(gpo.active_sons.at(sidechain_type::ethereum).size() == 0);

      flat_map<sidechain_type, string> sidechain_public_keys;

      son_test_helper sth(*this);

      sidechain_public_keys.clear();
      sidechain_public_keys[sidechain_type::bitcoin] = "bitcoin_address 1";
      sidechain_public_keys[sidechain_type::hive] = "hive account 1";
      sidechain_public_keys[sidechain_type::ethereum] = "ethereum address 1";
      sth.create_son("son1account", "http://son1", sidechain_public_keys);

      sidechain_public_keys.clear();
      sidechain_public_keys[sidechain_type::bitcoin] = "bitcoin_address 2";
      sidechain_public_keys[sidechain_type::hive] = "hive account 2";
      sidechain_public_keys[sidechain_type::ethereum] = "ethereum address 2";
      sth.create_son("son2account", "http://son2", sidechain_public_keys);

      gpo = con.wallet_api_ptr->get_global_properties();
      BOOST_CHECK(gpo.active_sons.at(sidechain_type::bitcoin).size() == 2);
      BOOST_CHECK(gpo.active_sons.at(sidechain_type::hive).size() == 2);
      BOOST_CHECK(gpo.active_sons.at(sidechain_type::ethereum).size() == 2);

   } catch( fc::exception& e ) {
      BOOST_TEST_MESSAGE("SON cli wallet tests exception");
      edump((e.to_detail_string()));
      throw;
   }
   BOOST_TEST_MESSAGE("SON-related functions cli wallet tests end");
}

BOOST_FIXTURE_TEST_CASE( get_active_sons, cli_fixture )
{
   BOOST_TEST_MESSAGE("SON cli wallet tests for get_active_sons begin");
   try
   {
      son_test_helper sth(*this);

      signed_transaction vote_tx;
      global_property_object gpo;

      gpo = con.wallet_api_ptr->get_global_properties();
      unsigned int son_number = 15; //gpo.parameters.maximum_son_count();

      flat_map<sidechain_type, string> sidechain_public_keys;
      BOOST_TEST_MESSAGE("Verify that there are no sons");
      BOOST_CHECK(0 == gpo.active_sons.at(sidechain_type::bitcoin).size());
      BOOST_CHECK(0 == gpo.active_sons.at(sidechain_type::hive).size());
      BOOST_CHECK(0 == gpo.active_sons.at(sidechain_type::ethereum).size());
      auto gpo_active_sons = gpo.active_sons;
      // create son accounts
      BOOST_TEST_MESSAGE("Create son accounts");      
      for(unsigned int i = 1; i < son_number + 1; i++)
      {
          sidechain_public_keys.clear();
          sidechain_public_keys[sidechain_type::bitcoin] = "bitcoin_address " + fc::to_pretty_string(i);
          sidechain_public_keys[sidechain_type::hive] = "hive account " + fc::to_pretty_string(i);
          sidechain_public_keys[sidechain_type::ethereum] = "ethereum address " + fc::to_pretty_string(i);
          sth.create_son("sonaccount" + fc::to_pretty_string(i),
                         "http://son" + fc::to_pretty_string(i),
                         sidechain_public_keys,
                         false);
          con.wallet_api_ptr->transfer(
              "nathan", "sonaccount" + fc::to_pretty_string(i), "1000", "1.3.0", "Here are some CORE tokens for your new account", true );
          con.wallet_api_ptr->create_vesting_balance("sonaccount" + fc::to_pretty_string(i), "500", "1.3.0", vesting_balance_type::gpos, true);
      }
      BOOST_CHECK(generate_maintenance_block());
      gpo = con.wallet_api_ptr->get_global_properties();
      BOOST_CHECK(gpo.active_sons != gpo_active_sons);

      gpo_active_sons = gpo.active_sons;
      auto cmd_active_sons = con.wallet_api_ptr->get_active_sons();
      BOOST_CHECK(gpo_active_sons == cmd_active_sons);

      BOOST_TEST_MESSAGE("Voting for SONs"); 
      for(unsigned int i = 5; i < son_number - 1; i++)
      {
         std::string name = "sonaccount" + fc::to_pretty_string(i);
         std::string name2 = "sonaccount" + fc::to_pretty_string(i + 1);
         vote_tx = con.wallet_api_ptr->vote_for_son(name, name2, sidechain_type::bitcoin, true, true);
         vote_tx = con.wallet_api_ptr->vote_for_son(name, name2, sidechain_type::hive, true, true);
         vote_tx = con.wallet_api_ptr->vote_for_son(name, name2, sidechain_type::ethereum, true, true);
      }
      BOOST_CHECK(generate_maintenance_block());
      gpo = con.wallet_api_ptr->get_global_properties();
      BOOST_CHECK(gpo.active_sons != gpo_active_sons);

      gpo_active_sons = gpo.active_sons;
      cmd_active_sons = con.wallet_api_ptr->get_active_sons();
      BOOST_CHECK(gpo_active_sons == cmd_active_sons);

      BOOST_TEST_MESSAGE("Unvoting for Specific SON");

      vote_tx = con.wallet_api_ptr->vote_for_son("sonaccount5", "sonaccount6", sidechain_type::bitcoin, false, true);
      vote_tx = con.wallet_api_ptr->vote_for_son("sonaccount5", "sonaccount6", sidechain_type::hive, false, true);
      vote_tx = con.wallet_api_ptr->vote_for_son("sonaccount5", "sonaccount6", sidechain_type::ethereum, false, true);

      BOOST_CHECK(generate_maintenance_block());
      gpo = con.wallet_api_ptr->get_global_properties();
      BOOST_CHECK(gpo.active_sons != gpo_active_sons);

      gpo_active_sons = gpo.active_sons;
      cmd_active_sons = con.wallet_api_ptr->get_active_sons();
      BOOST_CHECK(gpo_active_sons == cmd_active_sons);

      BOOST_TEST_MESSAGE("Unvoting for Specific SON in Specific Sidechain");

      vote_tx = con.wallet_api_ptr->vote_for_son("sonaccount6", "sonaccount7", sidechain_type::bitcoin, false, true);
      vote_tx = con.wallet_api_ptr->vote_for_son("sonaccount7", "sonaccount8", sidechain_type::hive, false, true);
      vote_tx = con.wallet_api_ptr->vote_for_son("sonaccount8", "sonaccount9", sidechain_type::ethereum, false, true);

      BOOST_CHECK(generate_maintenance_block());
      gpo = con.wallet_api_ptr->get_global_properties();
      BOOST_CHECK(gpo.active_sons != gpo_active_sons);

      gpo_active_sons = gpo.active_sons;
      cmd_active_sons = con.wallet_api_ptr->get_active_sons();
      BOOST_CHECK(gpo_active_sons == cmd_active_sons);

      BOOST_TEST_MESSAGE("Unvoting for all SONs");

      vote_tx = con.wallet_api_ptr->vote_for_son("sonaccount6", "sonaccount7", sidechain_type::bitcoin, true, true);
      vote_tx = con.wallet_api_ptr->vote_for_son("sonaccount7", "sonaccount8", sidechain_type::hive, true, true);
      vote_tx = con.wallet_api_ptr->vote_for_son("sonaccount8", "sonaccount9", sidechain_type::ethereum, true, true);
      BOOST_CHECK(generate_maintenance_block());

      for(unsigned int i = 6; i < son_number - 1; i++)
      {
         std::string name = "sonaccount" + fc::to_pretty_string(i);
         std::string name2 = "sonaccount" + fc::to_pretty_string(i + 1);
         vote_tx = con.wallet_api_ptr->vote_for_son(name, name2, sidechain_type::bitcoin, false, true);
         vote_tx = con.wallet_api_ptr->vote_for_son(name, name2, sidechain_type::hive, false, true);
         vote_tx = con.wallet_api_ptr->vote_for_son(name, name2, sidechain_type::ethereum, false, true);
      }
      BOOST_CHECK(generate_maintenance_block());
      gpo = con.wallet_api_ptr->get_global_properties();
      BOOST_CHECK(gpo.active_sons != gpo_active_sons);

      gpo_active_sons = gpo.active_sons;
      cmd_active_sons = con.wallet_api_ptr->get_active_sons();
      BOOST_CHECK(gpo_active_sons == cmd_active_sons);
      
      BOOST_CHECK_NO_THROW(con.wallet_api_ptr->get_active_sons());

   } catch( fc::exception& e ) {
      BOOST_TEST_MESSAGE("SON cli wallet tests exception");
      edump((e.to_detail_string()));
      throw;
   }
   BOOST_TEST_MESSAGE("SON cli wallet tests for get_active_sons end");
}

BOOST_FIXTURE_TEST_CASE( get_active_sons_by_sidechain, cli_fixture )
{
   BOOST_TEST_MESSAGE("SON cli wallet tests for get_active_sons_by_sidechain begin");
   try
   {
      son_test_helper sth(*this);

      signed_transaction vote_tx;
      global_property_object gpo;

      gpo = con.wallet_api_ptr->get_global_properties();
      unsigned int son_number = 15;

      flat_map<sidechain_type, string> sidechain_public_keys;
      BOOST_TEST_MESSAGE("Verify that there are no sons");
      BOOST_CHECK(0 == gpo.active_sons.at(sidechain_type::bitcoin).size());
      BOOST_CHECK(0 == gpo.active_sons.at(sidechain_type::hive).size());
      BOOST_CHECK(0 == gpo.active_sons.at(sidechain_type::ethereum).size());

      auto gpo_active_sons = gpo.active_sons;
      // create son accounts
      BOOST_TEST_MESSAGE("Create son accounts");      
      for(unsigned int i = 1; i < son_number + 1; i++)
      {
          sidechain_public_keys.clear();
          sidechain_public_keys[sidechain_type::bitcoin] = "bitcoin_address " + fc::to_pretty_string(i);
          sidechain_public_keys[sidechain_type::hive] = "hive account " + fc::to_pretty_string(i);
          sidechain_public_keys[sidechain_type::ethereum] = "ethereum address " + fc::to_pretty_string(i);
          sth.create_son("sonaccount" + fc::to_pretty_string(i),
                         "http://son" + fc::to_pretty_string(i),
                         sidechain_public_keys,
                         false);
          con.wallet_api_ptr->transfer(
              "nathan", "sonaccount" + fc::to_pretty_string(i), "1000", "1.3.0", "Here are some CORE tokens for your new account", true );
          con.wallet_api_ptr->create_vesting_balance("sonaccount" + fc::to_pretty_string(i), "500", "1.3.0", vesting_balance_type::gpos, true);
      }
      BOOST_CHECK(generate_maintenance_block());
      gpo = con.wallet_api_ptr->get_global_properties();
      BOOST_CHECK(gpo.active_sons != gpo_active_sons);

      gpo_active_sons = gpo.active_sons;
      auto cmd_active_sons = con.wallet_api_ptr->get_active_sons();
      BOOST_CHECK(gpo_active_sons == cmd_active_sons);

      auto cmd_active_sons2 = con.wallet_api_ptr->get_active_sons_by_sidechain((sidechain_type::bitcoin));
      BOOST_CHECK(gpo_active_sons.at(sidechain_type::bitcoin) == cmd_active_sons2);

      cmd_active_sons2 = con.wallet_api_ptr->get_active_sons_by_sidechain((sidechain_type::ethereum));
      BOOST_CHECK(gpo_active_sons.at(sidechain_type::ethereum) == cmd_active_sons2);

      cmd_active_sons2 = con.wallet_api_ptr->get_active_sons_by_sidechain((sidechain_type::hive));
      BOOST_CHECK(gpo_active_sons.at(sidechain_type::hive) == cmd_active_sons2);

      BOOST_TEST_MESSAGE("Voting for SONs");
      for(unsigned int i = 5; i < son_number - 1; i++)
      {
         std::string name = "sonaccount" + fc::to_pretty_string(i);
         std::string name2 = "sonaccount" + fc::to_pretty_string(i + 1);
         vote_tx = con.wallet_api_ptr->vote_for_son(name, name2, sidechain_type::bitcoin, true, true);
         vote_tx = con.wallet_api_ptr->vote_for_son(name, name2, sidechain_type::hive, true, true);
         vote_tx = con.wallet_api_ptr->vote_for_son(name, name2, sidechain_type::ethereum, true, true);
      }
      BOOST_CHECK(generate_maintenance_block());
      gpo = con.wallet_api_ptr->get_global_properties();
      BOOST_CHECK(gpo.active_sons != gpo_active_sons);

      gpo_active_sons = gpo.active_sons;
      cmd_active_sons = con.wallet_api_ptr->get_active_sons();
      BOOST_CHECK(gpo_active_sons == cmd_active_sons);

      cmd_active_sons2 = con.wallet_api_ptr->get_active_sons_by_sidechain((sidechain_type::bitcoin));
      BOOST_CHECK(gpo_active_sons.at(sidechain_type::bitcoin) == cmd_active_sons2);

      cmd_active_sons2 = con.wallet_api_ptr->get_active_sons_by_sidechain((sidechain_type::ethereum));
      BOOST_CHECK(gpo_active_sons.at(sidechain_type::ethereum) == cmd_active_sons2);

      cmd_active_sons2 = con.wallet_api_ptr->get_active_sons_by_sidechain((sidechain_type::hive));
      BOOST_CHECK(gpo_active_sons.at(sidechain_type::hive) == cmd_active_sons2);

      BOOST_TEST_MESSAGE("Unvoting for Specific SON");

      vote_tx = con.wallet_api_ptr->vote_for_son("sonaccount5", "sonaccount6", sidechain_type::bitcoin, false, true);
      vote_tx = con.wallet_api_ptr->vote_for_son("sonaccount5", "sonaccount6", sidechain_type::hive, false, true);
      vote_tx = con.wallet_api_ptr->vote_for_son("sonaccount5", "sonaccount6", sidechain_type::ethereum, false, true);
      BOOST_CHECK(generate_maintenance_block());
      gpo = con.wallet_api_ptr->get_global_properties();
      BOOST_CHECK(gpo.active_sons != gpo_active_sons);

      gpo_active_sons = gpo.active_sons;
      cmd_active_sons = con.wallet_api_ptr->get_active_sons();
      BOOST_CHECK(gpo_active_sons == cmd_active_sons);

      cmd_active_sons2 = con.wallet_api_ptr->get_active_sons_by_sidechain((sidechain_type::bitcoin));
      BOOST_CHECK(gpo_active_sons.at(sidechain_type::bitcoin) == cmd_active_sons2);

      cmd_active_sons2 = con.wallet_api_ptr->get_active_sons_by_sidechain((sidechain_type::ethereum));
      BOOST_CHECK(gpo_active_sons.at(sidechain_type::ethereum) == cmd_active_sons2);

      cmd_active_sons2 = con.wallet_api_ptr->get_active_sons_by_sidechain((sidechain_type::hive));
      BOOST_CHECK(gpo_active_sons.at(sidechain_type::hive) == cmd_active_sons2);

      BOOST_TEST_MESSAGE("Unvoting for Specific SON in Specific Sidechain");

      vote_tx = con.wallet_api_ptr->vote_for_son("sonaccount6", "sonaccount7", sidechain_type::bitcoin, false, true);
      vote_tx = con.wallet_api_ptr->vote_for_son("sonaccount7", "sonaccount8", sidechain_type::hive, false, true);
      vote_tx = con.wallet_api_ptr->vote_for_son("sonaccount8", "sonaccount9", sidechain_type::ethereum, false, true);
      BOOST_CHECK(generate_maintenance_block());
      gpo = con.wallet_api_ptr->get_global_properties();
      BOOST_CHECK(gpo.active_sons != gpo_active_sons);

      gpo_active_sons = gpo.active_sons;
      cmd_active_sons = con.wallet_api_ptr->get_active_sons();
      BOOST_CHECK(gpo_active_sons == cmd_active_sons);

      cmd_active_sons2 = con.wallet_api_ptr->get_active_sons_by_sidechain((sidechain_type::bitcoin));
      BOOST_CHECK(gpo_active_sons.at(sidechain_type::bitcoin) == cmd_active_sons2);

      cmd_active_sons2 = con.wallet_api_ptr->get_active_sons_by_sidechain((sidechain_type::ethereum));
      BOOST_CHECK(gpo_active_sons.at(sidechain_type::ethereum) == cmd_active_sons2);

      cmd_active_sons2 = con.wallet_api_ptr->get_active_sons_by_sidechain((sidechain_type::hive));
      BOOST_CHECK(gpo_active_sons.at(sidechain_type::hive) == cmd_active_sons2);

      BOOST_TEST_MESSAGE("Unvoting for all SONs");

      vote_tx = con.wallet_api_ptr->vote_for_son("sonaccount6", "sonaccount7", sidechain_type::bitcoin, true, true);
      vote_tx = con.wallet_api_ptr->vote_for_son("sonaccount7", "sonaccount8", sidechain_type::hive, true, true);
      vote_tx = con.wallet_api_ptr->vote_for_son("sonaccount8", "sonaccount9", sidechain_type::ethereum, true, true);
      BOOST_CHECK(generate_maintenance_block());

      for(unsigned int i = 6; i < son_number - 1; i++)
      {
         std::string name = "sonaccount" + fc::to_pretty_string(i);
         std::string name2 = "sonaccount" + fc::to_pretty_string(i + 1);
         if(i == 6)
         {
            vote_tx = con.wallet_api_ptr->vote_for_son(name, name2, sidechain_type::hive, false, true);
            vote_tx = con.wallet_api_ptr->vote_for_son(name, name2, sidechain_type::ethereum, false, true);
         }
         else if(i == 7)
         {
            vote_tx = con.wallet_api_ptr->vote_for_son(name, name2, sidechain_type::bitcoin, false, true);
            vote_tx = con.wallet_api_ptr->vote_for_son(name, name2, sidechain_type::ethereum, false, true);
         }
         else if(i == 8)
         {
            vote_tx = con.wallet_api_ptr->vote_for_son(name, name2, sidechain_type::bitcoin, false, true);
            vote_tx = con.wallet_api_ptr->vote_for_son(name, name2, sidechain_type::hive, false, true);
         }
         else{
            vote_tx = con.wallet_api_ptr->vote_for_son(name, name2, sidechain_type::bitcoin, false, true);
            vote_tx = con.wallet_api_ptr->vote_for_son(name, name2, sidechain_type::hive, false, true);
            vote_tx = con.wallet_api_ptr->vote_for_son(name, name2, sidechain_type::ethereum, false, true);            
         }
      }

      BOOST_CHECK(generate_maintenance_block());
      gpo = con.wallet_api_ptr->get_global_properties();
      BOOST_CHECK(gpo.active_sons != gpo_active_sons);

      gpo_active_sons = gpo.active_sons;
      cmd_active_sons = con.wallet_api_ptr->get_active_sons();
      BOOST_CHECK(gpo_active_sons == cmd_active_sons);

      cmd_active_sons2 = con.wallet_api_ptr->get_active_sons_by_sidechain((sidechain_type::bitcoin));
      BOOST_CHECK(gpo_active_sons.at(sidechain_type::bitcoin) == cmd_active_sons2);

      cmd_active_sons2 = con.wallet_api_ptr->get_active_sons_by_sidechain((sidechain_type::ethereum));
      BOOST_CHECK(gpo_active_sons.at(sidechain_type::ethereum) == cmd_active_sons2);

      cmd_active_sons2 = con.wallet_api_ptr->get_active_sons_by_sidechain((sidechain_type::hive));
      BOOST_CHECK(gpo_active_sons.at(sidechain_type::hive) == cmd_active_sons2);

      BOOST_CHECK_NO_THROW(con.wallet_api_ptr->get_active_sons_by_sidechain(sidechain_type::bitcoin));
      BOOST_CHECK_NO_THROW(con.wallet_api_ptr->get_active_sons_by_sidechain(sidechain_type::hive));
      BOOST_CHECK_NO_THROW(con.wallet_api_ptr->get_active_sons_by_sidechain(sidechain_type::ethereum));

   } catch( fc::exception& e ) {
      BOOST_TEST_MESSAGE("SON cli wallet tests exception");
      edump((e.to_detail_string()));
      throw;
   }
   BOOST_TEST_MESSAGE("SON cli wallet tests for get_active_sons_by_sidechain end");
}

BOOST_FIXTURE_TEST_CASE( get_son_network_status, cli_fixture )
{
   BOOST_TEST_MESSAGE("SON get_son_network_status cli wallet tests begin");
   try
   {
      son_test_helper sth(*this);

      auto db = app1->chain_database();
      signed_transaction vote_tx;

      global_property_object gpo;
      gpo = con.wallet_api_ptr->get_global_properties();
      unsigned int son_number = gpo.parameters.maximum_son_count();
      BOOST_TEST_MESSAGE("son_number"<<son_number);
      flat_map<sidechain_type, string> sidechain_public_keys;

      // create son accounts
      for(unsigned int i = 1; i < son_number + 1; i++)
      {
          sidechain_public_keys.clear();
          sidechain_public_keys[sidechain_type::bitcoin] = "bitcoin_address " + fc::to_pretty_string(i);
          sidechain_public_keys[sidechain_type::hive] = "hive account " + fc::to_pretty_string(i);
          sidechain_public_keys[sidechain_type::ethereum] = "ethereum address " + fc::to_pretty_string(i);
          sth.create_son("sonaccount" + fc::to_pretty_string(i),
                         "http://son" + fc::to_pretty_string(i),
                         sidechain_public_keys,
                         false);
          con.wallet_api_ptr->transfer(
            "nathan", "sonaccount" + fc::to_pretty_string(i), "1000", "1.3.0", "Here are some CORE tokens for your new account", true );
          con.wallet_api_ptr->create_vesting_balance("sonaccount" + fc::to_pretty_string(i), "500", "1.3.0", vesting_balance_type::gpos, true);

      }
      BOOST_CHECK(generate_maintenance_block());

      auto network_status_obj = con.wallet_api_ptr->get_son_network_status();

      for(map<sidechain_type, map<son_id_type, string>>::iterator outer_iter=network_status_obj.begin(); outer_iter!=network_status_obj.end(); ++outer_iter) 
      {
         for(map<son_id_type, string>::iterator inner_iter=outer_iter->second.begin(); inner_iter!=outer_iter->second.end(); ++inner_iter) 
         {
            BOOST_TEST_MESSAGE("status: "<< inner_iter->second);
            BOOST_CHECK(inner_iter->second == "No heartbeats sent");
         }
      }

      BOOST_TEST_MESSAGE("Voting for SONs");
      for(unsigned int i = 1; i < son_number-1; i++)
      {
          std::string name = "sonaccount" + fc::to_pretty_string(i);
          std::string name2 = "sonaccount" + fc::to_pretty_string(i + 2);
          vote_tx = con.wallet_api_ptr->vote_for_son(name, name2, sidechain_type::bitcoin, true, true);
          vote_tx = con.wallet_api_ptr->vote_for_son(name, name2, sidechain_type::hive, true, true);
          vote_tx = con.wallet_api_ptr->vote_for_son(name, name2, sidechain_type::ethereum, true, true);
      }
      BOOST_CHECK(generate_maintenance_block());
      gpo = con.wallet_api_ptr->get_global_properties();
      auto gpo_active_sons = gpo.active_sons;
      auto cmd_active_sons = con.wallet_api_ptr->get_active_sons();
      BOOST_CHECK(gpo_active_sons == cmd_active_sons);

      BOOST_TEST_MESSAGE("Sending Heartbeat for sonaccount3");
      son_object son_obj1 = con.wallet_api_ptr->get_son("sonaccount3");
      signed_transaction trx1;
      son_heartbeat_operation op1;
      op1.owner_account = son_obj1.son_account;
      op1.son_id = son_obj1.id;
      op1.ts = db->head_block_time()+fc::seconds(2*db->block_interval());
      trx1.operations.push_back(op1);
      con.wallet_api_ptr->sign_transaction(trx1, true);

      generate_blocks(50);

      BOOST_TEST_MESSAGE("Checking Network Status");
      network_status_obj = con.wallet_api_ptr->get_son_network_status();
      for(map<sidechain_type, map<son_id_type, string>>::iterator outer_iter=network_status_obj.begin(); outer_iter!=network_status_obj.end(); ++outer_iter) 
      {
         for(map<son_id_type, string>::iterator inner_iter=outer_iter->second.begin(); inner_iter!=outer_iter->second.end(); ++inner_iter) 
         {
            if((inner_iter->first == gpo.active_sons.at(sidechain_type::bitcoin).at(0).son_id) &&
               (inner_iter->first == gpo.active_sons.at(sidechain_type::hive).at(0).son_id) &&
               (inner_iter->first == gpo.active_sons.at(sidechain_type::ethereum).at(0).son_id))
            {
               BOOST_TEST_MESSAGE("status: "<< inner_iter->second);
               BOOST_CHECK(inner_iter->second == "OK, regular SON heartbeat");
            }
            else{
               BOOST_TEST_MESSAGE("status: "<< inner_iter->second);
               BOOST_CHECK(inner_iter->second == "No heartbeats sent");
            }
         }
      }

      BOOST_TEST_MESSAGE("Sending Heartbeat for sonaccount4");

      son_object son_obj2 = con.wallet_api_ptr->get_son("sonaccount4");
      signed_transaction trx2;
      son_heartbeat_operation op2;
      op2.owner_account = son_obj2.son_account;
      op2.son_id = son_obj2.id;
      op2.ts = db->head_block_time()+fc::seconds(2*db->block_interval());
      trx2.operations.push_back(op2);
      con.wallet_api_ptr->sign_transaction(trx2, true);

      generate_blocks(50);

      BOOST_TEST_MESSAGE("Checking Network Status");
      network_status_obj = con.wallet_api_ptr->get_son_network_status();
      for(map<sidechain_type, map<son_id_type, string>>::iterator outer_iter=network_status_obj.begin(); outer_iter!=network_status_obj.end(); ++outer_iter) 
      {
         for(map<son_id_type, string>::iterator inner_iter=outer_iter->second.begin(); inner_iter!=outer_iter->second.end(); ++inner_iter) 
         {
            if((inner_iter->first == gpo.active_sons.at(sidechain_type::bitcoin).at(0).son_id) &&
               (inner_iter->first == gpo.active_sons.at(sidechain_type::hive).at(0).son_id) &&
               (inner_iter->first == gpo.active_sons.at(sidechain_type::ethereum).at(0).son_id))
            {
               BOOST_TEST_MESSAGE("status: "<< inner_iter->second);
               BOOST_CHECK(inner_iter->second == "OK, irregular SON heartbeat, but not triggering SON down proposal");
            }
            else if((inner_iter->first == gpo.active_sons.at(sidechain_type::bitcoin).at(1).son_id) &&
               (inner_iter->first == gpo.active_sons.at(sidechain_type::hive).at(1).son_id) &&
               (inner_iter->first == gpo.active_sons.at(sidechain_type::ethereum).at(1).son_id))
            {
               BOOST_TEST_MESSAGE("status: "<< inner_iter->second);
               BOOST_CHECK(inner_iter->second == "OK, regular SON heartbeat");               
            }
            else{
               BOOST_TEST_MESSAGE("status: "<< inner_iter->second);
               BOOST_CHECK(inner_iter->second == "No heartbeats sent");
            }
         }
      }

      generate_blocks(db->head_block_time() + gpo.parameters.son_heartbeat_frequency(), false);
      BOOST_TEST_MESSAGE("Checking Network Status");
      network_status_obj = con.wallet_api_ptr->get_son_network_status();
      for(map<sidechain_type, map<son_id_type, string>>::iterator outer_iter=network_status_obj.begin(); outer_iter!=network_status_obj.end(); ++outer_iter) 
      {
         for(map<son_id_type, string>::iterator inner_iter=outer_iter->second.begin(); inner_iter!=outer_iter->second.end(); ++inner_iter) 
         {
            if((inner_iter->first == gpo.active_sons.at(sidechain_type::bitcoin).at(0).son_id) &&
               (inner_iter->first == gpo.active_sons.at(sidechain_type::hive).at(0).son_id) &&
               (inner_iter->first == gpo.active_sons.at(sidechain_type::ethereum).at(0).son_id))
            {
               BOOST_TEST_MESSAGE("status: "<< inner_iter->second);
               BOOST_CHECK(inner_iter->second == "NOT OK, irregular SON heartbeat, triggering SON down proposal");
            }
            else if((inner_iter->first == gpo.active_sons.at(sidechain_type::bitcoin).at(1).son_id) &&
               (inner_iter->first == gpo.active_sons.at(sidechain_type::hive).at(1).son_id) &&
               (inner_iter->first == gpo.active_sons.at(sidechain_type::ethereum).at(1).son_id))
            {
               BOOST_TEST_MESSAGE("status: "<< inner_iter->second);
               BOOST_CHECK(inner_iter->second == "OK, irregular SON heartbeat, but not triggering SON down proposal");               
            }
            else{
               BOOST_TEST_MESSAGE("status: "<< inner_iter->second);
               BOOST_CHECK(inner_iter->second == "No heartbeats sent");
            }
         }
      }

      generate_blocks(db->head_block_time() + gpo.parameters.son_heartbeat_frequency() + gpo.parameters.son_down_time(), false);
      BOOST_TEST_MESSAGE("Checking Network Status");

      network_status_obj = con.wallet_api_ptr->get_son_network_status();
      for(map<sidechain_type, map<son_id_type, string>>::iterator outer_iter=network_status_obj.begin(); outer_iter!=network_status_obj.end(); ++outer_iter) 
      {
         for(map<son_id_type, string>::iterator inner_iter=outer_iter->second.begin(); inner_iter!=outer_iter->second.end(); ++inner_iter) 
         {
            if((inner_iter->first == gpo.active_sons.at(sidechain_type::bitcoin).at(0).son_id) &&
               (inner_iter->first == gpo.active_sons.at(sidechain_type::hive).at(0).son_id) &&
               (inner_iter->first == gpo.active_sons.at(sidechain_type::ethereum).at(0).son_id))
            {
               BOOST_TEST_MESSAGE("status: "<< inner_iter->second);
               BOOST_CHECK(inner_iter->second == "NOT OK, irregular SON heartbeat, triggering SON down proposal");
            }
            else if((inner_iter->first == gpo.active_sons.at(sidechain_type::bitcoin).at(1).son_id) &&
               (inner_iter->first == gpo.active_sons.at(sidechain_type::hive).at(1).son_id) &&
               (inner_iter->first == gpo.active_sons.at(sidechain_type::ethereum).at(1).son_id))
            {
               BOOST_TEST_MESSAGE("status: "<< inner_iter->second);
               BOOST_CHECK(inner_iter->second == "NOT OK, irregular SON heartbeat, triggering SON down proposal");
            }
            else{
               BOOST_TEST_MESSAGE("status: "<< inner_iter->second);
               BOOST_CHECK(inner_iter->second == "No heartbeats sent");
            }
         }
      }

   } catch( fc::exception& e ) {
      BOOST_TEST_MESSAGE("SON cli wallet tests exception");
      edump((e.to_detail_string()));
      throw;
   }
   BOOST_TEST_MESSAGE("SON get_son_network_status cli wallet tests end");
}

BOOST_FIXTURE_TEST_CASE( get_son_network_status_by_sidechain, cli_fixture )
{
   BOOST_TEST_MESSAGE("SON get_son_network_status_by_sidechain cli wallet tests begin");
   try
   {
      son_test_helper sth(*this);
      signed_transaction vote_tx;
      auto db = app1->chain_database();

      global_property_object gpo;
      gpo = con.wallet_api_ptr->get_global_properties();
      unsigned int son_number = gpo.parameters.maximum_son_count();
      BOOST_TEST_MESSAGE("son_number"<<son_number);
      flat_map<sidechain_type, string> sidechain_public_keys;

      // create son accounts
      for(unsigned int i = 1; i < son_number + 1; i++)
      {
          sidechain_public_keys.clear();
          sidechain_public_keys[sidechain_type::bitcoin] = "bitcoin_address " + fc::to_pretty_string(i);
          sidechain_public_keys[sidechain_type::hive] = "hive account " + fc::to_pretty_string(i);
          sidechain_public_keys[sidechain_type::ethereum] = "ethereum address " + fc::to_pretty_string(i);
          sth.create_son("sonaccount" + fc::to_pretty_string(i),
                         "http://son" + fc::to_pretty_string(i),
                         sidechain_public_keys,
                         false);
          con.wallet_api_ptr->transfer(
            "nathan", "sonaccount" + fc::to_pretty_string(i), "1000", "1.3.0", "Here are some CORE tokens for your new account", true );
          con.wallet_api_ptr->create_vesting_balance("sonaccount" + fc::to_pretty_string(i), "500", "1.3.0", vesting_balance_type::gpos, true);

      }

      // Check Network Status Before sending Heartbeats
      BOOST_CHECK(generate_maintenance_block());
      for(sidechain_type sidechain : all_sidechain_types)
      {
         auto network_status_obj = con.wallet_api_ptr->get_son_network_status_by_sidechain(sidechain);
         for(map<son_id_type, string>::iterator iter=network_status_obj.begin(); iter!=network_status_obj.end(); ++iter) 
         {
            BOOST_TEST_MESSAGE("status: "<< iter->second);
            BOOST_CHECK(iter->second == "No heartbeats sent");
         }
      }

      BOOST_TEST_MESSAGE("Voting for SONs");
      for(unsigned int i = 1; i < son_number-1; i++)
      {
          std::string name = "sonaccount" + fc::to_pretty_string(i);
          std::string name2 = "sonaccount" + fc::to_pretty_string(i + 2);
          vote_tx = con.wallet_api_ptr->vote_for_son(name, name2, sidechain_type::bitcoin, true, true);
          vote_tx = con.wallet_api_ptr->vote_for_son(name, name2, sidechain_type::hive, true, true);
          vote_tx = con.wallet_api_ptr->vote_for_son(name, name2, sidechain_type::ethereum, true, true);
      }
      BOOST_CHECK(generate_maintenance_block());
      gpo = con.wallet_api_ptr->get_global_properties();

      auto gpo_active_sons = gpo.active_sons;
      auto cmd_active_sons = con.wallet_api_ptr->get_active_sons();
      BOOST_CHECK(gpo_active_sons == cmd_active_sons);

      auto cmd_active_sons2 = con.wallet_api_ptr->get_active_sons_by_sidechain((sidechain_type::bitcoin));
      BOOST_CHECK(gpo_active_sons.at(sidechain_type::bitcoin) == cmd_active_sons2);

      cmd_active_sons2 = con.wallet_api_ptr->get_active_sons_by_sidechain((sidechain_type::ethereum));
      BOOST_CHECK(gpo_active_sons.at(sidechain_type::ethereum) == cmd_active_sons2);

      cmd_active_sons2 = con.wallet_api_ptr->get_active_sons_by_sidechain((sidechain_type::hive));
      BOOST_CHECK(gpo_active_sons.at(sidechain_type::hive) == cmd_active_sons2);
        
      BOOST_TEST_MESSAGE("Sending Heartbeat for sonaccount3");
      son_object son_obj1 = con.wallet_api_ptr->get_son("sonaccount3");
      signed_transaction trx1;
      son_heartbeat_operation op1;
      op1.owner_account = son_obj1.son_account;
      op1.son_id = son_obj1.id;
      op1.ts = db->head_block_time()+fc::seconds(2*db->block_interval());
      trx1.operations.push_back(op1);
      con.wallet_api_ptr->sign_transaction(trx1, true);

      generate_blocks(50);

      BOOST_TEST_MESSAGE("Checking Network Status");      
      for(sidechain_type sidechain : all_sidechain_types)
      {
         auto network_status_obj = con.wallet_api_ptr->get_son_network_status_by_sidechain(sidechain);
         for(map<son_id_type, string>::iterator iter=network_status_obj.begin(); iter!=network_status_obj.end(); ++iter) 
         {
            if((iter->first == gpo.active_sons.at(sidechain_type::bitcoin).at(0).son_id) &&
               (iter->first == gpo.active_sons.at(sidechain_type::hive).at(0).son_id) &&
               (iter->first == gpo.active_sons.at(sidechain_type::ethereum).at(0).son_id))
            {
               BOOST_TEST_MESSAGE("status: "<< iter->second);
               BOOST_CHECK(iter->second == "OK, regular SON heartbeat");
            }
            else{
               BOOST_TEST_MESSAGE("status: "<< iter->second);
               BOOST_CHECK(iter->second == "No heartbeats sent");
            }

         }
      }

      BOOST_TEST_MESSAGE("Sending Heartbeat for sonaccount4");
      son_object son_obj2 = con.wallet_api_ptr->get_son("sonaccount4");
      signed_transaction trx2;
      son_heartbeat_operation op2;
      op2.owner_account = son_obj2.son_account;
      op2.son_id = son_obj2.id;
      op2.ts = db->head_block_time()+fc::seconds(2*db->block_interval());
      trx2.operations.push_back(op2);
      con.wallet_api_ptr->sign_transaction(trx2, true);

      generate_blocks(50);            
      for(sidechain_type sidechain : all_sidechain_types)
      {
         auto network_status_obj = con.wallet_api_ptr->get_son_network_status_by_sidechain(sidechain);
         for(map<son_id_type, string>::iterator iter=network_status_obj.begin(); iter!=network_status_obj.end(); ++iter) 
         {
           if((iter->first == gpo.active_sons.at(sidechain_type::bitcoin).at(0).son_id) &&
               (iter->first == gpo.active_sons.at(sidechain_type::hive).at(0).son_id) &&
               (iter->first == gpo.active_sons.at(sidechain_type::ethereum).at(0).son_id))
            {
               BOOST_TEST_MESSAGE("status: "<< iter->second);
               BOOST_CHECK(iter->second == "OK, irregular SON heartbeat, but not triggering SON down proposal");
            }
            else if((iter->first == gpo.active_sons.at(sidechain_type::bitcoin).at(1).son_id) &&
               (iter->first == gpo.active_sons.at(sidechain_type::hive).at(1).son_id) &&
               (iter->first == gpo.active_sons.at(sidechain_type::ethereum).at(1).son_id))
            {
               BOOST_TEST_MESSAGE("status: "<< iter->second);
               BOOST_CHECK(iter->second == "OK, regular SON heartbeat");               
            }
            else{
               BOOST_TEST_MESSAGE("status: "<< iter->second);
               BOOST_CHECK(iter->second == "No heartbeats sent");
            }
         }
      }

      generate_blocks(db->head_block_time() + gpo.parameters.son_heartbeat_frequency(), false);
      BOOST_TEST_MESSAGE("Checking Network Status");
      for(sidechain_type sidechain : all_sidechain_types)
      {
         auto network_status_obj = con.wallet_api_ptr->get_son_network_status_by_sidechain(sidechain);
         for(map<son_id_type, string>::iterator iter=network_status_obj.begin(); iter!=network_status_obj.end(); ++iter) 
         {
            if((iter->first == gpo.active_sons.at(sidechain_type::bitcoin).at(0).son_id) &&
               (iter->first == gpo.active_sons.at(sidechain_type::hive).at(0).son_id) &&
               (iter->first == gpo.active_sons.at(sidechain_type::ethereum).at(0).son_id))
            {
               BOOST_TEST_MESSAGE("status: "<< iter->second);
               BOOST_CHECK(iter->second == "NOT OK, irregular SON heartbeat, triggering SON down proposal");
            }
            else if((iter->first == gpo.active_sons.at(sidechain_type::bitcoin).at(1).son_id) &&
               (iter->first == gpo.active_sons.at(sidechain_type::hive).at(1).son_id) &&
               (iter->first == gpo.active_sons.at(sidechain_type::ethereum).at(1).son_id))
            {
               BOOST_TEST_MESSAGE("status: "<< iter->second);
               BOOST_CHECK(iter->second == "OK, irregular SON heartbeat, but not triggering SON down proposal");               
            }
            else{
               BOOST_TEST_MESSAGE("status: "<< iter->second);
               BOOST_CHECK(iter->second == "No heartbeats sent");
            }
         }
      } 

      generate_blocks(db->head_block_time() + gpo.parameters.son_heartbeat_frequency() + gpo.parameters.son_down_time(), false);;
      BOOST_TEST_MESSAGE("Checking Network Status");
      for(sidechain_type sidechain : all_sidechain_types)
      {
         auto network_status_obj = con.wallet_api_ptr->get_son_network_status_by_sidechain(sidechain);
         for(map<son_id_type, string>::iterator iter=network_status_obj.begin(); iter!=network_status_obj.end(); ++iter) 
         {
            if((iter->first == gpo.active_sons.at(sidechain_type::bitcoin).at(0).son_id) &&
               (iter->first == gpo.active_sons.at(sidechain_type::hive).at(0).son_id) &&
               (iter->first == gpo.active_sons.at(sidechain_type::ethereum).at(0).son_id))
            {
               BOOST_TEST_MESSAGE("status: "<< iter->second);
               BOOST_CHECK(iter->second == "NOT OK, irregular SON heartbeat, triggering SON down proposal");
            }
            else if((iter->first == gpo.active_sons.at(sidechain_type::bitcoin).at(1).son_id) &&
               (iter->first == gpo.active_sons.at(sidechain_type::hive).at(1).son_id) &&
               (iter->first == gpo.active_sons.at(sidechain_type::ethereum).at(1).son_id))
            {
               BOOST_TEST_MESSAGE("status: "<< iter->second);
               BOOST_CHECK(iter->second == "NOT OK, irregular SON heartbeat, triggering SON down proposal");
            }
            else{
               BOOST_TEST_MESSAGE("status: "<< iter->second);
               BOOST_CHECK(iter->second == "No heartbeats sent");
            }
         }
      }      

   } catch( fc::exception& e ) {
      BOOST_TEST_MESSAGE("SON cli wallet tests exception");
      edump((e.to_detail_string()));
      throw;
   }
   BOOST_TEST_MESSAGE("SON get_son_network_status_by_sidechain cli wallet tests end");
}

BOOST_AUTO_TEST_CASE( maintenance_test )
{
   BOOST_TEST_MESSAGE("SON maintenance cli wallet tests begin");
   try
   {
      son_test_helper sth(*this);

      std::string name("sonaccount1");

      global_property_object gpo;
      gpo = con.wallet_api_ptr->get_global_properties();
      unsigned int son_number = gpo.parameters.maximum_son_count();

      flat_map<sidechain_type, string> sidechain_public_keys;

      // create son accounts
      for(unsigned int i = 0; i < son_number + 1; i++)
      {
          sidechain_public_keys.clear();
          sidechain_public_keys[sidechain_type::bitcoin] = "bitcoin_address " + fc::to_pretty_string(i);
          sidechain_public_keys[sidechain_type::hive] = "hive account " + fc::to_pretty_string(i);
          sidechain_public_keys[sidechain_type::ethereum] = "ethereum address " + fc::to_pretty_string(i);
          sth.create_son("sonaccount" + fc::to_pretty_string(i),
                         "http://son" + fc::to_pretty_string(i),
                         sidechain_public_keys,
                         false);
      }
      BOOST_CHECK(generate_maintenance_block());

      BOOST_TEST_MESSAGE("Voting for SONs");
      for(unsigned int i = 1; i < son_number + 1; i++)
      {
         con.wallet_api_ptr->transfer(
              "nathan", "sonaccount" + fc::to_pretty_string(i), "1000", "1.3.0", "Here are some CORE tokens for your new account", true );
         con.wallet_api_ptr->create_vesting_balance("sonaccount" + fc::to_pretty_string(i), "500", "1.3.0", vesting_balance_type::gpos, true);
         con.wallet_api_ptr->vote_for_son("sonaccount" + fc::to_pretty_string(i), name, sidechain_type::bitcoin, true, true);
         con.wallet_api_ptr->vote_for_son("sonaccount" + fc::to_pretty_string(i), name, sidechain_type::hive, true, true);
         con.wallet_api_ptr->vote_for_son("sonaccount" + fc::to_pretty_string(i), name, sidechain_type::ethereum, true, true);
      }
      BOOST_CHECK(generate_maintenance_block());

      son_object son_obj = con.wallet_api_ptr->get_son(name);
      BOOST_CHECK(son_obj.statuses.at(sidechain_type::bitcoin) == son_status::active);
      BOOST_CHECK(son_obj.statuses.at(sidechain_type::hive) == son_status::active);
      BOOST_CHECK(son_obj.statuses.at(sidechain_type::ethereum) == son_status::active);

      // put SON in maintenance mode
      con.wallet_api_ptr->request_son_maintenance(name, true);
      generate_block();

      // check SON is in request_maintenance
      son_obj = con.wallet_api_ptr->get_son(name);
      BOOST_CHECK(son_obj.statuses.at(sidechain_type::bitcoin) == son_status::request_maintenance);
      BOOST_CHECK(son_obj.statuses.at(sidechain_type::hive) == son_status::request_maintenance);
      BOOST_CHECK(son_obj.statuses.at(sidechain_type::ethereum) == son_status::request_maintenance);

      // restore SON activity
      con.wallet_api_ptr->cancel_request_son_maintenance(name, true);
      generate_block();

      // check SON is active
      son_obj = con.wallet_api_ptr->get_son(name);
      BOOST_CHECK(son_obj.statuses.at(sidechain_type::bitcoin) == son_status::active);
      BOOST_CHECK(son_obj.statuses.at(sidechain_type::hive) == son_status::active);
      BOOST_CHECK(son_obj.statuses.at(sidechain_type::ethereum) == son_status::active);

      // put SON in maintenance mode
      con.wallet_api_ptr->request_son_maintenance(name, true);
      generate_block();

      // check SON is in request_maintenance
      son_obj = con.wallet_api_ptr->get_son(name);
      BOOST_CHECK(son_obj.statuses.at(sidechain_type::bitcoin)  == son_status::request_maintenance);
      BOOST_CHECK(son_obj.statuses.at(sidechain_type::hive)  == son_status::request_maintenance);
      BOOST_CHECK(son_obj.statuses.at(sidechain_type::ethereum)  == son_status::request_maintenance);

      // process maintenance
      BOOST_CHECK(generate_maintenance_block());

      // check SON is in maintenance
      son_obj = con.wallet_api_ptr->get_son(name);
      BOOST_CHECK(son_obj.statuses.at(sidechain_type::bitcoin) == son_status::in_maintenance);
      BOOST_CHECK(son_obj.statuses.at(sidechain_type::hive) == son_status::in_maintenance);
      BOOST_CHECK(son_obj.statuses.at(sidechain_type::ethereum) == son_status::in_maintenance);

   } catch( fc::exception& e ) {
      BOOST_TEST_MESSAGE("SON cli wallet tests exception");
      edump((e.to_detail_string()));
      throw;
   }
   BOOST_TEST_MESSAGE("SON maintenance cli wallet tests end");
}

BOOST_AUTO_TEST_SUITE_END()

