add_contract(pieos-stake-coin-offering pieos-stake-coin-offering
        ${CMAKE_CURRENT_SOURCE_DIR}/src/pieos-stake-coin-offering.cpp
        )

target_include_directories(pieos-stake-coin-offering
        PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
        ${CMAKE_CURRENT_SOURCE_DIR}/../include)

set_target_properties(pieos-stake-coin-offering
        PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}")

configure_file( ${CMAKE_CURRENT_SOURCE_DIR}/ricardian/pieos-stake-coin-offering.contracts.md.in ${CMAKE_CURRENT_BINARY_DIR}/ricardian/pieos-stake-coin-offering.contracts.md @ONLY )

target_compile_options( pieos-stake-coin-offering PUBLIC -R${CMAKE_CURRENT_SOURCE_DIR}/ricardian -R${CMAKE_CURRENT_BINARY_DIR}/ricardian )
