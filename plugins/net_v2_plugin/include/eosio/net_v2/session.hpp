/**
 *  @file
 *  @copyright defined in eos/LICENSE.txt
 */
#pragma once
#include <eosio/chain/block.hpp>
#include <eosio/chain/types.hpp>

#include <boost/asio/steady_timer.hpp>
#include <boost/multi_index_container.hpp>
#include <boost/multi_index/member.hpp>
#include <boost/multi_index/ordered_index.hpp>
#include <boost/multi_index/hashed_index.hpp>

#include <eosio/net_v2/state_machine.hpp>
#include <eosio/net_v2/connection_manager.hpp>
#include <eosio/net_v2/protocol.hpp>

namespace bmi = boost::multi_index;
using boost::multi_index_container;
using bmi::indexed_by;
using bmi::ordered_non_unique;
using bmi::hashed_unique;
using bmi::member;
using bmi::tag;
using boost::asio::steady_timer;

using namespace eosio::chain;
using namespace eosio::net_v2::state_machine;


namespace eosio { namespace net_v2 {

   using packed_transacton_ptr = std::shared_ptr<packed_transaction>;
   using signed_block_ptr = std::shared_ptr<signed_block>;
   using bytes_ptr = std::shared_ptr<bytes>;
   using dynamic_bitset = std::vector<bool>;

   struct transaction_cache_object {
      transaction_id_type   id;
      fc::time_point        expiration;
      packed_transacton_ptr trx;
      bytes_ptr             raw;
      dynamic_bitset        session_acks;
   };

   struct by_id;
   struct by_expiration;
   using transaction_cache = multi_index_container<
      transaction_cache_object,
      indexed_by<
         hashed_unique<
            tag< by_id >,
            member < transaction_cache_object, transaction_id_type, &transaction_cache_object::id >,
            std::hash<transaction_id_type>
         >,
         ordered_non_unique<
            tag< by_expiration >,
            member< transaction_cache_object, fc::time_point, &transaction_cache_object::expiration >
         >
      >
   >;

   struct block_cache_object {
      block_id_type    id;
      block_id_type    prev;
      signed_block_ptr blk;
      bytes_ptr        raw;
      dynamic_bitset   session_acks;
   };

   using block_cache = multi_index_container<
      block_cache_object,
      indexed_by<
         hashed_unique<
            tag<by_id>,
            member< block_cache_object, block_id_type, &block_cache_object::id>,
            std::hash<block_id_type>
         >
      >
   >;

   class session;
   using session_ptr = std::shared_ptr<session>;
   using session_wptr = std::weak_ptr<session>;


   struct chain_info {
      uint32_t      last_irreversible_block_number = 0;
      block_id_type head_block_id;
      chain_id_type chain_id;
   };

   struct node_info {
      fc::sha256       node_id;
      string           public_endpoint;
      string           agent_name;
   };

   struct shared_state {
      chain_info local_chain;

      node_info  local_info;


      transaction_cache txn_cache;
      block_cache       blk_cache;

      int next_session_index = 0;
      int reserve_session_index() {
         return next_session_index++;
      }
   };

   // internal events when the system does something
   struct sent_block_event {
      block_id_type id;
      const block_cache_object& entry;
   };

   struct received_block_event {
      block_id_type id;
      const block_cache_object& entry;
   };

   struct sent_transaction_event {
      block_id_type id;
      const transaction_cache_object& entry;
   };

   struct received_transaction_event {
      block_id_type id;
      const transaction_cache_object& entry;
   };

   struct broadcast_block_event {
      block_id_type id;
      const block_cache_object& entry;
   };
   struct broadcast_transaction_event {
      block_id_type id;
      const block_cache_object& entry;
   };

   namespace base {
      struct connected_state;
   };

   namespace broadcast {
      struct desynced_state;
      struct subscribed_state;


      /**
       * Idle state: this peer is connected but not subscribed.
       */
      struct idle_state {
         auto on(const subscribe_message& msg) -> next_states<desynced_state>;
      };

      /**
       * De-synced_state state: this peer is connected and subscribed but not sync'd properly to receive
       * broadcast messages
       */
      struct desynced_state : public container<desynced_state>{
         void enter(const base::connected_state& connected, const session& peer);

