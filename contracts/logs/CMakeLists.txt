add_contract(logs logs ${CMAKE_CURRENT_SOURCE_DIR}/src/logs.cpp)

target_include_directories(
   logs  
   PUBLIC 
   ${CMAKE_CURRENT_SOURCE_DIR}/include
)

set_target_properties(logs PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}")

configure_file( ${CMAKE_CURRENT_SOURCE_DIR}/ricardian/logs.contracts.md ${CMAKE_CURRENT_BINARY_DIR}/ricardian/logs.contracts.md @ONLY)

target_compile_options(logs PUBLIC -R${CMAKE_CURRENT_SOURCE_DIR}/ricardian -R${CMAKE_CURRENT_BINARY_DIR}/ricardian)
