#include <eosio/net_v2/session.hpp>


namespace eosio { namespace net_v2 {

static fc::logger logger;

using namespace eosio::net_v2::state_machine;

namespace base {
   next_states<handshaking_state>
   disconnected_state::on(const connection_established_event&) {
      return next_state<handshaking_state>();
   }

   void handshaking_state::send_hello(session& peer) {
      auto msg = std::make_shared<net_message>(hello_message{
         .chain_id = peer.shared.local_chain.chain_id,
         .node_id = peer.shared.local_info.node_id,
         .p2p_address = peer.shared.local_info.public_endpoint,
#if defined( __APPLE__ )
         .os = "osx",
#elif defined( __linux__ )
         .os = "linux",
#elif defined( _MSC_VER )
         .os = "win32",
#else
         .os = "other",
#endif
         .agent = peer.shared.local_info.agent_name
      });

      session_wptr weak_peer = peer.shared_from_this();
      peer.conn->enqueue(msg, [msg, weak_peer](const fc::exception_ptr& err) {
         if (weak_peer.expired()) {
            return;
         }

         auto session_ptr = weak_peer.lock();
         if (err) {
            session_ptr->post(hello_failed_event());
         } else {
            session_ptr->post(hello_sent_event());
         }
      });
   }

   void handshaking_state::enter(session& peer) {
      send_hello(peer);
   }

   next_states<connected_state>
   handshaking_state::on(const hello_message& msg, session& peer) {
      handshake_received = true;

      peer.info.node_id = msg.node_id;
      peer.info.agent_name = msg.agent;
      peer.info.public_endpoint = msg.p2p_address;
      peer.chain.chain_id = msg.chain_id;

      if (handshake_sent) {
         return next_state<connected_state>();
      } else {
         return {};
      }
   }

   next_states<connected_state>
   handshaking_state::on(const hello_sent_event&) {
      handshake_sent = true;

      if (handshake_received) {
         return next_state<connected_state>();
      } else {
         return {};
      }
   }

   void
   handshaking_state::on(const hello_failed_event&, session& peer) {
      send_hello(peer);
   }

   next_states<disconnected_state>
   handshaking_state::on(const connection_lost_event&) {
      return next_state<disconnected_state>();
   };

   void connected_state::enter(session& peer) {
      initialize(peer);
   }

   next_states<disconnected_state>
   connected_state::on(const connection_lost_event&) {
      return next_state<disconnected_state>();
   };

   void connected_state::exit(session& peer) {
      shutdown(peer);
   }

   /**
    * This is a generic handler for status messges, it will fire *before* any state-specfic handler
    * @param msg
    */
   void connected_state::on(const status_message& msg, session& peer) {
      peer.chain.head_block_id = msg.head_block_id;
      peer.chain.last_irreversible_block_number = msg.last_irreversible_block_number;
   }
}

namespace broadcast {
   /**
    * On a new subscription message, determine which of the subscribed states to transition to
    * @param msg
    * @param local
    * @param peer
    */
   next_states<desynced_state>
   idle_state::on(const subscribe_message& msg) {
      return next_state<desynced_state>();
   }

   void desynced_state::enter(const base::connected_state& parent, const session& peer) {
      const auto& local_chain = peer.shared.local_chain;
      auto local_lib = local_chain.last_irreversible_block_number;
      auto peer_lib = peer.chain.last_irreversible_block_number;
      const auto& local_head_id = local_chain.head_block_id;
      const auto& peer_head_id = peer.chain.head_block_id;

      if (local_lib > peer_lib) {
         sub_state.initialize<peer_behind_state>(parent, peer);
      } else if (local_lib < peer_lib || peer_head_id != local_head_id) {
         sub_state.initialize<local_behind_state>(parent, peer);
      } else {
         post(completed_event{}, parent);
         // TODO: determine if the peer is on the same fork as the local
         // if they are on the same fork they may be ahead of us
         // if they are on a different fork then we will consider them "behind" us for no
         // otherwise we are in sync
      }
   }

   next_states<idle_state>
   desynced_state::on(const unsubscribe_message& msg) {
      return next_state<idle_state>();
   }

   next_states<subscribed_state>
   desynced_state::on(const completed_event& event) {
      return next_state<subscribed_state>();
   }

   void desynced_state::exit(const base::connected_state& parent, const session& peer) {

   }


   /**
    * When we send a block to a peer, determine if we have sent enough blocks to consider
    * this peer in-sync
    *
    * @param event
    * @param local
    * @param peer
    */
   void desynced_state::peer_behind_state::on(const sent_block_event& event, desynced_state& parent, const base::connected_state& connected, const session& peer) {
      const auto& local_chain = peer.shared.local_chain;
      const auto& local_head_id = local_chain.head_block_id;
      if (event.id == local_head_id) {
         // after we sent our head, we can turn on real-time syncing
         parent.post(completed_event{}, peer);
      } else {
         // select next best block to send
         // we may have changed forks
         // they may have changed forks or synced from someone else faster than us
         // get them 1 block closer to our head
      }
   }

   /**
    * As we receive blocks, determine when we can transition to real-time broadcast
    * @param event
    * @param local
    * @param peer
    */
   void desynced_state::local_behind_state::on(const received_block_event& event, desynced_state& parent, const base::connected_state& connected, const session& peer) {
      const auto& local_chain = peer.shared.local_chain;
      auto local_lib = local_chain.last_irreversible_block_number;
      auto peer_lib = peer.chain.last_irreversible_block_number;
      const auto& peer_head_id = peer.chain.head_block_id;

      // TODO: check to see if our fork has changed, if so, we may want to now consider our peer as behind

      if (event.id == peer_head_id) {
         // after we received their head, we can turn on real-time syncing
         parent.post(completed_event{}, peer);
      }
   }


   next_states<idle_state>
   subscribed_state::on(const unsubscribe_message& msg) {
      return next_state<idle_state>();
   }


   /**
    * When we have a block to broadcast, send it to all peers who are in-sync
    * @param event
    * @param local
    * @param peer
    */
   void subscribed_state::on(const broadcast_block_event& event, base::connected_state&, session& peer) {
      if (!event.entry.session_acks.at(peer.session_index)) {
         // send the block to this peer
      }
   }

   /**
    * When we have a transaction to broadcast, send it to all peers who are in-sync
    * @param event
    * @param local
    * @param peer
    */
   void subscribed_state::on(const broadcast_transaction_event& event, base::connected_state&, session& peer) {
      if (!event.entry.session_acks.at(peer.session_index)) {
         // send the transaction to this peer
      }
   }
}

namespace receiver {

   next_states<subscribed_state>
   idle_state::on(const status_message& msg) {
      // determine if this is an interesting peer and subscribe to them
      return next_state<subscribed_state>();
   }

   void subscribed_state::enter(base::connected_state& connected, const session& peer) {
      // send a subscribe
   }

   void subscribed_state::exit(base::connected_state& connected, const session& peer) {
      // send unsubscribe
   }

}
}} // namespace eosio::net_v2
