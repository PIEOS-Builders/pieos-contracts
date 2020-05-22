cmake_minimum_required( VERSION 3.5 )

project(contracts)

set(EOSIO_WASM_OLD_BEHAVIOR "Off")
find_package(eosio.cdt)

set(ICON_BASE_URL "http://127.0.0.1/ricardian_assets/eosio.contracts/icons")

set(ADMIN_ICON_URI    "admin.png#9bf1cec664863bd6aaac0f814b235f8799fb02c850e9aa5da34e8a004bd6518e")
set(TOKEN_ICON_URI    "token.png#207ff68b0406eaa56618b08bda81d6a0954543f36adc328ab3065f31a5c5d654")
set(TRANSFER_ICON_URI "transfer.png#5dfad0df72772ee1ccc155e670c1d124f5c5122f1d5027565df38b418042d1dd")

if ($ENV{CLION_IDE})
    set(CMAKE_CXX_STANDARD 17)

    #file(GLOB EOSIO_CDT_INCLUDE_DIRS /usr/local/opt/eosio.cdt/opt/eosio.cdt/include/eosiolib/*/)
    ##message("EOSIO_CDT_INCLUDE_DIRS ${EOSIO_CDT_INCLUDE_DIRS}")
    #include_directories(${EOSIO_CDT_INCLUDE_DIRS})

    include_directories(
            /usr/local/opt/boost/include
            /usr/local/opt/eosio.cdt/opt/eosio.cdt/include/eosiolib/capi
            /usr/local/opt/eosio.cdt/opt/eosio.cdt/include/eosiolib/contracts
            /usr/local/opt/eosio.cdt/opt/eosio.cdt/include/eosiolib/core
            /usr/local/opt/eosio.cdt/opt/eosio.cdt/include/eosiolib/native
    )
endif ()

add_subdirectory(pieos-governance-token)
add_subdirectory(pieos-stake-coin-offering)

