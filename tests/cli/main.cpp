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

#include <fc/crypto/aes.hpp>

#include <graphene/utilities/tempdir.hpp>

#define BOOST_TEST_MODULE Test Application
#include <boost/test/included/unit_test.hpp>

///////////////////////////////
// Tests
///////////////////////////////

////////////////
// Start a server and connect using the same calls as the CLI
////////////////
BOOST_FIXTURE_TEST_SUITE( cli_common, cli_fixture )

BOOST_AUTO_TEST_CASE( cli_connect )
{
   BOOST_TEST_MESSAGE("Testing wallet connection.");
}

////////////////
// Start a server and connect using the same calls as the CLI
// Quit wallet and be sure that file was saved correctly
////////////////
BOOST_FIXTURE_TEST_CASE( cli_quit, cli_fixture )
{
   BOOST_TEST_MESSAGE("Testing wallet connection and quit command.");
   BOOST_CHECK_THROW( con.wallet_api_ptr->quit(), fc::canceled_exception );
}

BOOST_FIXTURE_TEST_CASE( upgrade_nathan_account, cli_fixture )
{
    init_nathan();
}

BOOST_AUTO_TEST_CASE( create_new_account )
{
   try
   {
      init_nathan();

      // create a new account
      graphene::wallet::brain_key_info bki = con.wallet_api_ptr->suggest_brain_key();
      BOOST_CHECK(!bki.brain_priv_key.empty());
      signed_transaction create_acct_tx = con.wallet_api_ptr->create_account_with_brain_key(
            bki.brain_priv_key, "jmjatlanta", "nathan", "nathan", true
      );
      // save the private key for this new account in the wallet file
      BOOST_CHECK(con.wallet_api_ptr->import_key("jmjatlanta", bki.wif_priv_key));
      con.wallet_api_ptr->save_wallet_file(con.wallet_filename);

      generate_block();
      fc::usleep( fc::seconds(1) );
      
      // attempt to give jmjatlanta some peerplays
      BOOST_TEST_MESSAGE("Transferring peerplays from Nathan to jmjatlanta");
      signed_transaction transfer_tx = con.wallet_api_ptr->transfer(
            "nathan", "jmjatlanta", "10000", "1.3.0", "Here are some CORE token for your new account", true
      );
   } catch( fc::exception& e ) {
      edump((e.to_detail_string()));
      throw;
   }
}

///////////////////////
// Start a server and connect using the same calls as the CLI
// Vote for two witnesses, and make sure they both stay there
// after a maintenance block
///////////////////////

// Todo: Removed by GPOS, refactor test.
/*
BOOST_FIXTURE_TEST_CASE( cli_vote_for_2_witnesses, cli_fixture )
{
   try
   {
      BOOST_TEST_MESSAGE("Cli Vote Test for 2 Witnesses");
      
      INVOKE(create_new_account);

      // get the details for init1
      witness_object init1_obj = con.wallet_api_ptr->get_witness("init1");
      int init1_start_votes = init1_obj.total_votes;
      // Vote for a witness
      signed_transaction vote_witness1_tx = con.wallet_api_ptr->vote_for_witness("jmjatlanta", "init1", true, true);

      // generate a block to get things started
      generate_block();
      // wait for a maintenance interval
      BOOST_CHECK(generate_maintenance_block());

      // Verify that the vote is there
      init1_obj = con.wallet_api_ptr->get_witness("init1");
      witness_object init2_obj = con.wallet_api_ptr->get_witness("init2");
      int init1_middle_votes = init1_obj.total_votes;
      BOOST_CHECK(init1_middle_votes > init1_start_votes);

      // Vote for a 2nd witness
      int init2_start_votes = init2_obj.total_votes;
      signed_transaction vote_witness2_tx = con.wallet_api_ptr->vote_for_witness("jmjatlanta", "init2", true, true);

      // send another block to trigger maintenance interval
      BOOST_CHECK(generate_maintenance_block());

      // Verify that both the first vote and the 2nd are there
      init2_obj = con.wallet_api_ptr->get_witness("init2");
      init1_obj = con.wallet_api_ptr->get_witness("init1");

      int init2_middle_votes = init2_obj.total_votes;
      BOOST_CHECK(init2_middle_votes > init2_start_votes);
      int init1_last_votes = init1_obj.total_votes;
      BOOST_CHECK(init1_last_votes > init1_start_votes);
   } catch( fc::exception& e ) {
      edump((e.to_detail_string()));
      throw;
   }
}
*/

