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

   struct connection_status {
      string            peer;
      bool              connecting = false;
      bool              syncing    = false;
      uint32_t          last_irreversible_block_num;
      block_id_type     head_block_id;
   };

   class plugin : public appbase::plugin<plugin>
   {
      public:
         plugin();
         virtual ~plugin();

         APPBASE_PLUGIN_REQUIRES()

         virtual void set_program_options(appbase::options_description& cli, appbase::options_description& cfg) override;

         void plugin_initialize(const appbase::variables_map& options);
         void plugin_startup();
         void plugin_shutdown();

      private:
         std::unique_ptr<class plugin_impl> my;
   };

} }

FC_REFLECT( eosio::net_v2::connection_status, (peer)(connecting)(syncing)(last_irreversible_block_num)(head_block_id) )
