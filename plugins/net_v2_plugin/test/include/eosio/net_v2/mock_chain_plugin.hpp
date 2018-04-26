/**
 *  @file
 *  @copyright defined in eos/LICENSE.txt
 */
#pragma once
#include <appbase/application.hpp>
#include <eosio/chain/types.hpp>
#include <eosio/chain/block.hpp>

namespace eosio { namespace net_v2 {
using namespace eosio::chain;

class mock_chain_plugin : public appbase::plugin<mock_chain_plugin>
{
public:
   mock_chain_plugin();
   virtual ~mock_chain_plugin();

   APPBASE_PLUGIN_REQUIRES()

   virtual void set_program_options(appbase::options_description& cli, appbase::options_description& cfg) override;

   void plugin_initialize(const appbase::variables_map& options);
   void plugin_startup();
   void plugin_shutdown();

private:
   std::unique_ptr<class mock_chain_plugin_impl> my;
};

} }
