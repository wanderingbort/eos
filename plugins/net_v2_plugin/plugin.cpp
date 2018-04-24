/**
 *  @file
 *  @copyright defined in eos/LICENSE.txt
 */
#include <eosio/chain/types.hpp>
#include <eosio/net_v2/plugin.hpp>
#include <eosio/net_v2/protocol.hpp>
#include <eosio/net_v2/connection_manager.hpp>
#include <eosio/net_v2/session.hpp>
#include <eosio/chain/plugin_interface.hpp>

#include <fc/network/ip.hpp>
#include <fc/io/json.hpp>
#include <fc/io/raw.hpp>
#include <fc/log/appender.hpp>
#include <fc/container/flat.hpp>
#include <fc/reflect/variant.hpp>
#include <fc/crypto/rand.hpp>
#include <fc/exception/exception.hpp>

#include <boost/intrusive/set.hpp>
#include <boost/algorithm/string/predicate.hpp>

namespace fc {
   extern std::unordered_map<std::string,logger>& get_logger_map();
}

using namespace appbase;
using namespace eosio::chain;
using namespace eosio::chain::plugin_interface;
namespace bip = boost::interprocess;
using boost::asio::io_service;
using namespace std;
using namespace boost::algorithm;

namespace eosio { namespace net_v2 {

   static plugin& _plugin = app().register_plugin<plugin>();

   class plugin_impl : public std::enable_shared_from_this<plugin_impl> {
   public:
      plugin_impl(io_service& ios)
      : connections(ios)
      {}

      void connect( const string& endpoint);
      void disconnect(const string& endpoint);
      void post( const session_ptr& session, const net_message_ptr& msg);

      int16_t                       network_version = 0;
      shared_state                  shared;

      connection_manager            connections;
      std::vector<session_ptr>      sessions;

      string                        listen_endpoint;
      string                        public_endpoint;
      set<string>                   declared_peers;
      string                        agent_name;

      fc::logger                    logger;
   };

   using plugin_impl_wptr = std::weak_ptr<plugin_impl>;

   const fc::string logger_name("net_plugin_impl");
   fc::logger logger;

   plugin::plugin()
   {
   }

   plugin::~plugin() {
   }

   void plugin::set_program_options( options_description& /*cli*/, options_description& cfg )
   {
      cfg.add_options()
         ( "p2p-listen-endpoint", bpo::value<string>()->default_value( "0.0.0.0:9876" ), "The actual host:port used to listen for incoming p2p connections.")
         ( "p2p-server-address", bpo::value<string>(), "An externally accessible host:port for identifying this node. Defaults to p2p-listen-endpoint.")
         ( "p2p-peer-address", bpo::value< vector<string> >()->composing(), "The public endpoint of a peer node to connect to. Use multiple p2p-peer-address options as needed to compose a network.")
         ( "agent-name", bpo::value<string>()->default_value("\"EOS Test Agent\""), "The name supplied to identify this node amongst the peers.")
         ( "allowed-connection", bpo::value<vector<string>>()->multitoken()->default_value({"any"}, "any"), "Can be 'any' or 'producers' or 'specified' or 'none'. If 'specified', peer-key must be specified at least once. If only 'producers', peer-key is not required. 'producers' and 'specified' may be combined.")
         ( "log-level-net-plugin", bpo::value<string>()->default_value("info"), "Log level: one of 'all', 'debug', 'info', 'warn', 'error', or 'off'")
         ( "max-clients", bpo::value<int>()->default_value(0), "Maximum number of clients from which connections are accepted, use 0 for no limit")
         ( "connection-cleanup-period", bpo::value<int>()->default_value(10), "number of seconds to wait before cleaning up dead connections")
         ;
   }