         /**
          * Internal event to signal when desynced_state is resolved
          */
         struct completed_event{};

         auto on(const unsubscribe_message& msg) -> next_states<idle_state>;

         auto on(const completed_event& event) -> next_states<subscribed_state>;

         void exit(const base::connected_state& connected, const session& peer);

         /**
          * peer-behind state: this peer is subscribed to real-time broadcast HOWEVER, it
          * does not know all the blocks preceeding our broadcast so, it must catch up before
          * receiving real-time messages
          */
         struct peer_behind_state {
            void on(const sent_block_event& event, desynced_state& parent, const base::connected_state& connected, const session& peer);
         };

         /**
          * local-behind state : this peer is subscribed to the real-time broadcast but has more information
          * about the chain than we do locally.
          */
         struct local_behind_state {
            void on(const received_block_event& event, desynced_state& parent, const base::connected_state& connected, const session& peer);
         };

         machine<no_default_state, peer_behind_state, local_behind_state> sub_state;

         using state_machine_member_list = std::tuple<
            container::member<decltype(sub_state), &desynced_state::sub_state>
         >;
      };

      /**
       * Subscribed state: this peer is connected and subscribed.
       */
      struct subscribed_state  {
         auto on(const unsubscribe_message& msg) -> next_states<idle_state>;

         void on(const broadcast_block_event& event, base::connected_state&, session& peer);

         void on(const broadcast_transaction_event& event, base::connected_state&, session& peer);
      };

      using state_machine_type = machine<idle_state, desynced_state, subscribed_state>;
   }

   namespace receiver {
      struct subscribed_state;
      struct idle_state {
         auto on(const status_message& msg) -> next_states<subscribed_state> ;
      };

      /**
       * Subscribed state: this peer is connected and subscribed.
       */
      struct subscribed_state {
         void enter(base::connected_state& connected, const session& peer_session);
         void exit(base::connected_state& connected, const session& peer_session);
      };

      using state_machine_type = machine<idle_state, subscribed_state>;
   }

   namespace base {
      struct connection_established_event {};
      struct connection_accepted_event {};
      struct connection_lost_event {};

      struct handshaking_state;
      struct disconnected_state {
         auto on(const connection_established_event& e) -> next_states<handshaking_state>;
      };

      struct connected_state;
      struct handshaking_state {
         struct hello_sent_event{};
         struct hello_failed_event{};

         bool handshake_sent = false;
         bool handshake_received = false;

         void enter(session& peer);
         auto on(const hello_message& msg, session& session) -> next_states<connected_state>;
         auto on(const hello_sent_event& msg) -> next_states<connected_state>;
         auto on(const hello_failed_event& msg, session& peer) -> void;
         auto on(const connection_lost_event& e) -> next_states<disconnected_state>;

         void send_hello(session& peer);
      };

      struct connected_state : public container<connected_state> {
         void enter(session& peer);
         void on(const status_message& msg, session& peer_session);
         auto on(const connection_lost_event& e) -> next_states<disconnected_state>;
         void exit(session& peer);

         broadcast::state_machine_type broadcast_state_machine;
         receiver::state_machine_type  receiver_state_machine;

         using state_machine_member_list = std::tuple<
            container::member<decltype(broadcast_state_machine), &connected_state::broadcast_state_machine>,
            container::member<decltype(receiver_state_machine), &connected_state::receiver_state_machine>
         >;
      };

      using state_machine_type = machine<disconnected_state, handshaking_state, connected_state>;
   }

   class session : public container<session>, public std::enable_shared_from_this<session> {
   public:
      session(const connection_ptr& conn, shared_state& shared)
      :conn(conn)
      ,shared(shared)
      ,session_index(shared.reserve_session_index())
      {
         initialize();
      }

      ~session()
      {
         shutdown();
      }

      session() = delete;
      session(const session&) = delete;

      chain_info       chain;
      node_info        info;

      connection_ptr      conn;
      shared_state&       shared;
      int                 session_index;


      base::state_machine_type session_state_machine;
      using state_machine_member_list = std::tuple<
         container<session>::member<decltype(session_state_machine), &session::session_state_machine>
      >;
   };

} } // namespace eosio::net_v2