#pragma once

#include <eosio/eosio.hpp>
#include <eosio/asset.hpp>

///////////////////////////////////
/// EOS Native

#define EOS_SYSTEM_CONTRACT "eosio"_n
#define EOS_TOKEN_CONTRACT "eosio.token"_n

#define EOS_SYMBOL_STR "EOS"
#define EOS_SYMBOL_DECIMAL 4
#define EOS_SYMBOL eosio::symbol(EOS_SYMBOL_STR,EOS_SYMBOL_DECIMAL)

#define REX_FUND_ACCOUNT "eosio.rex"_n
#define REX_RAM_FUND_ACCOUNT "eosio.ram"_n
#define REX_SYMBOL_STR "REX"
#define REX_SYMBOL_DECIMAL 4
#define REX_SYMBOL eosio::symbol(REX_SYMBOL_STR,REX_SYMBOL_DECIMAL)


///////////////////////////////////
/// PIEOS Governance Token

#define PIEOS_TOKEN_CONTRACT "pieostokenct"_n

#define PIEOS_SYMBOL_STR "PIEOS"
#define PIEOS_SYMBOL_DECIMAL 4
#define PIEOS_SYMBOL eosio::symbol(PIEOS_SYMBOL_STR,PIEOS_SYMBOL_DECIMAL)


///////////////////////////////////
/// PIEOS SCO(Stake-Coin-Offering) Governance Token Distribution

#define PIEOS_SCO_CONTRACT "pieosscodist"_n


///////////////////////////////////
/// PIEOS BP-voting proxy

#define PIEOS_PROXY_VOTING_ACCOUNT "pieosproxy11"_n


///////////////////////////////////
/// PIE Stablecoin

#define PIE_TOKEN_CONTRACT_STR "piestbltoken"
#define PIE_TOKEN_CONTRACT PIE_TOKEN_CONTRACT_STR_n

#define PIE_SYMBOL_STR "PIE"
#define PIE_SYMBOL_DECIMAL 4
#define PIE_SYMBOL eosio::symbol(PIE_SYMBOL_STR,PIE_SYMBOL_DECIMAL)