   void plugin::plugin_initialize( const variables_map& options ) {
      ilog("Initialize net plugin");
      io_service& ios = app().get_io_service();

      my.reset(new plugin_impl(ios));

      if(options.count("p2p-listen-endpoint")) {
         my->listen_endpoint = options.at("p2p-listen-endpoint").as< string >();
      }

      if(options.count("p2p-server-address")) {
         my->public_endpoint = options.at("p2p-server-address").as< string >();
      } else {
         // if this is an any/address we should pick an appropriate address
         bool is_any_address =
                 starts_with(my->listen_endpoint, address_v4::any().to_string()) ||
                 starts_with(my->listen_endpoint, string("[") + address_v6::any().to_string());

         if(is_any_address) {
            boost::system::error_code ec;
            auto host = host_name(ec);
            if( ec.value() != boost::system::errc::success) {

               FC_THROW_EXCEPTION( fc::invalid_arg_exception,
                                   "Unable to retrieve host_name. ${msg}",( "msg",ec.message()));

            }
            auto port = my->listen_endpoint.substr( my->listen_endpoint.find_last_of(':'), my->listen_endpoint.size());
            my->public_endpoint = host + port;
         }
      }

      if(options.count("p2p-peer-address")) {
         auto peers =  options.at("p2p-peer-address").as<vector<string> >();
         my->declared_peers.insert(peers.begin(), peers.end());
      }

      if(options.count("agent-name")) {
         my->agent_name = options.at("agent-name").as<string>();
      }
   }

   void plugin::plugin_startup() {
      // get information about the local chain, and subscribe to methods
      my->shared.local_chain.head_block_id = app().get_method<methods::get_head_block_id>()();
      my->shared.local_chain.last_irreversible_block_number = app().get_method<methods::get_last_irreversible_block_number>()();

      if (!my->listen_endpoint.empty()) {
         my->connections.listen(my->listen_endpoint);
      }

      if(fc::get_logger_map().find("net_v2_plugin") != fc::get_logger_map().end())
         my->logger = fc::get_logger_map()[logger_name];


      for( auto seed_node : my->declared_peers ) {
         my->connect(seed_node);
      }

   }

   void plugin::plugin_shutdown() {

   }

   void plugin_impl::connect(const string& endpoint) {
      auto conn = connections.get( endpoint );
      sessions.emplace_back(conn, shared);
      session_wptr weak_session = sessions.back();

      conn->on_connected([weak_session]() {
         if (!weak_session.expired()) {
            weak_session.lock()->post(base::connection_established_event{});
         }
      });

      conn->on_disconnected([weak_session]() {
         if (!weak_session.expired()) {
            weak_session.lock()->post(base::connection_lost_event{});
         }
      });

      conn->on_message([weak_session, this](const net_message_ptr& msg) {
         if (!weak_session.expired()) {
            post(weak_session.lock(), msg);
         }
      });

      conn->on_error([weak_session, this](const fc::exception_ptr& e) {
         if (!weak_session.expired()) {
            auto session = weak_session.lock();
            fc_elog(logger, "failed to connect to ${endpoint}", ("endpoint", session->conn->endpoint));
            fc_dlog(logger, "${details}", ("details", e->to_detail_string()));
         }
      });

      conn->open();
   }

   void plugin_impl::disconnect(const string& endpoint) {
      auto iter = sessions.begin();
      while(iter != sessions.end()) {
         if ((*iter)->conn->endpoint == endpoint) {
            break;
         }
      }

      if (iter != sessions.end()) {
         auto last = std::prev(session.end());
         if (iter != last) {
            std::iter_swap(iter, last);
         }

         // discard the session
         sessions.pop_back();
      }
   }

   struct session_post_visitor : fc::visitor<void> {
      session_post_visitor(const session_ptr& session)
      :session(session)
      {}

      void operator()( const auto& msg ) {
         session->post(msg);
      }

      const session_ptr& session;
   };

   void plugin_impl::post( const session_ptr& session, const net_message_ptr& msg) {
      // maybe do something with this message and the greater chain?
      msg.visit(session_post_visitor(session));
   }



} }
