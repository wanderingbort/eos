/**
 *  @file
 *  @copyright defined in eos/LICENSE.txt
 */
#include <eosio/chain/types.hpp>
#include <eosio/net_v2/mock_chain_plugin.hpp>
#include <eosio/chain/plugin_interface.hpp>
#include <boost/thread/thread.hpp>

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
      ,scenario(scenario)
      ,actor(actor)
      ,incoming_blocks_channel(app().get_channel<channels::incoming_blocks>())
      ,incoming_transactions_channel(app().get_channel<channels::incoming_transactions>())
      ,applied_block_channel(app().get_channel<channels::applied_block>())
      ,transaction_validation_results_channel(app().get_channel<channels::transaction_validation_results>())
      ,block_validation_results_channel(app().get_channel<channels::block_validation_results>())
      ,get_block_by_number_method(app().get_method<methods::get_block_by_number>())
      ,get_block_by_id_method(app().get_method<methods::get_block_by_id>())
      ,get_head_block_id_method(app().get_method<methods::get_head_block_id>())
      ,get_last_irreversible_block_number_method(app().get_method<methods::get_last_irreversible_block_number>())
      {
      }

      struct trace_hack {
         trace_hack()
         :block()
         ,trace(block)
         {}

         trace_hack( const trace_hack& ) = delete;
         trace_hack( trace_hack&& ) = delete;

         trace_hack& operator=(const trace_hack &) = delete;
         trace_hack& operator=( trace_hack&& ) = delete;

         signed_block block;
         block_trace trace;
      };

      void start_scenario() {
         // test fires
         auto hack = std::make_shared<trace_hack>();

         app().get_channel<channels::applied_block>().publish(block_trace_ptr(hack, &hack->trace));
         scenario_thread.emplace([this]() -> void {
            run_scenario();
         });
      }

      void stop_scenario() {
         shutting_down;
         scenario_thread->join();
         scenario_thread.reset();
      }

      void run_scenario() {
         while(!shutting_down);
      }

      io_service& ios;
      string scenario;
      string actor;

      channels::incoming_blocks::channel_type&                    incoming_blocks_channel;
      channels::incoming_transactions::channel_type&              incoming_transactions_channel;
      channels::applied_block::channel_type&                      applied_block_channel;
      channels::transaction_validation_results::channel_type&     transaction_validation_results_channel;
      channels::block_validation_results::channel_type&           block_validation_results_channel;

      methods::get_block_by_number::method_type&                  get_block_by_number_method;
      methods::get_block_by_id::method_type&                      get_block_by_id_method;
      methods::get_head_block_id::method_type&                    get_head_block_id_method;
      methods::get_last_irreversible_block_number::method_type&   get_last_irreversible_block_number_method;

      fc::optional<thread> scenario_thread;
      volatile bool shutting_down = false;
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
         ( "scenario", bpo::value<std::string>()->required(), "the scenario to run")
         ( "actor", bpo::value<std::string>()->required(), "the actor to play in the scenario")
         ;
   }

   void mock_chain_plugin::plugin_initialize( const variables_map& options ) {
      my.reset(new mock_chain_plugin_impl(app().get_io_service(), options.at("scenario").as<string>(), options.at("actor").as<string>()));
      my->start_scenario();
   }

   void mock_chain_plugin::plugin_startup() {
   }

   void mock_chain_plugin::plugin_shutdown() {
      my->stop_scenario();
   }


} }