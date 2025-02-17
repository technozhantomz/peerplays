/*
 * Copyright (c) 2015 Cryptonomex, Inc., and contributors.
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

#include <graphene/delayed_node/delayed_node_plugin.hpp>
#include <graphene/chain/protocol/types.hpp>
#include <graphene/chain/database.hpp>
#include <graphene/app/api.hpp>

#include <fc/network/http/websocket.hpp>
#include <fc/rpc/websocket_api.hpp>
#include <fc/api.hpp>

namespace graphene { namespace delayed_node {
namespace bpo = boost::program_options;

namespace detail {
struct delayed_node_plugin_impl {
   std::string remote_endpoint;
   fc::http::websocket_client client;
   std::shared_ptr<fc::rpc::websocket_api_connection> client_connection;
   fc::api<graphene::app::database_api> database_api;
   boost::signals2::scoped_connection client_connection_closed;
   graphene::chain::block_id_type last_received_remote_head;
   graphene::chain::block_id_type last_processed_remote_head;
};
}

delayed_node_plugin::delayed_node_plugin()
   : my(new detail::delayed_node_plugin_impl)
{}

delayed_node_plugin::~delayed_node_plugin()
{}

void delayed_node_plugin::plugin_set_program_options(bpo::options_description& cli, bpo::options_description& cfg)
{
   cli.add_options()
         ("trusted-node", boost::program_options::value<std::string>(), "RPC endpoint of a trusted validating node (required)")
         ;
   cfg.add(cli);
}

void delayed_node_plugin::connect()
{
   fc::http::websocket_connection_ptr con;
   try
   {
      con = my->client.connect(my->remote_endpoint);
   }
   catch( const fc::exception& e )
   {
      wlog("Error while connecting: ${e}", ("e", e.to_detail_string()));
      connection_failed();
      return;
   }
   my->client_connection = std::make_shared<fc::rpc::websocket_api_connection>(
           con, GRAPHENE_NET_MAX_NESTED_OBJECTS );
   my->database_api = my->client_connection->get_remote_api<graphene::app::database_api>(0);
   my->database_api->set_block_applied_callback([this]( const fc::variant& block_id )
   {
      fc::from_variant( block_id, my->last_received_remote_head, GRAPHENE_MAX_NESTED_OBJECTS );
   } );
   my->client_connection_closed = my->client_connection->closed.connect([this] {
      connection_failed();
   });
}

void delayed_node_plugin::plugin_initialize(const boost::program_options::variables_map& options)
{
   FC_ASSERT(options.count("trusted-node") > 0);
   ilog("delayed_node_plugin:  plugin_initialize() begin");
   my->remote_endpoint = "ws://" + options.at("trusted-node").as<std::string>();
   ilog("delayed_node_plugin:  plugin_initialize() end");
}

void delayed_node_plugin::sync_with_trusted_node()
{
   auto& db = database();
   uint32_t synced_blocks = 0;
   uint32_t pass_count = 0;
   while( true )
   {
      graphene::chain::dynamic_global_property_object remote_dpo = my->database_api->get_dynamic_global_properties();
      if( remote_dpo.last_irreversible_block_num <= db.head_block_num() )
      {
         if( remote_dpo.last_irreversible_block_num < db.head_block_num() )
         {
            wlog( "Trusted node seems to be behind delayed node" );
         }
         if( synced_blocks > 1 )
         {
            ilog( "Delayed node finished syncing ${n} blocks in ${k} passes", ("n", synced_blocks)("k", pass_count) );
         }
         break;
      }
      pass_count++;
      while( remote_dpo.last_irreversible_block_num > db.head_block_num() )
      {
         fc::optional<graphene::chain::signed_block> block = my->database_api->get_block( db.head_block_num()+1 );
         // TODO: during sync, decouple requesting blocks from preprocessing + applying them
         FC_ASSERT(block, "Trusted node claims it has blocks it doesn't actually have.");
         ilog("Pushing block #${n}", ("n", block->block_num()));
         // timur: failed to merge from bitshares, API n/a in peerplays
         // db.precompute_parallel( *block, graphene::chain::database::skip_nothing ).wait();
         db.push_block(*block);
         synced_blocks++;
      }
   }
}

void delayed_node_plugin::mainloop()
{
   while( true )
   {
      try
      {
         fc::usleep( fc::microseconds( 296645 ) );  // wake up a little over 3Hz

         if( my->last_received_remote_head == my->last_processed_remote_head )
            continue;

         sync_with_trusted_node();
         my->last_processed_remote_head = my->last_received_remote_head;
      }
      catch( const fc::exception& e )
      {
         elog("Error during connection: ${e}", ("e", e.to_detail_string()));
      }
   }
}

void delayed_node_plugin::plugin_startup()
{
   fc::async([this]()
   {
      mainloop();
   });

   connect();
}

void delayed_node_plugin::connection_failed()
{
   my->last_received_remote_head = my->last_processed_remote_head;
   elog("Connection to trusted node failed; retrying in 5 seconds...");
   fc::schedule([this]{connect();}, fc::time_point::now() + fc::seconds(5));
}

} }
