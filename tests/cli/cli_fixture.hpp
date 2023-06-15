#include <graphene/app/application.hpp>
#include <graphene/wallet/wallet.hpp>

#include <fc/network/http/websocket.hpp>
#include <fc/rpc/cli.hpp>
#include <fc/rpc/websocket_api.hpp>
#include <fc/thread/thread.hpp>

#define INVOKE(test) ((struct test*)this)->test_method();

int get_available_port();

std::shared_ptr<graphene::app::application> start_application(fc::temp_directory& app_dir, int& server_port_number);

///////////
/// @brief a class to make connecting to the application server easier
///////////
class client_connection
{
public:
   client_connection(
         std::shared_ptr<graphene::app::application> app,
         const fc::temp_directory& data_dir,
         const int server_port_number
   );
   ~client_connection();
public:
   fc::http::websocket_client websocket_client;
   graphene::wallet::wallet_data wallet_data;
   fc::http::websocket_connection_ptr websocket_connection;
   std::shared_ptr<fc::rpc::websocket_api_connection> api_connection;
   fc::api<login_api> remote_login_api;
   std::shared_ptr<graphene::wallet::wallet_api> wallet_api_ptr;
   fc::api<graphene::wallet::wallet_api> wallet_api;
   std::shared_ptr<fc::rpc::cli> wallet_cli;
   std::string wallet_filename;
};

///////////////////////////////
// Cli Wallet Fixture
///////////////////////////////
struct cli_fixture
{
   class dummy
   {
   public:
      ~dummy()
      {
         // wait for everything to finish up
         fc::usleep(fc::milliseconds(500));
      }
   };
   dummy dmy;
   int server_port_number;
   fc::temp_directory app_dir;
   std::shared_ptr<graphene::app::application> app1;
   client_connection con;
   std::vector<std::string> nathan_keys;

   cli_fixture();
   ~cli_fixture();

   signed_block generate_block(uint32_t skip = ~0,
                               const fc::ecc::private_key& key = fc::ecc::private_key::regenerate(fc::sha256::hash(string("null_key"))),
                               int miss_blocks = 0);

   void generate_blocks(fc::time_point_sec timestamp, bool miss_intermediate_blocks = true, uint32_t skip = ~0);

   ///////////
   /// @brief Skip intermediate blocks, and generate a maintenance block
   /// @returns true on success
   ///////////
   bool generate_maintenance_block();

   ///////////
   // @brief init "nathan" account and make it LTM to use in tests
   //////////
   void init_nathan();
};

