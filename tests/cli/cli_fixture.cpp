#include <graphene/app/plugin.hpp>
#include <graphene/app/api.hpp>

#include <graphene/utilities/tempdir.hpp>
#include <graphene/bookie/bookie_plugin.hpp>
#include <graphene/witness/witness.hpp>
#include <graphene/account_history/account_history_plugin.hpp>
#include <graphene/egenesis/egenesis.hpp>
#include <graphene/chain/config.hpp>
#include <graphene/chain/database.hpp>

#include <fc/crypto/base58.hpp>

#include <fc/crypto/aes.hpp>

#ifdef _WIN32
#ifndef _WIN32_WINNT
      #define _WIN32_WINNT 0x0501
   #endif
   #include <winsock2.h>
   #include <WS2tcpip.h>
#else
#include <sys/socket.h>
#include <netinet/ip.h>
#include <sys/types.h>
#endif
#include <thread>

#include <boost/filesystem/path.hpp>
#include <boost/test/unit_test.hpp>

#include "cli_fixture.hpp"

/*****
 * Global Initialization for Windows
 * ( sets up Winsock stuf )
 */
#ifdef _WIN32
int sockInit(void)
{
   WSADATA wsa_data;
   return WSAStartup(MAKEWORD(1,1), &wsa_data);
}
int sockQuit(void)
{
   return WSACleanup();
}
#endif

/*********************
 * Helper Methods
 *********************/

#include "../common/genesis_file_util.hpp"

//////
/// @brief attempt to find an available port on localhost
/// @returns an available port number, or -1 on error
/////
int get_available_port()
{
   struct sockaddr_in sin;
   int socket_fd = socket(AF_INET, SOCK_STREAM, 0);
   if (socket_fd == -1)
      return -1;
   sin.sin_family = AF_INET;
   sin.sin_port = 0;
   sin.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
   if (::bind(socket_fd, (struct sockaddr*)&sin, sizeof(struct sockaddr_in)) == -1)
      return -1;
   socklen_t len = sizeof(sin);
   if (getsockname(socket_fd, (struct sockaddr *)&sin, &len) == -1)
      return -1;
#ifdef _WIN32
   closesocket(socket_fd);
#else
   close(socket_fd);
#endif
   return ntohs(sin.sin_port);
}

///////////
/// @brief Start the application
/// @param app_dir the temporary directory to use
/// @param server_port_number to be filled with the rpc endpoint port number
/// @returns the application object
//////////
std::shared_ptr<graphene::app::application> start_application(fc::temp_directory& app_dir, int& server_port_number) {
   std::shared_ptr<graphene::app::application> app1(new graphene::app::application{});

   app1->register_plugin<graphene::bookie::bookie_plugin>();
   app1->register_plugin<graphene::account_history::account_history_plugin>();
   app1->register_plugin<graphene::market_history::market_history_plugin>();
   app1->register_plugin<graphene::witness_plugin::witness_plugin>();
   app1->startup_plugins();
   boost::program_options::variables_map cfg;
#ifdef _WIN32
   sockInit();
#endif
   server_port_number = get_available_port();
   cfg.emplace(
         "rpc-endpoint",
         boost::program_options::variable_value(string("127.0.0.1:" + std::to_string(server_port_number)), false)
   );
   cfg.emplace("genesis-json", boost::program_options::variable_value(create_genesis_file(app_dir), false));
   cfg.emplace("seed-nodes", boost::program_options::variable_value(string("[]"), false));
   cfg.emplace("plugins", boost::program_options::variable_value(string("bookie account_history market_history"), false));

   app1->initialize(app_dir.path(), cfg);

   app1->initialize_plugins(cfg);
   app1->startup_plugins();

   app1->startup();
   fc::usleep(fc::milliseconds(500));
   return app1;
}

client_connection::client_connection(
     std::shared_ptr<graphene::app::application> app,
     const fc::temp_directory& data_dir,
     const int server_port_number
)
{
  wallet_data.chain_id = app->chain_database()->get_chain_id();
  wallet_data.ws_server = "ws://127.0.0.1:" + std::to_string(server_port_number);
  wallet_data.ws_user = "";
  wallet_data.ws_password = "";
  websocket_connection  = websocket_client.connect( wallet_data.ws_server );

  api_connection = std::make_shared<fc::rpc::websocket_api_connection>(websocket_connection, GRAPHENE_MAX_NESTED_OBJECTS);

  remote_login_api = api_connection->get_remote_api< graphene::app::login_api >(1);
  BOOST_CHECK(remote_login_api->login( wallet_data.ws_user, wallet_data.ws_password ) );

  wallet_api_ptr = std::make_shared<graphene::wallet::wallet_api>(wallet_data, remote_login_api);
  wallet_filename = data_dir.path().generic_string() + "/wallet.json";
  wallet_api_ptr->set_wallet_filename(wallet_filename);

  wallet_api = fc::api<graphene::wallet::wallet_api>(wallet_api_ptr);

  wallet_cli = std::make_shared<fc::rpc::cli>(GRAPHENE_MAX_NESTED_OBJECTS);
  for( auto& name_formatter : wallet_api_ptr->get_result_formatters() )
     wallet_cli->format_result( name_formatter.first, name_formatter.second );

  boost::signals2::scoped_connection closed_connection(websocket_connection->closed.connect([=]{
     cerr << "Server has disconnected us.\n";
     wallet_cli->stop();
  }));
  (void)(closed_connection);
}

