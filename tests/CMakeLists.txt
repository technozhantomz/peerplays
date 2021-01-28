find_package( Gperftools QUIET )
if( GPERFTOOLS_FOUND )
    message( STATUS "Found gperftools; compiling tests with TCMalloc")
    list( APPEND PLATFORM_SPECIFIC_LIBS tcmalloc )
endif()

file(GLOB COMMON_SOURCES "common/*.cpp")
add_library(graphene_tests_common "${COMMON_SOURCES}" )
target_link_libraries( graphene_tests_common
                       PUBLIC graphene_app graphene_egenesis_none )

file(GLOB UNIT_TESTS "tests/*.cpp")
add_executable( chain_test ${UNIT_TESTS} )
target_link_libraries( chain_test PRIVATE graphene_wallet graphene_tests_common ${PLATFORM_SPECIFIC_LIBS} )
if(MSVC)
  set_source_files_properties( tests/serialization_tests.cpp PROPERTIES COMPILE_FLAGS "/bigobj" )
endif(MSVC)

file(GLOB PERFORMANCE_TESTS "performance/*.cpp")
add_executable( performance_test ${PERFORMANCE_TESTS} )
target_link_libraries( performance_test PRIVATE graphene_tests_common ${PLATFORM_SPECIFIC_LIBS} )

file(GLOB BENCH_MARKS "benchmarks/*.cpp")
add_executable( chain_bench ${BENCH_MARKS} )
target_link_libraries( chain_bench PRIVATE graphene_tests_common ${PLATFORM_SPECIFIC_LIBS} )

file(GLOB APP_SOURCES "app/*.cpp")
add_executable( app_test ${APP_SOURCES} )
target_link_libraries( app_test PRIVATE graphene_tests_common graphene_witness ${PLATFORM_SPECIFIC_LIBS} )

file(GLOB INTENSE_SOURCES "intense/*.cpp")
add_executable( intense_test ${INTENSE_SOURCES} )
target_link_libraries( intense_test PRIVATE graphene_tests_common ${PLATFORM_SPECIFIC_LIBS} )

file(GLOB BETTING_TESTS "betting/*.cpp")
add_executable( betting_test ${BETTING_TESTS} )
target_link_libraries( betting_test PRIVATE graphene_tests_common ${PLATFORM_SPECIFIC_LIBS} )

file(GLOB PEERPLAYS_SIDECHAIN_TESTS "peerplays_sidechain/*.cpp")
add_executable( peerplays_sidechain_test ${PEERPLAYS_SIDECHAIN_TESTS} )
target_link_libraries( peerplays_sidechain_test PRIVATE graphene_tests_common peerplays_sidechain ${PLATFORM_SPECIFIC_LIBS} )

file(GLOB TOURNAMENT_TESTS "tournament/*.cpp")
add_executable( tournament_test ${TOURNAMENT_TESTS} )
target_link_libraries( tournament_test PRIVATE graphene_tests_common ${PLATFORM_SPECIFIC_LIBS} )

file(GLOB RANDOM_SOURCES "random/*.cpp")
add_executable( random_test ${RANDOM_SOURCES} )
target_link_libraries( random_test PRIVATE graphene_tests_common ${PLATFORM_SPECIFIC_LIBS} )

file(GLOB CLI_SOURCES "cli/*.cpp")
add_executable( cli_test ${CLI_SOURCES} )
if(WIN32)
    list(APPEND PLATFORM_SPECIFIC_LIBS ws2_32)
endif()
target_link_libraries( cli_test PRIVATE graphene_wallet graphene_tests_common graphene_witness ${PLATFORM_SPECIFIC_LIBS} )
if(MSVC)
    set_source_files_properties( cli/main.cpp PROPERTIES COMPILE_FLAGS "/bigobj" )
endif(MSVC)

file(GLOB ES_SOURCES "elasticsearch/*.cpp")
add_executable( es_test ${ES_SOURCES} )
target_link_libraries( es_test PRIVATE graphene_tests_common )

add_subdirectory( generate_empty_blocks )
