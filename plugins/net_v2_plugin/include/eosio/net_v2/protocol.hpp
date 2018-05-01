/**
 *  @file
 *  @copyright defined in eos/LICENSE.txt
 */
#pragma once
#include <eosio/chain/block.hpp>
#include <eosio/chain/types.hpp>
#include <chrono>

namespace eosio { namespace net_v2 {
   using namespace chain;
   using namespace fc;

   static_assert(sizeof(std::chrono::system_clock::duration::rep) >= 8, "system_clock is expected to be at least 64 bits");
   typedef std::chrono::system_clock::duration::rep tstamp;

   struct hello_message {
      int16_t                    network_version = 0; ///< derived from git commit hash, not sequential
      chain_id_type              chain_id; ///< used to identify chain
      fc::sha256                 node_id; ///< used to identify peers and prevent self-connect
      string                     p2p_address;
      string                     os;
      string                     agent;
   };

   enum goodbye_reason {
      no_reason, ///< no reason to go away
      self, ///< the connection is to itself
      duplicate, ///< the connection is redundant
      wrong_chain, ///< the peer's chain id doesn't match
      wrong_version, ///< the peer's network version doesn't match
      forked, ///< the peer's irreversible blocks are different
      unlinkable, ///< the peer sent a block we couldn't use
      bad_transaction, ///< the peer sent a transaction that failed verification
      validation, ///< the peer sent a block that failed validation
      benign_other, ///< reasons such as a timeout. not fatal but warrant resetting
      fatal_other, ///< a catch-all for errors we don't have discriminated
      authentication ///< peer failed authenicatio
   };

   constexpr auto reason_str( goodbye_reason rsn ) {
      switch ( rsn ) {
         case no_reason : return "no reason";
         case self : return "self connect";
         case duplicate : return "duplicate";
         case wrong_chain : return "wrong chain";
         case wrong_version : return "wrong version";
         case forked : return "chain is forked";
         case unlinkable : return "unlinkable block received";
         case bad_transaction : return "bad transaction";
         case validation : return "invalid block";
         case authentication : return "authentication failure";
         case fatal_other : return "some other failure";
         case benign_other : return "some other non-fatal condition";
         default : return "some crazy reason";
      }
   }

   struct goodbye_message {
      explicit goodbye_message (goodbye_reason r = no_reason) : reason(r), node_id() {}
      goodbye_reason reason;
      fc::sha256 node_id; ///< for duplicate notification
   };

   typedef std::chrono::system_clock::duration::rep tstamp;
   typedef int32_t                                  tdist;

   struct status_message {
      uint32_t         last_irreversible_block_number;
      block_id_type    head_block_id;
   };

   struct subscribe_message {
   };

   struct unsubscribe_message {
   };

   struct subscription_refused_message {
   };

   struct block_received_message {
      block_id_type       block_id;
   };

   struct transaction_received_message {
      transaction_id_type transaction_id;
   };

   using net_message = static_variant<hello_message,
                                      goodbye_message,
                                      status_message,
                                      subscribe_message,
                                      unsubscribe_message,
                                      subscription_refused_message,
                                      block_received_message,
                                      transaction_received_message,
                                      signed_block,
                                      packed_transaction>;


   using net_message_ptr = std::shared_ptr<net_message>;

} } // namespace eosio::net_v2

FC_REFLECT( eosio::net_v2::hello_message,
            (network_version)(chain_id)(node_id)
            (os)(agent))
FC_REFLECT( eosio::net_v2::goodbye_message, (reason)(node_id) )
FC_REFLECT( eosio::net_v2::status_message, (last_irreversible_block_number)(head_block_id))
FC_REFLECT( eosio::net_v2::subscribe_message, BOOST_PP_SEQ_NIL )
FC_REFLECT( eosio::net_v2::unsubscribe_message, BOOST_PP_SEQ_NIL )
FC_REFLECT( eosio::net_v2::subscription_refused_message, BOOST_PP_SEQ_NIL )
FC_REFLECT( eosio::net_v2::block_received_message, (block_id))
FC_REFLECT( eosio::net_v2::transaction_received_message, (transaction_id))
