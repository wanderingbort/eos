/**
 *  @file
 *  @copyright defined in eos/LICENSE.txt
 */
#include <eosio/chain/types.hpp>
#include <eosio/net_v2/mock_chain_plugin.hpp>
#include <eosio/chain/plugin_interface.hpp>

#include <fc/reflect/variant.hpp>
#include <fc/exception/exception.hpp>


using namespace appbase;
using namespace eosio::chain;
using namespace eosio::chain::plugin_interface;
using boost::asio::io_service;
using namespace std;

namespace eosio { namespace net_v2 {

   static mock_chain_plugin& _plugin = app().register_plugin<mock_chain_plugin>();

   class mock_chain_plugin_impl {
   public:
      mock_chain_plugin_impl(io_service& ios, const string &scenario, const string &actor)
      : ios(ios)
      {}

      void start_scenario() {

      }

      void stop_scenario() {

      }

      io_service& ios;
   };

   using plugin_impl_wptr = std::weak_ptr<mock_chain_plugin_impl>;

   mock_chain_plugin::mock_chain_plugin()
   {
   }

   mock_chain_plugin::~mock_chain_plugin() {
   }

   void mock_chain_plugin::set_program_options( options_description& cli, options_description& /*cfg*/ )
   {
      cli.add_options()
         ( "scenario", bpo::value<string>()->required(), "the scenario to run")
         ( "actor", bpo::value<string>()->required(), "the actor to play in the scenario")
         ;
   }

   void mock_chain_plugin::plugin_initialize( const variables_map& options ) {
      my.reset(new mock_chain_plugin_impl(app().get_io_service(), options.at("scenario").as<string>(), options.at("actor").as<string>()));
   }

   void mock_chain_plugin::plugin_startup() {
      my->start_scenario();
   }

   void mock_chain_plugin::plugin_shutdown() {
      my->stop_scenario();
   }


} }