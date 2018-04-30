/**
 *  @file
 *  @copyright defined in eos/LICENSE.txt
 */
#include <eosio/net_v2/connection_manager.hpp>
#include <boost/asio/buffers_iterator.hpp>
#include <boost/asio/write.hpp>
#include <regex>
#include <chrono>

using namespace boost::system;
using namespace boost::asio;
using namespace ip;
using namespace std;

static const auto host_port_regex = std::regex("^(\\[([^\\]]+)\\]|([^:]+)):([^:]+)$");



namespace eosio { namespace net_v2 {

   /*
    *  @brief datastream adapter that adapts message_buffer for use with fc unpack
    *
    *  This class supports unpack functionality but not pack.
    *
    *  NOTE: this version will not actully consume the data from the message buffer
    */
   template <uint32_t Length>
   class peek_mb_datastream {
      public:
         using message_buffer_type = message_buffer<Length>;
         using index_type = typename message_buffer_type::index_t;
         explicit peek_mb_datastream( message_buffer_type& m )
         :mb(m)
         ,bytes_read(0)
         ,index(mb.read_index())
         {}

         inline void skip( size_t s ) {
            message_buffer_type::advance_index(index, s);
         }

         inline bool read( char* d, size_t s ) {
            uint32_t bytes_remaining = mb.bytes_to_read() - bytes_read;
            if (bytes_remaining >= s) {
               mb.peek(d, s, index);
               bytes_read += s;
               return true;
            }
            fc::detail::throw_datastream_range_error( "read", bytes_remaining, (uint32_t)s - bytes_remaining);
         }

         inline bool get( unsigned char& c ) {
            return read(reinterpret_cast<char *>(&c), 1);
         }

         inline bool get( char& c ) {
            return read(&c, 1);
         }

      private:
         message_buffer_type&  mb;
         uint32_t              bytes_read;
         index_type            index;
   };

   template<uint32_t Length>
   auto make_peek_mb_datastream( message_buffer<Length>& mb ) {
      return peek_mb_datastream<Length>(mb);
   }

   struct data_buffer_visitor : fc::visitor<data_buffer_ptr> {
      data_buffer_ptr operator()(const data_buffer_ptr& ptr) {
         return ptr;
      }

      data_buffer_ptr operator()(const net_message_ptr& msg) {
         auto res = std::make_shared<bytes>();
         size_t size = fc::raw::pack_size(*msg);
         res->resize(size);
         datastream<char*> ds( res->data(), size );
         fc::raw::pack(ds, *msg);
         return res;
      }
   };

   void connection::try_connect(const connection_ptr& c, tcp::resolver::iterator endpoint_itr) {
      auto next_endpoint = *endpoint_itr;
      ++endpoint_itr;

      c->close();
      c->socket.reset(new tcp::socket( c->mgr.ios ));
      c->resolved_endpoint = next_endpoint;
      connection_wptr weak_c = c;
      c->socket->async_connect( next_endpoint, [endpoint_itr, weak_c] ( const error_code& err ) {
         if (weak_c.expired()) {
            return;
         }

         connection_ptr conn = weak_c.lock();

         if( err ) {
            conn->on_error(wrap_boost_err(err));

            if (endpoint_itr == tcp::resolver::iterator() ) {
               conn->set_retry();
            } else {
               try_connect(conn, endpoint_itr);
            }

         } else {
            conn->retry_attempts = 0;
            conn->on_connected();
            read_next(conn);
         }
      });
   }

   void connection::open( ) {
      reconnect = true;
      if (!socket && !reconnect_timer) {
         initiate();
      }
   }

   void connection::close() {
      reconnect = false;
      if (socket) {
         socket->close();
         socket.reset();
         on_disconnected();
      }
   }

   void connection::initiate() {
      std::smatch url_match;
      if (!std::regex_match(endpoint, url_match, host_port_regex)) {
         on_error(FC_MAKE_EXCEPTION_PTR(net_v2_connection_exception, "Invalid peer address. must be \"host:port\": ${p}", ("p",endpoint)));
         return;
      }

      tcp::resolver::query query( url_match[2].length() > 0 ? url_match[2].str() : url_match[3].str(), url_match[4].str() );

      connection_wptr weak_c = shared_from_this();
      mgr.resolver->async_resolve( query, [weak_c](const error_code& err, tcp::resolver::iterator endpoint_itr ){
         if (weak_c.expired()) {
            return;
         }

         connection_ptr conn = weak_c.lock();

         if( err ) {
            conn->on_error(wrap_boost_err(err));
            conn->set_retry();
         } else {
            try_connect(conn, endpoint_itr);
         }
      });
   }

   void connection::set_retry() {
      if (reconnect && !reconnect_timer) {
         auto delay_s = std::min((0x1 << std::min(retry_attempts++, 8)) * mgr.base_reconnect_delay_s, mgr.max_reconnect_delay_s);
         reconnect_timer.emplace(mgr.ios, std::chrono::seconds(delay_s));
         socket.reset();

         connection_wptr weak_c = shared_from_this();
         reconnect_timer->async_wait([weak_c](const error_code& error){
            if (!weak_c.expired() && error != error::operation_aborted) {
               auto conn = weak_c.lock();
               conn->initiate();
            }
         });
      }
   }

   void connection::handle_error() {
      on_disconnected();
      set_retry();
   }

