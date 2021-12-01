file(GLOB HEADERS "include/graphene/app/*.hpp")
file(GLOB EGENESIS_HEADERS "../egenesis/include/graphene/app/*.hpp")

add_library( graphene_app 
             api.cpp
             application.cpp
             config_util.cpp
             database_api.cpp
             plugin.cpp
             ${HEADERS}
             ${EGENESIS_HEADERS}
           )

# need to link graphene_debug_witness because plugins aren't sufficiently isolated #246
#target_link_libraries( graphene_app graphene_market_history graphene_account_history graphene_chain fc graphene_db graphene_net graphene_utilities graphene_debug_witness )
target_link_libraries( graphene_app
                       PUBLIC graphene_net graphene_utilities
                       graphene_account_history graphene_accounts_list graphene_affiliate_stats graphene_bookie graphene_debug_witness graphene_elasticsearch graphene_es_objects graphene_generate_genesis graphene_market_history )

target_include_directories( graphene_app
                            PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/include"
                            "${CMAKE_CURRENT_SOURCE_DIR}/../egenesis/include" )

if(MSVC)
  set_source_files_properties( application.cpp api.cpp database_api.cpp PROPERTIES COMPILE_FLAGS "/bigobj" )
endif(MSVC)

INSTALL( TARGETS
   graphene_app

   RUNTIME DESTINATION bin
   LIBRARY DESTINATION lib
   ARCHIVE DESTINATION lib
)
INSTALL( FILES ${HEADERS} DESTINATION "include/graphene/app" )



add_library( graphene_plugin
             plugin.cpp

             include/graphene/app/plugin.hpp
           )

target_link_libraries( graphene_plugin
                       PUBLIC graphene_net graphene_utilities )

target_include_directories( graphene_plugin
                            PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/include" )

INSTALL( TARGETS
   graphene_app

   RUNTIME DESTINATION bin
   LIBRARY DESTINATION lib
   ARCHIVE DESTINATION lib
)

