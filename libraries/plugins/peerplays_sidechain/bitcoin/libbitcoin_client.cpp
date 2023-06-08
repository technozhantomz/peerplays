
#include <graphene/peerplays_sidechain/bitcoin/libbitcoin_client.hpp>

#include <system_error>

#include <bitcoin/explorer/config/transaction.hpp>
#include <bitcoin/system/config/hash256.hpp>

#include <boost/xpressive/xpressive.hpp>

#include <fc/crypto/hex.hpp>
#include <fc/log/logger.hpp>

namespace graphene { namespace peerplays_sidechain {

libbitcoin_client::libbitcoin_client(std::string url) :
      obelisk_client(LIBBITCOIN_SERVER_TIMEOUT, LIBBITCOIN_SERVER_RETRIES) {

   std::string reg_expr = "^((?P<Protocol>https|http|tcp):\\/\\/)?(?P<Host>[a-zA-Z0-9\\-\\.]+)(:(?P<Port>\\d{1,5}))?(?P<Target>\\/.+)?";
   boost::xpressive::sregex sr = boost::xpressive::sregex::compile(reg_expr);

   boost::xpressive::smatch sm;

   if (boost::xpressive::regex_search(url, sm, sr)) {
      protocol = sm["Protocol"];
      if (protocol.empty()) {
         protocol = "tcp";
      }

      host = sm["Host"];
      if (host.empty()) {
         host + "localhost";
      }

      port = sm["Port"];
      if (port.empty()) {
         port = "9091";
      }
   }

   uint16_t port_num = std::stoi(port);
   std::string final_url = protocol + "://" + host;

   libbitcoin::config::endpoint address(final_url, port_num);

   libbitcoin::client::connection_type connection;
   connection.retries = LIBBITCOIN_SERVER_RETRIES;
   connection.server = address;

   if (!obelisk_client.connect(connection)) {
      elog("Can't connect libbitcoin for url: ${url}", ("url", final_url));
   }

   is_connected = true;
}

std::string libbitcoin_client::send_transaction(std::string tx) {

   std::string res;

   auto error_handler = [&](const std::error_code &ec) {
      elog("error on sending bitcoin transaction ${error_code}", ("error_code", ec.message()));
   };

   auto result_handler = [&](libbitcoin::code result_code) {
      ilog("result code on sending transaction ${result_code}", ("result_code", result_code.message()));
      res = std::to_string(result_code.value());
   };

   libbitcoin::explorer::config::transaction transaction(tx);

   libbitcoin::chain::transaction trx;

   // This validates the tx, submits it to local tx pool, and notifies peers.
   obelisk_client.transaction_pool_broadcast(error_handler, result_handler, transaction);
   obelisk_client.wait();

   return res;
}

libbitcoin::chain::output::list libbitcoin_client::get_transaction(std::string tx_id, std::string &tx_hash, uint32_t &confirmitions) {

   libbitcoin::chain::output::list outs;

   auto error_handler = [&](const std::error_code &ec) {
      elog("error on fetch_trx_by_hash: ${hash} ${error_code}", ("hash", tx_id)("error_code", ec.message()));
   };

   auto transaction_handler = [&](const libbitcoin::chain::transaction &tx_handler) {
      tx_hash = libbitcoin::config::hash256(tx_handler.hash(false)).to_string();
      // TODO try to find this value (confirmitions)
      confirmitions = 1;
      outs = tx_handler.outputs();
   };

   libbitcoin::hash_digest hash = libbitcoin::config::hash256(tx_id);

   // obelisk_client.blockchain_fetch_transaction (error_handler, transaction_handler,hash);
   obelisk_client.blockchain_fetch_transaction2(error_handler, transaction_handler, hash);

   obelisk_client.wait();

   return outs;
}

std::vector<list_unspent_replay> libbitcoin_client::listunspent(std::string address, double amount) {
   std::vector<list_unspent_replay> result;

   auto error_handler = [&](const std::error_code &ec) {
      elog("error on list_unspent ${error_code}", ("error_code", ec.message()));
   };

   auto replay_handler = [&](const libbitcoin::chain::points_value &points) {
      for (auto &point : points.points) {
         list_unspent_replay output;
         output.hash = libbitcoin::config::hash256(point.hash()).to_string();
         output.value = point.value();
         output.index = point.index();
         result.emplace_back(output);
      }
   };

   libbitcoin::wallet::payment_address payment_address(address);
   uint64_t satoshi = 100000000 * amount;

   obelisk_client.blockchain_fetch_unspent_outputs(error_handler,
                                                   replay_handler, payment_address, satoshi, libbitcoin::wallet::select_outputs::algorithm::individual);

   obelisk_client.wait();

   return result;
}

bool libbitcoin_client::get_is_test_net() {

   bool result = false;

   auto error_handler = [&](const std::error_code &ec) {
      elog("error on fetching genesis block ${error_code}", ("error_code", ec.message()));
   };

   auto block_header_handler = [&](const libbitcoin::chain::header &block_header) {
      std::string hash_str = libbitcoin::config::hash256(block_header.hash()).to_string();
      if (hash_str == GENESIS_TESTNET_HASH || hash_str == GENESIS_REGTEST_HASH) {
         result = true;
      }
   };

   obelisk_client.blockchain_fetch_block_header(error_handler, block_header_handler, 0);

   obelisk_client.wait();
   return result;
}

uint64_t libbitcoin_client::get_fee_from_trx(libbitcoin::chain::transaction trx) {
   bool general_fee_est_error = false;

   if (trx.is_coinbase()) {
      return 0;
   }

   const auto total_output_value = trx.total_output_value();
   // get the inputs and from inputs previous outputs
   std::map<libbitcoin::hash_digest, std::vector<uint32_t>> prev_out_trxs;
   for (auto &ins : trx.inputs()) {
      const auto &prev_out = ins.previous_output();
      prev_out_trxs[prev_out.hash()].emplace_back(prev_out.index());
   }

   // fetch the trx to get total input value
   uint64_t total_input_value = 0;
   auto transaction_handler = [&](const libbitcoin::chain::transaction &tx_handler) {
      std::vector<uint32_t> indexes = prev_out_trxs[tx_handler.hash()];

      for (auto &index : indexes) {
         total_input_value += tx_handler.outputs()[index].value();
      }
   };

   auto error_handler = [&](const std::error_code &ec) {
      elog("error on fetching trx ${error_code}", ("error_code", ec.message()));
      general_fee_est_error = true;
   };

   for (const auto &iter : prev_out_trxs) {
      if (general_fee_est_error) {
         break;
      }

      obelisk_client.blockchain_fetch_transaction2(error_handler, transaction_handler, iter.first);
      obelisk_client.wait();
   }

   if (total_input_value >= total_output_value) {
      return total_input_value - total_output_value;
   } else {
      // something is really wrong if this happens,so we are going to mark as an error
      elog("On fee estimation something is wrong in total inputs and total outputs for trx hash: ${hash}",
           ("hash", libbitcoin::config::hash256(trx.hash()).to_string()));
      return 0;
   }
}

uint64_t libbitcoin_client::get_average_fee_from_trxs(std::vector<libbitcoin::chain::transaction> trx_list) {
   std::vector<uint64_t> fee_per_trxs;

   for (auto &trx : trx_list) {

      uint64_t fee = get_fee_from_trx(trx);
      if (fee > 0) {
         fee_per_trxs.emplace_back(fee);
      }
   }

   uint64_t average_estimated_fee = 0;

   if (fee_per_trxs.size()) {
      for (const auto &fee : fee_per_trxs) {
         average_estimated_fee += fee;
      }

      average_estimated_fee /= fee_per_trxs.size();
   }

   return average_estimated_fee;
}
}} // namespace graphene::peerplays_sidechain