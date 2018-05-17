/**
 *  @file
 *  @copyright defined in eos/LICENSE.txt
 */
#pragma once
#include <eosio/net_v2/types.hpp>
#include <eosio/net_v2/transaction_cache.hpp>
#include <eosio/net_v2/block_cache.hpp>

#include <boost/asio/io_service.hpp>
#include <boost/asio/steady_timer.hpp>

#include <eosio/net_v2/state_machine.hpp>
#include <eosio/net_v2/connection.hpp>
#include <eosio/net_v2/protocol.hpp>

using boost::asio::steady_timer;

using namespace eosio::chain;
using namespace eosio::net_v2::state_machine;


namespace eosio { namespace net_v2 {
   using boost::asio::io_service;

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
         void enter(base::connected_state& connected, const session& peer);

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
            block_id_type last_block_sent;

            void enter(const desynced_state& parent, base::connected_state& connected, session& peer);
            void on(const sent_block_event& event, const desynced_state& parent, base::connected_state& connected, const session& peer);

            void send_next_best_block(session& peer);
         };

         /**
          * local-behind state : this peer is subscribed to the real-time broadcast but has more information
          * about the chain than we do locally.
          */
         struct local_behind_state {
            void on(const received_block_event& event, const desynced_state& parent, base::connected_state& connected, const session& peer);
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
      struct subscribed_state; struct delay_state;
      struct idle_state {
         auto on(const status_message& msg, const base::connected_state&, const session& peer) -> next_states<subscribed_state> ;
      };

      /**
       * Subscribed state: this peer is connected and subscribed.
       */
      struct subscribed_state {
         void enter(const base::connected_state& connected, session& peer_session);
         auto on(const subscription_refused_message& msg ) -> next_states<delay_state>;
         void exit(const base::connected_state& connected, session& peer_session);
      };

      struct delay_state {
         struct delay_timer_event{};

         void enter(const base::connected_state& connected, session& peer_session);
         auto on(const delay_timer_event&) -> next_states<idle_state>;

         optional<steady_timer> delay_timer;
      };

      using state_machine_type = machine<idle_state, subscribed_state, delay_state>;
   }

   namespace base {
      struct connection_established_event {};
      struct connection_lost_event {};
      struct status_timer_event {};

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
         auto on(const connection_lost_event& e) -> next_states<disconnected_state>;
      };

      struct connected_state : public container<connected_state> {
         void enter(session& peer);
         void on(const status_message& msg, session& peer_session);
         void on(const status_timer_event&, session& peer_session);
         auto on(const connection_lost_event& e) -> next_states<disconnected_state>;
         void exit(session& peer);

         broadcast::state_machine_type broadcast_state_machine;
         receiver::state_machine_type  receiver_state_machine;

         using state_machine_member_list = std::tuple<
            container::member<decltype(broadcast_state_machine), &connected_state::broadcast_state_machine>,
            container::member<decltype(receiver_state_machine), &connected_state::receiver_state_machine>
         >;

         void send_status(session& peer);
         optional<steady_timer> status_timer;
      };

      using state_machine_type = machine<disconnected_state, handshaking_state, connected_state>;
   }

   class session : public container<session>, public std::enable_shared_from_this<session> {
   public:
      session(io_service& ios, const connection_ptr& conn, shared_state& shared)
      :ios(ios)
      ,conn(conn)
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

      io_service          &ios;
      connection_ptr      conn;
      shared_state&       shared;
      int                 session_index;


      base::state_machine_type session_state_machine;
      using state_machine_member_list = std::tuple<
         container<session>::member<decltype(session_state_machine), &session::session_state_machine>
      >;
   };

} } // namespace eosio::net_v2