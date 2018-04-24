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

#include <memory>


namespace eosio { namespace net_v2 {

   using boost::asio::ip::tcp;
   using boost::asio::ip::address_v4;
   using boost::asio::ip::address_v6;
   using boost::asio::ip::host_name;
   using boost::asio::io_service;
   using boost::asio::steady_timer;
   using boost::signals2::signal;
   using namespace boost::system;
   using namespace std;


   FC_DECLARE_EXCEPTION(net_v2_connection_exception, 0xA0000, "Connection Error");
   FC_DECLARE_DERIVED_EXCEPTION(net_v2_boost_error, net_v2_connection_exception, 0xA0001, "Boost returned an error code from a system call");

#define FC_MAKE_EXCEPTION_PTR( EXCEPTION, FORMAT, ...  ) \
   fc::exception_ptr(new EXCEPTION( FC_LOG_MESSAGE( error, FORMAT, __VA_ARGS__ ) ))

   inline fc::exception_ptr wrap_boost_err(const error_code& err) {
      return FC_MAKE_EXCEPTION_PTR(net_v2_boost_error, "${message}", ("message", err.message()));
   }

   template<typename ...Results>
   using result_signal_type = signal<void(const fc::exception_ptr& , Results...)>;

   template<typename ...Results>
   using then_type = const result_signal_type<Results...>::slot_type&;

   class connection_manager;
   class connection : public std::enable_shared_from_this<connection> {
      public:
         ~connection()
         {
            close();
         }

         void close();
         void open(bool auto_reconnect = false);

         using message_signal = signal<void(const net_message_ptr&)>;
         message_signal on_message;

         using disconnect_signal = signal<void()>;
         disconnect_signal on_disconnected;

         using disconnect_signal = signal<void()>;
         disconnect_signal on_connected;

         using error_signal = signal<void(const fc::exception_ptr&)>;
         error_signal on_error;

         bool enqueue( const net_message_ptr& msg, then_type<> then );
         bool enqueue( const shared_ptr<bytes>& raw, then_type<> then );

         using socket_ptr = std::unique_ptr<tcp::socket>;
         string                      endpoint;
         fc::optional<tcp::endpoint> resolved_endpoint;
         socket_ptr                  socket;

      private:
         connection(const string& endpoint, connection_manager& mgr)
         :endpoint(endpoint), mgr(mgr)
         {
         }

         void initiate();
         void set_retry();

         connection_manager&         mgr;
         bool                        reconnect = false;
         fc::optional<steady_timer>  reconnect_timer;
         int                         retry_attempts = 0;
         friend class connection_manager;
   };

   using connection_ptr = shared_ptr<connection>;
   using connection_wptr = weak_ptr<connection>;

   class connection_manager {
      public:
         explicit connection_manager(io_service& ios)
         :ios(ios)
         ,resolver(new tcp::resolver(ios))
         {
         }

         connection_ptr get( const string& host );

         void listen( string host );
         signal<void(const connection_ptr&)> on_incoming_connection;

      private:
         unique_ptr<tcp::acceptor>        acceptor;
         unique_ptr<tcp::resolver>        resolver;

         io_service&                      ios;
         friend class connection;

   };

} }