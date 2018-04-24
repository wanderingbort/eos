file(GLOB HEADERS "include/eosio/net_v2/*.hpp")
add_library( net_v2_plugin
        plugin.cpp
        session.cpp
        connection_manager.cpp
        ${HEADERS} )

target_link_libraries( net_v2_plugin appbase fc eosio_chain )
target_include_directories( net_v2_plugin PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include ${CMAKE_CURRENT_SOURCE_DIR}/../chain_interface/include )
