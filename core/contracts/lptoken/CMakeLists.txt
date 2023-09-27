add_contract(lptoken lptoken ${CMAKE_CURRENT_SOURCE_DIR}/src/lptoken.cpp)

target_include_directories(
   lptoken  
   PUBLIC 
   ${CMAKE_CURRENT_SOURCE_DIR}/include
)

set_target_properties(lptoken PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}")

configure_file( ${CMAKE_CURRENT_SOURCE_DIR}/ricardian/lptoken.contracts.md ${CMAKE_CURRENT_BINARY_DIR}/ricardian/lptoken.contracts.md @ONLY)

target_compile_options(lptoken PUBLIC -R${CMAKE_CURRENT_SOURCE_DIR}/ricardian -R${CMAKE_CURRENT_BINARY_DIR}/ricardian)
