/**
 *  @file
 *  @copyright defined in eos/LICENSE.txt
 */
#pragma once

#include <eosio/net_v2/protocol.hpp>
#include <boost/signals2/signal.hpp>

namespace eosio { namespace net_v2 {

   using boost::signals2::signal;

   using serialized_net_message = bytes;
   using serialized_net_message_ptr = shared_ptr<serialized_net_message>;

   class lazy_serialized_net_message_ptr {
      public:
         virtual serialized_net_message_ptr get() const = 0;
   };

   class connection {
      public:
         virtual ~connection(){}
         virtual void close() = 0;

         /**
          * signals
          */
         using message_signal = signal<void(const net_message_ptr&, const lazy_serialized_net_message_ptr&)>;
         message_signal on_message;

         using disconnected_signal = signal<void()>;
         disconnected_signal on_disconnected;

         using connected_signal = signal<void()>;
         connected_signal on_connected;

         using error_signal = signal<void(const fc::exception_ptr&)>;
         error_signal on_error;

         /**
          * methods
          */
         using message_type = fc::static_variant<net_message_ptr, serialized_net_message_ptr>;
         using then_type = std::function<void()>;
         virtual bool enqueue(const message_type& message, then_type&& then) = 0;
         virtual bool enqueue(const message_type& message) = 0;

         // convenience method for lambda
         template<typename Then>
         bool enqueue(const message_type& message, Then then) {
            return enqueue(message, then_type(then));
         }
   };

   using connection_ptr = std::shared_ptr<connection>;

}}