BOOST_FIXTURE_TEST_CASE( cli_get_signed_transaction_signers, cli_fixture )
{
   try
   {
      INVOKE(upgrade_nathan_account);

      // register account and transfer funds
      const auto test_bki = con.wallet_api_ptr->suggest_brain_key();
      con.wallet_api_ptr->register_account(
         "test", test_bki.pub_key, test_bki.pub_key, "nathan", "nathan", 0, true
      );
      con.wallet_api_ptr->transfer("nathan", "test", "1000", "1.3.0", "", true);

      // import key and save wallet
      BOOST_CHECK(con.wallet_api_ptr->import_key("test", test_bki.wif_priv_key));
      con.wallet_api_ptr->save_wallet_file(con.wallet_filename);

      // create transaction and check expected result
      auto signed_trx = con.wallet_api_ptr->transfer("test", "nathan", "10", "1.3.0", "", true);

      const auto &test_acc = con.wallet_api_ptr->get_account("test");
      flat_set<public_key_type> expected_signers = {test_bki.pub_key};
      vector<vector<account_id_type> > expected_key_refs{{test_acc.id, test_acc.id}};

      auto signers = con.wallet_api_ptr->get_transaction_signers(signed_trx);
      BOOST_CHECK(signers == expected_signers);

      auto key_refs = con.wallet_api_ptr->get_key_references({test_bki.pub_key});
      BOOST_CHECK(key_refs == expected_key_refs);

   } catch( fc::exception& e ) {
      edump((e.to_detail_string()));
      throw;
   }
}

///////////////////////
// Check account history pagination
///////////////////////
BOOST_AUTO_TEST_CASE( account_history_pagination )
{
   try
   {
      INVOKE(create_new_account);

       // attempt to give jmjatlanta some peerplay
      BOOST_TEST_MESSAGE("Transferring peerplay from Nathan to jmjatlanta");
      for(int i = 1; i <= 199; i++)
      {
         signed_transaction transfer_tx = con.wallet_api_ptr->transfer("nathan", "jmjatlanta", std::to_string(i),
                                                "1.3.0", "Here are some CORE token for your new account", true);
      }

      generate_block();

       // now get account history and make sure everything is there (and no duplicates)
      std::vector<graphene::wallet::operation_detail> history = con.wallet_api_ptr->get_account_history("jmjatlanta", 300);
      BOOST_CHECK_EQUAL(201u, history.size() );

       std::set<object_id_type> operation_ids;

       for(auto& op : history)
      {
         if( operation_ids.find(op.op.id) != operation_ids.end() )
         {
            BOOST_FAIL("Duplicate found");
         }
         operation_ids.insert(op.op.id);
      }
   } catch( fc::exception& e ) {
      edump((e.to_detail_string()));
      throw;
   }
}

BOOST_FIXTURE_TEST_CASE( cli_get_available_transaction_signers, cli_fixture )
{
   try
   {
      INVOKE(upgrade_nathan_account);

      // register account
      const auto test_bki = con.wallet_api_ptr->suggest_brain_key();
      con.wallet_api_ptr->register_account(
         "test", test_bki.pub_key, test_bki.pub_key, "nathan", "nathan", 0, true
      );
      const auto &test_acc = con.wallet_api_ptr->get_account("test");

      // create and sign transaction
      signed_transaction trx;
      trx.operations = {transfer_operation()};

      // sign with test key
      const auto test_privkey = wif_to_key( test_bki.wif_priv_key );
      BOOST_REQUIRE( test_privkey );
      trx.sign( *test_privkey, con.wallet_data.chain_id );

      // sign with other keys
      const auto privkey_1 = fc::ecc::private_key::generate();
      trx.sign( privkey_1, con.wallet_data.chain_id );

      const auto privkey_2 = fc::ecc::private_key::generate();
      trx.sign( privkey_2, con.wallet_data.chain_id );

      // verify expected result
      flat_set<public_key_type> expected_signers = {test_bki.pub_key, 
                                                    privkey_1.get_public_key(), 
                                                    privkey_2.get_public_key()};

      auto signers = con.wallet_api_ptr->get_transaction_signers(trx);
      BOOST_CHECK(signers == expected_signers);

      // blockchain has no references to unknown accounts (privkey_1, privkey_2)
      // only test account available
      vector<vector<account_id_type> > expected_key_refs;
      expected_key_refs.push_back(vector<account_id_type>());
      expected_key_refs.push_back(vector<account_id_type>());
      expected_key_refs.push_back({test_acc.id, test_acc.id});

      auto key_refs = con.wallet_api_ptr->get_key_references({expected_signers.begin(), expected_signers.end()});
      std::sort(key_refs.begin(), key_refs.end());

      BOOST_CHECK(key_refs == expected_key_refs);

   } catch( fc::exception& e ) {
      edump((e.to_detail_string()));
      throw;
   }
}

