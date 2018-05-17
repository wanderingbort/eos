/**
 *  @file
 *  @copyright defined in eos/LICENSE.txt
 */
#pragma once

#include <eosio/net_v2/connection.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/ip/host_name.hpp>
#include <boost/asio/io_service.hpp>
#include <boost/asio/steady_timer.hpp>
#include <fc/exception/exception.hpp>
#include <fc/network/message_buffer.hpp>

#include <memory>


namespace eosio { namespace net_v2 {

   using boost::asio::ip::tcp;
   using boost::asio::ip::address_v4;
   using boost::asio::ip::address_v6;
   using boost::asio::ip::host_name;
   using boost::asio::io_service;
   using boost::asio::steady_timer;
   using boost::system::error_code;
   using fc::message_buffer;
   using namespace std;

   FC_DECLARE_EXCEPTION(net_v2_connection_exception, 0xA0000, "Connection Error");
   FC_DECLARE_DERIVED_EXCEPTION(net_v2_boost_error, net_v2_connection_exception, 0xA0001, "Boost returned an error code from a system call");

#define FC_MAKE_EXCEPTION_PTR( EXCEPTION, FORMAT, ...  ) \
   fc::exception_ptr(new EXCEPTION( FC_LOG_MESSAGE( error, FORMAT, __VA_ARGS__ ) ))

   inline fc::exception_ptr wrap_boost_err(const error_code& err) {
      return FC_MAKE_EXCEPTION_PTR(net_v2_boost_error, "${message}", ("message", err.message()));
   }

   class tcp_connection_manager;
   class tcp_connection;
   using tcp_connection_ptr = shared_ptr<tcp_connection>;
   using tcp_connection_wptr = weak_ptr<tcp_connection>;
   using message_buffer_type = message_buffer<1024*1024>;

   class tcp_lazy_serialized_net_message_ptr final : public lazy_serialized_net_message_ptr {
      public:
         serialized_net_message_ptr get() const {
            auto res = std::make_shared<serialized_net_message>();
            res->resize(size);

            auto read_index = mb.read_index();
            mb.peek(res->data(), res->size(), read_index);
            return res;
         }

      private:
         tcp_lazy_serialized_net_message_ptr(const message_buffer_type& mb, uint32_t size)
         :mb(mb)
         ,size(size)
         {
         }

         const message_buffer_type& mb;
         uint32_t                   size;

         friend class tcp_connection;
   };

   class tcp_connection final : public connection, public std::enable_shared_from_this<tcp_connection>  {
      public:
         ~tcp_connection()
         {
            close();
         }

         void close() override;
         void open();


         bool enqueue(const message_type& message, then_type&& then) override {
            if (!socket) {
               return false;
            }

            queued_writes.emplace_back(message, std::forward<then_type>(then));
            if (queued_writes.size() == 1) {
               write_next(shared_from_this());
            }
            return true;
         }

         bool enqueue(const message_type& message) override {
            if (!socket) {
               return false;
            }

            queued_writes.emplace_back(message, fc::optional<then_type>());
            if (queued_writes.size() == 1) {
               write_next(shared_from_this());
            }
            return true;
         }

         string                      endpoint;
         fc::optional<tcp::endpoint> resolved_endpoint;

      private:
         tcp_connection(const string& endpoint, tcp_connection_manager& mgr)
         :endpoint(endpoint), mgr(mgr)
         {
         }

         using socket_ptr = std::unique_ptr<tcp::socket>;
         tcp_connection(socket_ptr&& socket, const string& endpoint, tcp_connection_manager& mgr)
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

         tcp_connection_manager&         mgr;
         bool                            reconnect = false;
         fc::optional<steady_timer>      reconnect_timer;
         int                             retry_attempts = 0;
         socket_ptr                      socket;

         using queued_write = std::tuple<message_type, fc::optional<then_type>>;

         std::deque<queued_write>        queued_writes;
         message_buffer_type             queued_reads;

         static void write_next(const connection_ptr& c);
         static void read_next(const connection_ptr& c);

         friend class tcp_connection_manager;
   };


   class tcp_connection_manager {
      public:
         explicit tcp_connection_manager(io_service& ios)
         :ios(ios)
         ,acceptor(new tcp::acceptor(ios))
         ,resolver(new tcp::resolver(ios))
         {
         }

         tcp_connection_ptr get( const string& host );

         void listen( string host );
         signal<void(const tcp_connection_ptr&)> on_incoming_connection;
         connection::error_signal                on_error;

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