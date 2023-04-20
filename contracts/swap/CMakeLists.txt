add_contract(swap swap ${CMAKE_CURRENT_SOURCE_DIR}/src/swap.cpp)

target_include_directories(
   swap  
   PUBLIC 
   ${CMAKE_CURRENT_SOURCE_DIR}/include
)

set_target_properties(swap PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}")

configure_file( ${CMAKE_CURRENT_SOURCE_DIR}/ricardian/swap.contracts.md ${CMAKE_CURRENT_BINARY_DIR}/ricardian/swap.contracts.md @ONLY)

target_compile_options(swap PUBLIC -R${CMAKE_CURRENT_SOURCE_DIR}/ricardian -R${CMAKE_CURRENT_BINARY_DIR}/ricardian)
