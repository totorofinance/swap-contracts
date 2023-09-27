project(lppool)

set(EOSIO_WASM_OLD_BEHAVIOR "Off")
find_package(eosio.cdt)

add_contract( lppool lppool lppool.cpp )
target_include_directories( lppool PUBLIC ${CMAKE_SOURCE_DIR}/../include )
target_ricardian_directory( lppool ${CMAKE_SOURCE_DIR}/../ricardian )