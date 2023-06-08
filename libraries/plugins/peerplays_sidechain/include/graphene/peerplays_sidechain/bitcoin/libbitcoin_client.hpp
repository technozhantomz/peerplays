#pragma once

#include <bitcoin/client/obelisk_client.hpp>
#include <bitcoin/client/socket_stream.hpp>
#include <bitcoin/system/chain/block.hpp>

#include <boost/signals2.hpp>
#include <mutex>

#define LIBBITCOIN_SERVER_TIMEOUT (10)
#define LIBBITCOIN_SERVER_RETRIES (100)
#define DEAFULT_LIBBITCOIN_TRX_FEE (20000)
#define MAX_TRXS_IN_MEMORY_POOL (30000)
#define MIN_TRXS_IN_BUCKET (100)

#define GENESIS_MAINNET_HASH "000000000019d6689c085ae165831e934ff763ae46a2a6c172b3f1b60a8ce26f"
#define GENESIS_TESTNET_HASH "000000000933ea01ad0ee984209779baaec3ced90fa3f408719526f8d77f4943"
#define GENESIS_REGTEST_HASH "0f9188f13cb7b2c71f2a335e3a4fc328bf5beb436012afca590b1a11466e2206"

namespace graphene { namespace peerplays_sidechain {

typedef std::function<void(const libbitcoin::chain::block &)>
      block_update_handler;

struct list_unspent_replay {
   std::string hash;
   uint64_t value;
   uint32_t index;
};

class libbitcoin_client {
public:
   libbitcoin_client(std::string url);
   std::string send_transaction(const std::string tx);
   libbitcoin::chain::output::list get_transaction(std::string tx_id, std::string &tx_hash, uint32_t &confirmitions);
   std::vector<list_unspent_replay> listunspent(std::string address, double amount);
   uint64_t get_average_fee_from_trxs(std::vector<libbitcoin::chain::transaction> trx_list);
   uint64_t get_fee_from_trx(libbitcoin::chain::transaction trx);
   bool get_is_test_net();

private:
   libbitcoin::client::obelisk_client obelisk_client;
   libbitcoin::protocol::zmq::identifier id;

   std::string protocol;
   std::string host;
   std::string port;
   std::string url;

   bool is_connected = false;
};

}} // namespace graphene::peerplays_sidechain