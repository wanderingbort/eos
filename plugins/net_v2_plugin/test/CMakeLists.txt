file(GLOB HEADERS "include/eosio/net_v2/*.hpp")
add_library( net_v2_mock_chain_plugin
        mock_chain_plugin.cpp
        ${HEADERS} )

target_link_libraries( net_v2_mock_chain_plugin appbase fc eosio_chain )
target_include_directories( net_v2_mock_chain_plugin PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include ${CMAKE_CURRENT_SOURCE_DIR}/../../chain_interface/include )


add_executable( net_test main.cpp )
if( UNIX AND NOT APPLE )
  set(rt_library rt )
endif()

find_package( Gperftools QUIET )
if( GPERFTOOLS_FOUND )
    message( STATUS "Found gperftools; compiling nodeos with TCMalloc")
    list( APPEND PLATFORM_SPECIFIC_LIBS tcmalloc )
endif()

target_include_directories(net_test PUBLIC ${CMAKE_CURRENT_BINARY_DIR})

if(UNIX)
  if(APPLE)
    set(whole_archive_flag "-force_load")
    set(no_whole_archive_flag "")
  else()
    set(whole_archive_flag "--whole-archive")
    set(no_whole_archive_flag "--no-whole-archive")
  endif()
else()
  set(whole_archive_flag "--whole-archive")
  set(no_whole_archive_flag "--no-whole-archive")
endif()

target_link_libraries( net_test
        PRIVATE appbase
        PRIVATE -Wl,${whole_archive_flag} net_v2_plugin -Wl,${no_whole_archive_flag}
        PRIVATE -Wl,${whole_archive_flag} net_v2_mock_chain_plugin -Wl,${no_whole_archive_flag}
        PRIVATE eosio_chain fc ${CMAKE_DL_LIBS} ${PLATFORM_SPECIFIC_LIBS} )

