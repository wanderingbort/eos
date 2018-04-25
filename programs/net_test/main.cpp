/**
 *  @file
 *  @copyright defined in eosio/LICENSE.txt
 */
#include <appbase/application.hpp>

#include <eosio/net_v2/plugin.hpp>
#include <eosio/chain/plugin_interface.hpp>
#include <eosio/chain/types.hpp>
#include <fc/exception/exception.hpp>

#include <boost/dll/runtime_symbol_info.hpp>
#include <boost/exception/diagnostic_information.hpp>

using namespace appbase;
using namespace eosio;
using namespace eosio::chain;
using namespace eosio::chain::plugin_interface;

int main(int argc, char** argv)
{
   try {
      app().set_version(0);
      auto root = fc::app_path(); 
      app().set_default_data_dir(root / "eosio/nodeos/data" );
      app().set_default_config_dir(root / "eosio/nodeos/config" );
      if(!app().initialize<net_v2::plugin>(argc, argv))
         return -1;

      app().get_method<methods::get_head_block_id>().register_provider([]() -> const block_id_type& {
         static block_id_type fake;
         return fake;
      });
      app().get_method<methods::get_last_irreversible_block_number>().register_provider([]() -> uint32_t {
         return 0U;
      });

      app().startup();
      app().exec();
   } catch (const fc::exception& e) {
      elog("${e}", ("e",e.to_detail_string()));
   } catch (const boost::exception& e) {
      elog("${e}", ("e",boost::diagnostic_information(e)));
   } catch (const std::exception& e) {
      elog("${e}", ("e",e.what()));
   } catch (...) {
      elog("unknown exception");
   }
   return 0;
}
