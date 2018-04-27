/**
 *  @file
 *  @copyright defined in eosio/LICENSE.txt
 */
#include <appbase/application.hpp>

#include <eosio/net_v2/plugin.hpp>
#include <eosio/net_v2/mock_chain_plugin.hpp>
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
   auto root = boost::filesystem::temp_directory_path() / boost::filesystem::unique_path();
   boost::filesystem::create_directories(root);

   try {
      app().set_version(0);
      app().set_default_data_dir(root);
      app().set_default_config_dir(root);
      if(!app().initialize<net_v2::plugin, net_v2::mock_chain_plugin>(argc, argv))
         return -1;

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

   boost::filesystem::remove_all(root);
   return 0;
}