   bool connection::enqueue(const net_message_ptr& msg, then_type<> then) {
      queued_writes.emplace_back(msg, then);
      if (queued_writes.size() == 1) {
         write_next(shared_from_this());
      }
      return true;
   }

   bool connection::enqueue(const std::shared_ptr<bytes>& raw, then_type<> then) {
      queued_writes.emplace_back(raw, then);
      if (queued_writes.size() == 1) {
         write_next(shared_from_this());
      }
      return true;
   }

   void connection::write_next(const connection_ptr& c) {
      if (c->queued_writes.empty() || !c->socket) {
         return;
      }

      auto header_ptr = std::make_shared<uint32_t>();
      auto data_ptr = std::get<0>(c->queued_writes.front()).visit(data_buffer_visitor());
      *header_ptr = data_ptr->size();

      std::array<const_buffer, 2> write_buffers = {{
         const_buffer(header_ptr.get(), sizeof(uint32_t)),
         const_buffer(data_ptr->data(), data_ptr->size())
      }};

      connection_wptr weak_c = c;
      async_write(*c->socket, write_buffers, [weak_c, header_ptr, data_ptr](const error_code& err, size_t) {
         if (weak_c.expired()) {
            return;
         }

         auto c = weak_c.lock();
         const auto& entry = c->queued_writes.front();
         if (err) {
            std::get<1>(entry)(wrap_boost_err(err));
         } else {
            std::get<1>(entry)(nullptr);
         }
         c->queued_writes.pop_front();
         connection::write_next(c);
      });
   }

   void connection::read_next(const connection_ptr& c) {
      if (!c->socket) {
         return;
      }

      auto mutable_buffers = c->queued_reads.get_buffer_sequence_for_boost_async_read();
      connection_wptr weak_c = c;
      c->socket->async_read_some(mutable_buffers, [weak_c](const error_code& err, size_t read_size){
         if (weak_c.expired()) {
            return;
         }

         auto c = weak_c.lock();
         if (err) {
            c->on_error(wrap_boost_err(err));
            c->handle_error();
         } else {
            if(read_size > c->queued_reads.bytes_to_write()) {
               c->on_error(FC_MAKE_EXCEPTION_PTR(net_v2_connection_exception, "async_read_some read too much"));
               c->handle_error();
               return;
            }

            c->queued_reads.advance_write_ptr(read_size);
            while (c->queued_reads.bytes_to_read() > 0 && c->read_message()) {
               // spinning on conditions
            }

            read_next(c);
         }
      });
   }

   bool connection::read_message() {
      const size_t message_header_size = sizeof(uint32_t);

      uint32_t bytes_in_buffer = queued_reads.bytes_to_read();

      if (bytes_in_buffer < message_header_size) {
         return false;
      }

      uint32_t message_length = 0;
      auto index = queued_reads.read_index();
      queued_reads.peek(&message_length, message_header_size, index);

      // enforce max_message_length
      if(message_length > mgr.max_message_length) {
         on_error(FC_MAKE_EXCEPTION_PTR(net_v2_connection_exception, "incoming message is too large"));
         handle_error();
         return false;
      }

      // extend the read buffer if its not large enough
      if (bytes_in_buffer < message_length + message_header_size) {
         queued_reads.add_space((uint32_t(message_length + message_header_size - bytes_in_buffer)));
         return false;
      }

      // process a message
      queued_reads.advance_read_ptr(message_header_size);
      auto ds = make_peek_mb_datastream(queued_reads);
      net_message_ptr msg = std::make_shared<net_message>();
      fc::raw::unpack(ds, *msg);

      on_message(msg, lazy_data_buffer_ptr(queued_reads, message_length));

      queued_reads.advance_read_ptr(message_length);

      return true;
   }

   connection_ptr connection_manager::get( const string& host ){
      return connection_ptr(new connection( host, *this ));
   }

   void connection_manager::listen( string endpoint )
   {
      std::smatch url_match;
      if (!std::regex_match(endpoint, url_match, host_port_regex)) {
         FC_THROW_EXCEPTION(net_v2_connection_exception, "Invalid listen address. must be \"host:port\": ${p}", ("p",endpoint));
      }

      tcp::resolver::query query( url_match[2].length() > 0 ? url_match[2].str() : url_match[3].str(), url_match[4].str() );
      resolved_listen_endpoint = *resolver->resolve( query);
      acceptor->open(resolved_listen_endpoint->protocol());
      acceptor->set_option(tcp::acceptor::reuse_address(true));
      acceptor->bind(*resolved_listen_endpoint);
      acceptor->listen();
      accept_next();
   }

   void connection_manager::accept_next() {
      auto socket = std::make_unique<tcp::socket>(ios);
      acceptor->async_accept( *socket, [socket = std::move(socket),this]( boost::system::error_code ec ) mutable {
         if( !ec ) {
            // todo check for max connections
            auto endpoint = socket->remote_endpoint();
            string host = endpoint.address().to_string();
            if (endpoint.address().is_v6()) {
               host = string("[") + host + "]";
            }

            host += ":" + std::to_string(endpoint.port());

            auto conn = connection_ptr(new connection(std::move(socket), host, *this));
            connection::read_next(conn);
            on_incoming_connection(conn);

            accept_next();
         } else {
            on_error(wrap_boost_err(ec));
         }
      });
   }

}}