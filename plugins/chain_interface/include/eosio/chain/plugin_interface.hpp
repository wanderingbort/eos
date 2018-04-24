/**
 *  @file
 *  @copyright defined in eos/LICENSE.txt
 */
#pragma once

#include <appbase/channel.hpp>
#include <appbase/method.hpp>

#include <eosio/chain/block.hpp>
#include <eosio/chain/block_trace.hpp>
#include <eosio/chain/transaction.hpp>
#include <eosio/chain/transaction_trace.hpp>

namespace eosio { namespace chain { namespace plugin_interface {
   using namespace eosio::chain;
   using namespace appbase;

   using signed_block_ptr = std::shared_ptr<signed_block>;
   using packed_transaction_ptr = std::shared_ptr<packed_transaction>;
   using block_trace_ptr = std::shared_ptr<block_trace>;
   using transaction_trace_ptr = std::shared_ptr<transaction_trace>;
   
   struct chain_plugin_interface;

   template<typename Id>
   struct validation_result {
      Id id;  ///< The ID of the object being validated
      fc::exception_ptr err;  ///< any exception thrown during validation or nullptr if successful
   };

   using block_validation_result = validation_result<block_id_type>;
   using transaction_validation_result = validation_result<transaction_id_type>;

#define EOSIO_CATCH_AND_PUBLISH(CHANNEL, RESULT_TYPE, ... )\
   catch( fc::exception& er ) { \
      CHANNEL.publish(RESULT_TYPE { __VA_ARGS__, er.dynamic_copy_exception() }); \
   } catch( const std::exception& e ) {  \
      CHANNEL.publish(RESULT_TYPE { __VA_ARGS__, std::make_shared<fc::exception>( \
                FC_LOG_MESSAGE( warn, "rethrow ${what}: ", ("what",e.what())), \
                fc::std_exception_code,\
                BOOST_CORE_TYPEID(e).name(), \
                e.what() )}); \
   } catch( ... ) {  \
      CHANNEL.publish(RESULT_TYPE { __VA_ARGS__, std::make_shared<fc::unhandled_exception>( \
                FC_LOG_MESSAGE( warn, "rethrow"), \
                std::current_exception()  )} ); \
   }

   namespace channels {
      using incoming_blocks = channel_decl<chain_plugin_interface, signed_block_ptr>;
      using incoming_transactions = channel_decl<chain_plugin_interface, packed_transaction_ptr>;
      using applied_block = channel_decl<chain_plugin_interface, block_trace_ptr>;

      using transaction_validation_results = channel_decl<chain_plugin_interface, validation_result<transaction_id_type>>;
      using block_validation_results = channel_decl<chain_plugin_interface, validation_result<block_id_type>>;
   }

   namespace methods {
      using get_block_by_number = method_decl<chain_plugin_interface, const signed_block&(uint32_t block_num)>;
      using get_block_by_id = method_decl<chain_plugin_interface, const signed_block&(const block_id_type& block_id)>;
      using get_head_block_id = method_decl<chain_plugin_interface, const block_id_type& ()>;
      using get_last_irreversible_block_number = method_decl<chain_plugin_interface, uint32_t ()>;
   }


} } }