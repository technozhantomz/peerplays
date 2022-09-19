file(GLOB HEADERS "include/sha3/*.h")

add_library( sha3
   memzero.c
   sha3.c
)

target_include_directories( sha3 PUBLIC ${CMAKE_CURRENT_LIST_DIR}/include )
target_compile_definitions( sha3 PUBLIC USE_KECCAK=1 )

install( TARGETS
   sha3
   RUNTIME DESTINATION bin
   LIBRARY DESTINATION lib
   ARCHIVE DESTINATION lib
)
