file(GLOB_RECURSE HEADERS "include/graphene/peerplays_sidechain/*.hpp")

add_library( peerplays_sidechain
             peerplays_sidechain_plugin.cpp
             sidechain_net_manager.cpp
             sidechain_net_handler.cpp
             sidechain_net_handler_bitcoin.cpp
             sidechain_net_handler_peerplays.cpp
             bitcoin/bech32.cpp
             bitcoin/bitcoin_address.cpp
             bitcoin/bitcoin_script.cpp
             bitcoin/bitcoin_transaction.cpp
             bitcoin/segwit_addr.cpp
             bitcoin/utils.cpp
             bitcoin/sign_bitcoin_transaction.cpp
           )

if (ENABLE_DEV_FEATURES)
    set(ENABLE_MULTIPLE_SONS 1)
    set(ENABLE_PEERPLAYS_ASSET_DEPOSITS 1)
endif()
unset(ENABLE_DEV_FEATURES)
unset(ENABLE_DEV_FEATURES CACHE)

if (ENABLE_MULTIPLE_SONS)
   message ("Multiple SONs per software instance are supported")
   target_compile_definitions(peerplays_sidechain PRIVATE ENABLE_MULTIPLE_SONS)
endif()
unset(ENABLE_MULTIPLE_SONS)
unset(ENABLE_MULTIPLE_SONS CACHE)

if (ENABLE_PEERPLAYS_ASSET_DEPOSITS)
   message ("Depositing Peerplays assets enabled")
   target_compile_definitions(peerplays_sidechain PRIVATE ENABLE_PEERPLAYS_ASSET_DEPOSITS)
endif()
unset(ENABLE_PEERPLAYS_ASSET_DEPOSITS)
unset(ENABLE_PEERPLAYS_ASSET_DEPOSITS CACHE)

target_link_libraries( peerplays_sidechain PRIVATE graphene_plugin zmq )
target_include_directories( peerplays_sidechain
                            PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/include" )

install( TARGETS
   peerplays_sidechain

   RUNTIME DESTINATION bin
   LIBRARY DESTINATION lib
   ARCHIVE DESTINATION lib
)
INSTALL( FILES ${HEADERS} DESTINATION "include/graphene/peerplays_sidechain" )

