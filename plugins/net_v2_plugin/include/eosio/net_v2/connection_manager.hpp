/**
 *  @file
 *  @copyright defined in eos/LICENSE.txt
 */
#pragma once

#include <eosio/net_v2/protocol.hpp>
#include <boost/signals2/signal.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/ip/host_name.hpp>
#include <boost/asio/io_service.hpp>
#include <boost/asio/steady_timer.hpp>
#include <fc/exception/exception.hpp>
#include <fc/network/message_buffer.hpp>

#include <memory>

using boost::asio::ip::tcp;
using boost::asio::ip::address_v4;
using boost::asio::ip::address_v6;
using boost::asio::ip::host_name;
using boost::asio::io_service;
using boost::asio::steady_timer;
using boost::signals2::signal;
using boost::system::error_code;
using fc::message_buffer;
using namespace std;

namespace eosio { namespace net_v2 {

   FC_DECLARE_EXCEPTION(net_v2_connection_exception, 0xA0000, "Connection Error");
   FC_DECLARE_DERIVED_EXCEPTION(net_v2_boost_error, net_v2_connection_exception, 0xA0001, "Boost returned an error code from a system call");

#define FC_MAKE_EXCEPTION_PTR( EXCEPTION, FORMAT, ...  ) \
   fc::exception_ptr(new EXCEPTION( FC_LOG_MESSAGE( error, FORMAT, __VA_ARGS__ ) ))

   inline fc::exception_ptr wrap_boost_err(const error_code& err) {
      return FC_MAKE_EXCEPTION_PTR(net_v2_boost_error, "${message}", ("message", err.message()));
   }

   using then_type = const signal<void()>::slot_type&;

   using error_signal = signal<void(const fc::exception_ptr&)>;

   class connection_manager;
   class connection;
   using connection_ptr = shared_ptr<connection>;
   using connection_wptr = weak_ptr<connection>;
   using data_buffer_ptr = shared_ptr<bytes>;
   using message_buffer_type = message_buffer<1024*1024>;

   class lazy_data_buffer_ptr {
      public:

         explicit operator data_buffer_ptr() const {
            auto res = std::make_shared<bytes>();
            res->resize(size);

            auto read_index = mb.read_index();
            mb.peek(res->data(), res->size(), read_index);
            return res;
         }

      private:
         lazy_data_buffer_ptr(const message_buffer_type& mb, uint32_t size)
         :mb(mb)
         ,size(size)
         {
         }

         const message_buffer_type& mb;
         uint32_t                   size;

         friend class connection;
   };

   class connection : public std::enable_shared_from_this<connection> {
      public:
         ~connection()
         {
            close();
         }

         void close();
         void open();

         using message_signal = signal<void(const net_message_ptr&, const lazy_data_buffer_ptr&)>;
         message_signal on_message;

         using disconnected_signal = signal<void()>;
         disconnected_signal on_disconnected;

         using connected_signal = signal<void()>;
         connected_signal on_connected;

         error_signal on_error;


         template<typename Type, typename Functor>
         bool enqueue(const Type& entry, Functor&& then) {
            if (!socket) {
               return false;
            }

            queued_writes.emplace_back(entry, std::forward<Functor>(then));
            if (queued_writes.size() == 1) {
               write_next(shared_from_this());
            }
            return true;
         }

         template<typename Type>
         bool enqueue(const Type& entry) {
            if (!socket) {
               return false;
            }

            queued_writes.emplace_back(entry, fc::optional<std::decay_t<then_type>>());
            if (queued_writes.size() == 1) {
               write_next(shared_from_this());
            }
            return true;
         }

         string                      endpoint;
         fc::optional<tcp::endpoint> resolved_endpoint;

      private:
         connection(const string& endpoint, connection_manager& mgr)
         :endpoint(endpoint), mgr(mgr)
         {
         }

         using socket_ptr = std::unique_ptr<tcp::socket>;
         connection(socket_ptr&& socket, const string& endpoint, connection_manager& mgr)
         : endpoint(endpoint)
         , mgr(mgr)
         , socket(std::move(socket))
         {
         }

         void initiate();
         void set_retry();
         void handle_error();
         bool read_message();

         static void try_connect(const connection_ptr& c, tcp::resolver::iterator endpoint_itr);

         connection_manager&         mgr;
         bool                        reconnect = false;
         fc::optional<steady_timer>  reconnect_timer;
         int                         retry_attempts = 0;
         socket_ptr                  socket;

         using payload_type = fc::static_variant<net_message_ptr, data_buffer_ptr>;
         using queued_write = std::tuple<payload_type, fc::optional<std::decay_t<then_type>>>;

         std::deque<queued_write>      queued_writes;
         message_buffer_type           queued_reads;

         static void write_next(const connection_ptr& c);
         static void read_next(const connection_ptr& c);

         friend class connection_manager;
   };


   class connection_manager {
      public:
         explicit connection_manager(io_service& ios)
         :ios(ios)
         ,acceptor(new tcp::acceptor(ios))
         ,resolver(new tcp::resolver(ios))
         {
         }

         connection_ptr get( const string& host );

         void listen( string host );
         signal<void(const connection_ptr&)> on_incoming_connection;
         error_signal                        on_error;

      private:
         void accept_next();

         io_service&                      ios;
         unique_ptr<tcp::acceptor>        acceptor;
         unique_ptr<tcp::resolver>        resolver;

         fc::optional<tcp::endpoint>      resolved_listen_endpoint;

         int base_reconnect_delay_s = 1;
         int max_reconnect_delay_s = 300;
         int max_message_length = 10 * 1024 * 1024;

         friend class connection;

   };

} }