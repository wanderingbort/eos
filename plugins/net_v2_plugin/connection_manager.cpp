/**
 *  @file
 *  @copyright defined in eos/LICENSE.txt
 */
#include <eosio/net_v2/connection_manager.hpp>
#include <regex>
#include <chrono>

using namespace boost::system;
using namespace boost::asio::ip;
using namespace std;

static const auto host_port_regex = std::regex("^(\\[([^]]+)\\]|([^:]+)):([^:]+)$");



namespace eosio { namespace net_v2 {

   static void try_connect(const connection_ptr& c, tcp::resolver::iterator endpoint_itr) {
      auto next_endpoint = *endpoint_itr;
      ++endpoint_itr;

      c->close();
      c->socket = std::make_shared<tcp::socket>( c->mgr.ios );
      connection_wptr weak_c = c;
      c->socket->async_connect( next_endpoint, [endpoint_itr, weak_c] ( const error_code& err ) {
         if (weak_c.expired()) {
            return;
         }

         connection_ptr conn = weak_c.lock();

         if( err ) {
            conn->on_error(wrap_boost_err(err));

            if (endpoint_itr == tcp::resolver::iterator() ) {
               // set timer to re-try
            } else {
               try_connect(conn, endpoint_itr);
            }

         } else {
            conn->retry_attempts = 0;
            conn->on_connected();
         }
      });
   }

   void connection::open( bool auto_reconnect ) {
      reconnect = auto_reconnect;
      initiate();
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
            set_retry();
         } else {
            try_connect(conn, endpoint_itr);
         }
      });
   }

   void connection::set_retry() {
      if (reconnect && !reconnect_timer) {
         retry_attempts++;
         auto delay_s = std::min(std::pow(2, std::min(retry_attempts, 8), mgr.base_reconnect_delay_s), mgr.max_reconnect_delay_s);
         reconnect_timer.emplace(chrono::seconds(delay_s));

         connection_wptr weak_c = shared_from_this();
         reconnect_timer->async_wait([weak_c](const error_code& error){
            if (!weak_c.expired() && error != boost::asio::error::operation_aborted) {
               auto conn = weak_c.lock();
               conn->initiate();
            }
         });
      }
   }

   bool connection::enqueue(const net_message_ptr& msg, then_type<> then) {
      return false;
   }

   bool connection::enqueue(const std::shared_ptr<bytes>& raw, then_type<> then) {
      return false;
   }


   connection_ptr connection_manager::get( const string& host ){
      return connection_ptr(new connection( host, *this ));
   }

   void connection_manager::listen( string host )
   {

   }

}}