client_connection::~client_connection()
{
  // wait for everything to finish up
  fc::usleep(fc::milliseconds(500));
}

///////////////////////////////
// Cli Wallet Fixture
///////////////////////////////

cli_fixture::cli_fixture() :
     server_port_number(0),
     app_dir( graphene::utilities::temp_directory_path() ),
     app1( start_application(app_dir, server_port_number) ),
     con( app1, app_dir, server_port_number ),
     nathan_keys( {"5KQwrPbwdL6PhXujxW37FSSQZ1JiwsST4cqQzDeyXtP79zkvFD3"} )
{
  BOOST_TEST_MESSAGE("Setup cli_wallet::boost_fixture_test_case");

  using namespace graphene::chain;
  using namespace graphene::app;

  try
  {
     BOOST_TEST_MESSAGE("Setting wallet password");
     con.wallet_api_ptr->set_password("supersecret");
     con.wallet_api_ptr->unlock("supersecret");

     // import Nathan account
     BOOST_TEST_MESSAGE("Importing nathan key");
     BOOST_CHECK_EQUAL(nathan_keys[0], "5KQwrPbwdL6PhXujxW37FSSQZ1JiwsST4cqQzDeyXtP79zkvFD3");
     BOOST_CHECK(con.wallet_api_ptr->import_key("nathan", nathan_keys[0]));
  } catch( fc::exception& e ) {
     edump((e.to_detail_string()));
     throw;
  }
}

cli_fixture::~cli_fixture()
{
  BOOST_TEST_MESSAGE("Cleanup cli_wallet::boost_fixture_test_case");

  // wait for everything to finish up
  fc::usleep(fc::seconds(1));

  app1->shutdown();
#ifdef _WIN32
  sockQuit();
#endif
}

void cli_fixture::generate_blocks(fc::time_point_sec timestamp, bool miss_intermediate_blocks, uint32_t skip)
{
   auto db = app1->chain_database();

   if( miss_intermediate_blocks )
   {
      generate_block(skip);
      auto slots_to_miss = db->get_slot_at_time(timestamp);
      if( slots_to_miss <= 1 )
         return;
      --slots_to_miss;
      generate_block(skip, fc::ecc::private_key::regenerate(fc::sha256::hash(string("null_key"))), slots_to_miss);
      return;
   }
   while( db->head_block_time() < timestamp )
      generate_block(skip);
}

signed_block cli_fixture::generate_block(uint32_t skip, const fc::ecc::private_key& key, int miss_blocks)
{
   skip |= database::skip_undo_history_check;
   // skip == ~0 will skip checks specified in database::validation_steps
   auto db = app1->chain_database();
   auto block = db->generate_block(db->get_slot_time(miss_blocks + 1),
                            db->get_scheduled_witness(miss_blocks + 1),
                            key, skip);
   db->clear_pending();
   return block;
}

bool cli_fixture::generate_maintenance_block() {
   try {
      fc::ecc::private_key committee_key = fc::ecc::private_key::regenerate(fc::sha256::hash(string("null_key")));
      uint32_t skip = ~database::skip_fork_db;
      auto db = app1->chain_database();
      auto maint_time = db->get_dynamic_global_properties().next_maintenance_time;
      auto slots_to_miss = db->get_slot_at_time(maint_time);
      db->generate_block(db->get_slot_time(slots_to_miss),
                         db->get_scheduled_witness(slots_to_miss),
                         committee_key,
                         skip);
      return true;
   } catch (exception& e)
   {
      return false;
   }
}

void cli_fixture::init_nathan()
{
    try
    {
       BOOST_TEST_MESSAGE("Upgrade Nathan's account");

       account_object nathan_acct_before_upgrade, nathan_acct_after_upgrade;
       std::vector<signed_transaction> import_txs;
       signed_transaction upgrade_tx;

       BOOST_TEST_MESSAGE("Importing nathan's balance");
       import_txs = con.wallet_api_ptr->import_balance("nathan", nathan_keys, true);
       nathan_acct_before_upgrade = con.wallet_api_ptr->get_account("nathan");

       generate_block();

       // upgrade nathan
       BOOST_TEST_MESSAGE("Upgrading Nathan to LTM");
       upgrade_tx = con.wallet_api_ptr->upgrade_account("nathan", true);

       nathan_acct_after_upgrade = con.wallet_api_ptr->get_account("nathan");

       // verify that the upgrade was successful
       BOOST_CHECK_PREDICATE(
             std::not_equal_to<uint32_t>(),
             (nathan_acct_before_upgrade.membership_expiration_date.sec_since_epoch())
                   (nathan_acct_after_upgrade.membership_expiration_date.sec_since_epoch())
       );
       BOOST_CHECK(nathan_acct_after_upgrade.is_lifetime_member());
    } catch( fc::exception& e ) {
       edump((e.to_detail_string()));
       throw;
    }
}