BOOST_FIXTURE_TEST_CASE( cli_cant_get_signers_from_modified_transaction, cli_fixture )
{
   try
   {
      INVOKE(upgrade_nathan_account);

      // register account
      const auto test_bki = con.wallet_api_ptr->suggest_brain_key();
      con.wallet_api_ptr->register_account(
         "test", test_bki.pub_key, test_bki.pub_key, "nathan", "nathan", 0, true
      );

      // create and sign transaction
      signed_transaction trx;
      trx.operations = {transfer_operation()};

      // sign with test key
      const auto test_privkey = wif_to_key( test_bki.wif_priv_key );
      BOOST_REQUIRE( test_privkey );
      trx.sign( *test_privkey, con.wallet_data.chain_id );

      // modify transaction (MITM-attack)
      trx.operations.clear();

      // verify if transaction has no valid signature of test account 
      flat_set<public_key_type> expected_signers_of_valid_transaction = {test_bki.pub_key};
      auto signers = con.wallet_api_ptr->get_transaction_signers(trx);
      BOOST_CHECK(signers != expected_signers_of_valid_transaction);

   } catch( fc::exception& e ) {
      edump((e.to_detail_string()));
      throw;
   }
}

///////////////////
// Start a server and connect using the same calls as the CLI
// Set a voting proxy and be assured that it sticks
///////////////////
BOOST_FIXTURE_TEST_CASE( cli_set_voting_proxy, cli_fixture )
{
   try {
      INVOKE(create_new_account);

      // grab account for comparison
      account_object prior_voting_account = con.wallet_api_ptr->get_account("jmjatlanta");
      // set the voting proxy to nathan
      BOOST_TEST_MESSAGE("About to set voting proxy.");
      signed_transaction voting_tx = con.wallet_api_ptr->set_voting_proxy("jmjatlanta", "nathan", true);
      account_object after_voting_account = con.wallet_api_ptr->get_account("jmjatlanta");
      // see if it changed
      BOOST_CHECK(prior_voting_account.options.voting_account != after_voting_account.options.voting_account);
   } catch( fc::exception& e ) {
      edump((e.to_detail_string()));
      throw;
   }
}


