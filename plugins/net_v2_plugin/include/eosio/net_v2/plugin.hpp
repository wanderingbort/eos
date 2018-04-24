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

         virtual void set_program_options(options_description& cli, options_description& cfg) override;

         void plugin_initialize(const variables_map& options);
         void plugin_startup();
         void plugin_shutdown();

         void   broadcast_block(const signed_block &sb);

         bool                           connect( const string& endpoint );
         bool                           disconnect( const string& endpoint );
         optional<connection_status>    status( const string& endpoint )const;
         vector<connection_status>      connections()const;

         size_t num_peers() const;
      private:
         std::unique_ptr<class plugin_impl> my;
   };

} }

FC_REFLECT( eosio::net_v2::connection_status, (peer)(connecting)(syncing)(last_irreversible_block_num)(head_block_id) )
