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
      ,accepted_block_header_channel(app().get_channel<channels::accepted_block_header>())
      ,accepted_block_channel(app().get_channel<channels::accepted_block>())
      ,irreversible_block_channel(app().get_channel<channels::irreversible_block>())
      ,accepted_transaction_channel(app().get_channel<channels::accepted_transaction>())
      ,applied_transaction_channel(app().get_channel<channels::applied_transaction>())
      ,accepted_confirmation_channel(app().get_channel<channels::accepted_confirmation>())
      ,get_block_by_number_method(app().get_method<methods::get_block_by_number>())
      ,get_block_by_id_method(app().get_method<methods::get_block_by_id>())
      ,get_head_block_id_method(app().get_method<methods::get_head_block_id>())
      ,get_last_irreversible_block_number_method(app().get_method<methods::get_last_irreversible_block_number>())
      {
         get_head_block_provider = app().get_method<methods::get_head_block_id>().register_provider([this]() -> const block_id_type& {
            return head_block_id;
         });

         get_last_irreversible_block_number_provider = app().get_method<methods::get_last_irreversible_block_number>().register_provider([this]() -> uint32_t {
            return last_irreversible_block_number;
         });
      }

      ~mock_chain_plugin_impl() {
         if (scenario_thread) {
            stop_scenario();
         }
      }

      void start_scenario() {
         scenario_thread.emplace([this]() -> void {
            run_scenario();
         });
      }

      void stop_scenario() {
         shutting_down = true;
         scenario_thread->join();
         scenario_thread.reset();
      }

      void run_scenario() {
         while(!shutting_down);
      }

      io_service& ios;
      string scenario;
      string actor;

      channels::accepted_block_header::channel_type&              accepted_block_header_channel;
      channels::accepted_block::channel_type&                     accepted_block_channel;
      channels::irreversible_block::channel_type&                 irreversible_block_channel;
      channels::accepted_transaction::channel_type&               accepted_transaction_channel;
      channels::applied_transaction::channel_type&                applied_transaction_channel;
      channels::accepted_confirmation::channel_type&              accepted_confirmation_channel;

      methods::get_block_by_number::method_type&                  get_block_by_number_method;
      methods::get_block_by_id::method_type&                      get_block_by_id_method;
      methods::get_head_block_id::method_type&                    get_head_block_id_method;
      methods::get_last_irreversible_block_number::method_type&   get_last_irreversible_block_number_method;

      methods::get_head_block_id::method_type::handle                  get_head_block_provider;
      methods::get_last_irreversible_block_number::method_type::handle get_last_irreversible_block_number_provider;

      block_id_type      head_block_id;
      uint32_t           last_irreversible_block_number = 0U;

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
   }

   void mock_chain_plugin::plugin_startup() {
      my->start_scenario();
   }

   void mock_chain_plugin::plugin_shutdown() {
      my->stop_scenario();
   }


} }