///////////////////////
// Create a multi-sig account and verify that only when all signatures are
// signed, the transaction could be broadcast
///////////////////////
BOOST_AUTO_TEST_CASE( cli_multisig_transaction )
{
   using namespace graphene::chain;
   using namespace graphene::app;
   std::shared_ptr<graphene::app::application> app1;
   try {
      fc::temp_directory app_dir( graphene::utilities::temp_directory_path() );

      int server_port_number = 0;
      app1 = start_application(app_dir, server_port_number);

      // connect to the server
      client_connection con(app1, app_dir, server_port_number);

      BOOST_TEST_MESSAGE("Setting wallet password");
      con.wallet_api_ptr->set_password("supersecret");
      con.wallet_api_ptr->unlock("supersecret");

      // import Nathan account
      BOOST_TEST_MESSAGE("Importing nathan key");
      std::vector<std::string> nathan_keys{"5KQwrPbwdL6PhXujxW37FSSQZ1JiwsST4cqQzDeyXtP79zkvFD3"};
      BOOST_CHECK_EQUAL(nathan_keys[0], "5KQwrPbwdL6PhXujxW37FSSQZ1JiwsST4cqQzDeyXtP79zkvFD3");
      BOOST_CHECK(con.wallet_api_ptr->import_key("nathan", nathan_keys[0]));

      BOOST_TEST_MESSAGE("Importing nathan's balance");
      std::vector<signed_transaction> import_txs = con.wallet_api_ptr->import_balance("nathan", nathan_keys, true);
      account_object nathan_acct_before_upgrade = con.wallet_api_ptr->get_account("nathan");

      // upgrade nathan
      BOOST_TEST_MESSAGE("Upgrading Nathan to LTM");
      signed_transaction upgrade_tx = con.wallet_api_ptr->upgrade_account("nathan", true);
      account_object nathan_acct_after_upgrade = con.wallet_api_ptr->get_account("nathan");

      // verify that the upgrade was successful
      BOOST_CHECK_PREDICATE( std::not_equal_to<uint32_t>(), (nathan_acct_before_upgrade.membership_expiration_date.sec_since_epoch())(nathan_acct_after_upgrade.membership_expiration_date.sec_since_epoch()) );
      BOOST_CHECK(nathan_acct_after_upgrade.is_lifetime_member());

      // create a new multisig account
      graphene::wallet::brain_key_info bki1 = con.wallet_api_ptr->suggest_brain_key();
      graphene::wallet::brain_key_info bki2 = con.wallet_api_ptr->suggest_brain_key();
      graphene::wallet::brain_key_info bki3 = con.wallet_api_ptr->suggest_brain_key();
      graphene::wallet::brain_key_info bki4 = con.wallet_api_ptr->suggest_brain_key();
      BOOST_CHECK(!bki1.brain_priv_key.empty());
      BOOST_CHECK(!bki2.brain_priv_key.empty());
      BOOST_CHECK(!bki3.brain_priv_key.empty());
      BOOST_CHECK(!bki4.brain_priv_key.empty());

      signed_transaction create_multisig_acct_tx;
      account_create_operation account_create_op;

      account_create_op.referrer = nathan_acct_after_upgrade.id;
      account_create_op.referrer_percent = nathan_acct_after_upgrade.referrer_rewards_percentage;
      account_create_op.registrar = nathan_acct_after_upgrade.id;
      account_create_op.name = "cifer.test";
      account_create_op.owner = authority(1, bki1.pub_key, 1);
      account_create_op.active = authority(2, bki2.pub_key, 1, bki3.pub_key, 1);
      account_create_op.options.memo_key = bki4.pub_key;
      account_create_op.fee = asset(1000000);  // should be enough for creating account

      create_multisig_acct_tx.operations.push_back(account_create_op);
      con.wallet_api_ptr->sign_transaction(create_multisig_acct_tx, true);

      // attempt to give cifer.test some peerplays
      BOOST_TEST_MESSAGE("Transferring peerplays from Nathan to cifer.test");
      signed_transaction transfer_tx1 = con.wallet_api_ptr->transfer("nathan", "cifer.test", "10000", "1.3.0", "Here are some BTS for your new account", true);

      // transfer bts from cifer.test to nathan
      BOOST_TEST_MESSAGE("Transferring peerplays from cifer.test to nathan");
      auto dyn_props = app1->chain_database()->get_dynamic_global_properties();
      account_object cifer_test = con.wallet_api_ptr->get_account("cifer.test");

      // construct a transfer transaction
      signed_transaction transfer_tx2;
      transfer_operation xfer_op;
      xfer_op.from = cifer_test.id;
      xfer_op.to = nathan_acct_after_upgrade.id;
      xfer_op.amount = asset(100000000);
      xfer_op.fee = asset(3000000);  // should be enough for transfer
      transfer_tx2.operations.push_back(xfer_op);

      // case1: sign a transaction without TaPoS and expiration fields
      // expect: return a transaction with TaPoS and expiration filled
      transfer_tx2 =
         con.wallet_api_ptr->add_transaction_signature( transfer_tx2, false );
      BOOST_CHECK( ( transfer_tx2.ref_block_num != 0 &&
                     transfer_tx2.ref_block_prefix != 0 ) ||
                   ( transfer_tx2.expiration != fc::time_point_sec() ) );

      // case2: broadcast without signature
      // expect: exception with missing active authority
      BOOST_CHECK_THROW(con.wallet_api_ptr->broadcast_transaction(transfer_tx2), fc::exception);

      // case3:
      // import one of the private keys for this new account in the wallet file,
      // sign and broadcast with partial signatures
      //
      // expect: exception with missing active authority
      BOOST_CHECK(con.wallet_api_ptr->import_key("cifer.test", bki2.wif_priv_key));
      BOOST_CHECK_THROW(con.wallet_api_ptr->add_transaction_signature(transfer_tx2, true), fc::exception);

      // case4: sign again as signature exists
      // expect: num of signatures not increase
      // transfer_tx2 = con.wallet_api_ptr->add_transaction_signature(transfer_tx2, false);
      // BOOST_CHECK_EQUAL(transfer_tx2.signatures.size(), 1);

      // case5:
      // import another private key, sign and broadcast without full signatures
      //
      // expect: transaction broadcast successfully
      BOOST_CHECK(con.wallet_api_ptr->import_key("cifer.test", bki3.wif_priv_key));
      con.wallet_api_ptr->add_transaction_signature(transfer_tx2, true);
      auto balances = con.wallet_api_ptr->list_account_balances( "cifer.test" );
      for (auto b : balances) {
         if (b.asset_id == asset_id_type()) {
            BOOST_ASSERT(b == asset(900000000 - 3000000));
         }
      }

      // wait for everything to finish up
      fc::usleep(fc::seconds(1));
   } catch( fc::exception& e ) {
      edump((e.to_detail_string()));
      throw;
   }
   app1->shutdown();
}

