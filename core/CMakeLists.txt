cmake_minimum_required(VERSION 3.5)

project(swap_contracts)

set(VERSION_MAJOR 1)
set(VERSION_MINOR 0)
set(VERSION_PATCH 0)

set(VERSION_FULL "${VERSION_MAJOR}.${VERSION_MINOR}.${VERSION_PATCH}")

include(ExternalProject)

find_package(eosio.cdt)

message(STATUS "Building swap contracts v${VERSION_FULL}")


if(CMAKE_BUILD_TYPE STREQUAL "Debug")
   set(TEST_BUILD_TYPE "Debug")
   set(CMAKE_BUILD_TYPE "Release")
else()
   set(TEST_BUILD_TYPE ${CMAKE_BUILD_TYPE})
endif()

ExternalProject_Add(
   contracts_project
   SOURCE_DIR ${CMAKE_SOURCE_DIR}/contracts
   BINARY_DIR ${CMAKE_BINARY_DIR}/contracts
   CMAKE_ARGS -DCMAKE_TOOLCHAIN_FILE=${EOSIO_CDT_ROOT}/lib/cmake/eosio.cdt/EosioWasmToolchain.cmake
   UPDATE_COMMAND ""
   PATCH_COMMAND ""
   TEST_COMMAND ""
   INSTALL_COMMAND ""
   BUILD_ALWAYS 1
)

if (APPLE)
   set(OPENSSL_ROOT "/usr/local/opt/openssl")
elseif (UNIX)
   set(OPENSSL_ROOT "/usr/include/openssl")
endif()
set(SECP256K1_ROOT "/usr/local")

if (APPLE)
   set(OPENSSL_ROOT "/usr/local/opt/openssl")
elseif (UNIX)
   set(OPENSSL_ROOT "/usr/include/openssl")
endif()
set(SECP256K1_ROOT "/usr/local")

string(REPLACE ";" "|" TEST_PREFIX_PATH "${CMAKE_PREFIX_PATH}")
string(REPLACE ";" "|" TEST_FRAMEWORK_PATH "${CMAKE_FRAMEWORK_PATH}")
string(REPLACE ";" "|" TEST_MODULE_PATH "${CMAKE_MODULE_PATH}")

set(BUILD_TESTS FALSE CACHE BOOL "Build unit tests")

if(BUILD_TESTS)
   message(STATUS "Building unit tests.")
   ExternalProject_Add(
     contracts_unit_tests
     LIST_SEPARATOR | # Use the alternate list separator
     CMAKE_ARGS -DCMAKE_BUILD_TYPE=${TEST_BUILD_TYPE} -DCMAKE_PREFIX_PATH=${TEST_PREFIX_PATH} -DCMAKE_FRAMEWORK_PATH=${TEST_FRAMEWORK_PATH} -DCMAKE_MODULE_PATH=${TEST_MODULE_PATH} -DEOSIO_ROOT=${EOSIO_ROOT} -DLLVM_DIR=${LLVM_DIR} -DBOOST_ROOT=${BOOST_ROOT}
     SOURCE_DIR ${CMAKE_SOURCE_DIR}/tests
     BINARY_DIR ${CMAKE_BINARY_DIR}/tests
     BUILD_ALWAYS 1
     TEST_COMMAND   ""
     INSTALL_COMMAND ""
   )
else()
   message(STATUS "Unit tests will not be built. To build unit tests, set BUILD_TESTS to true.")
endif()

