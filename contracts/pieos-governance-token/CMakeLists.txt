add_contract(pieos-governance-token pieos-governance-token
        ${CMAKE_CURRENT_SOURCE_DIR}/src/pieos-governance-token.cpp
)

target_include_directories(pieos-governance-token
   PUBLIC
   ${CMAKE_CURRENT_SOURCE_DIR}/include
   ${CMAKE_CURRENT_SOURCE_DIR}/../include)

set_target_properties(pieos-governance-token
   PROPERTIES
   RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}")

configure_file( ${CMAKE_CURRENT_SOURCE_DIR}/ricardian/pieos-governance-token.contracts.md.in ${CMAKE_CURRENT_BINARY_DIR}/ricardian/pieos-governance-token.contracts.md @ONLY )

target_compile_options( pieos-governance-token PUBLIC -R${CMAKE_CURRENT_SOURCE_DIR}/ricardian -R${CMAKE_CURRENT_BINARY_DIR}/ricardian )