graphene::wallet::plain_keys decrypt_keys( const std::string& password, const vector<char>& cipher_keys )
{
   auto pw = fc::sha512::hash( password.c_str(), password.size() );
   vector<char> decrypted = fc::aes_decrypt( pw, cipher_keys );
   return fc::raw::unpack<graphene::wallet::plain_keys>( decrypted );
}

BOOST_FIXTURE_TEST_CASE( saving_keys_wallet_test, cli_fixture )
{
   con.wallet_api_ptr->import_balance( "nathan", nathan_keys, true );
   con.wallet_api_ptr->upgrade_account( "nathan", true );
   std::string brain_key( "FICTIVE WEARY MINIBUS LENS HAWKIE MAIDISH MINTY GLYPH GYTE KNOT COCKSHY LENTIGO PROPS BIFORM KHUTBAH BRAZIL" );
   con.wallet_api_ptr->create_account_with_brain_key( brain_key, "account1", "nathan", "nathan", true );

   BOOST_CHECK_NO_THROW( con.wallet_api_ptr->transfer( "nathan", "account1", "9000", "1.3.0", "", true ) );

   std::string path( app_dir.path().generic_string() + "/wallet.json" );
   graphene::wallet::wallet_data wallet = fc::json::from_file( path ).as<graphene::wallet::wallet_data>( 2 * GRAPHENE_MAX_NESTED_OBJECTS );
   BOOST_CHECK( wallet.extra_keys.size() == 1 ); // nathan
   BOOST_CHECK( wallet.pending_account_registrations.size() == 1 ); // account1
   BOOST_CHECK( wallet.pending_account_registrations["account1"].size() == 2 ); // account1 active key + account1 memo key

   graphene::wallet::plain_keys pk = decrypt_keys( "supersecret", wallet.cipher_keys );
   BOOST_CHECK( pk.keys.size() == 1 ); // nathan key

   generate_block();
   fc::usleep( fc::seconds(1) );

   wallet = fc::json::from_file( path ).as<graphene::wallet::wallet_data>( 2 * GRAPHENE_MAX_NESTED_OBJECTS );
   BOOST_CHECK( wallet.extra_keys.size() == 2 ); // nathan + account1
   BOOST_CHECK( wallet.pending_account_registrations.empty() );
   BOOST_CHECK_NO_THROW( con.wallet_api_ptr->transfer( "account1", "nathan", "1000", "1.3.0", "", true ) );

   pk = decrypt_keys( "supersecret", wallet.cipher_keys );
   BOOST_CHECK( pk.keys.size() == 3 ); // nathan key + account1 active key + account1 memo key
}

BOOST_AUTO_TEST_SUITE_END()
