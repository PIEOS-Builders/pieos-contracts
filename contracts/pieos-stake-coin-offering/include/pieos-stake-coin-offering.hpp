#pragma once

#include <eosio/asset.hpp>
#include <eosio/eosio.hpp>
#include <eosio/print.hpp>
#include <eosio/system.hpp>

#include <pieos.hpp>

#include <string>

using namespace eosio;

namespace pieos {

   using std::string;

   /**
    * pieos-stake-coin-offering contract defines the structures and actions that implement
    * PIEOS SCO (stake-coin-offering) governance token distribution smart contract
    */
   class [[eosio::contract("pieos-stake-coin-offering")]] pieos_sco : public contract {
   public:
      using contract::contract;

      pieos_sco( name s, name code, datastream<const char*> ds );

      /**
       * token transfer action notification handler,
       * called when EOS token on eosio.token contract is transferred to this pieos-sco contract account
       *
       * @param from - the account to transfer from,
       * @param to - the account to be transferred to,
       * @param quantity - the quantity of tokens to be transferred,
       * @param memo - the memo string to accompany the transaction.
       */
      void receive_token( const name&    from,
                          const name&    to,
                          const asset&   quantity,
                          const string&  memo );

      /**
       * @brief Initialize contract state.
       *
       * only the contract account owner can initialize.
       * Contract owner initializes the PIEOS SCO(Stake-Coin-Offering) contract state to activate the SCO contract services.
       *
       * @pre stake pool must not be already initialized
       */
      [[eosio::action]]
      void init();

      /**
       * @brief Stake EOS tokens on PIEOS SCO(Stake-Coin-Offering) contract to earn PIEOS tokens
       *
       * {{owner}} stakes the EOS amount of {{}} from the deposited EOS fund on the PIEOS SCO(Stake-Coin-Offering) contract.
       * The {{owner}} receives EOS-share (SEOS) and PIEOS-token share(SPIEOS) from the contract.
       * The amount of the received SEOS represents the ownership of the {{owner}}’s staked EOS tokens and the profits(excluding contract operation costs) from the staked EOS (EOS-REX profits and BP voting rewards).
       * The amount of the received SPIEOS represent the right to receive the PIEOS tokens issued to the SCO contract.
       * SPIEOS owner can get the newly-issued PIEOS tokens proportional to their SCO-staked EOS token amount and the staking time span, inversely proportional to the total amount of EOS tokens being staked by all SCO participants.
       *
       * @param owner - account staking EOS to earn PIEOS token by participating in SCO(Stake-Coin-Offering)
       * @param amount - amount of EOS tokens to be staked
       *
       * @pre the staking EOS amount must be deposited (transferred) to this SCO contract accout
       */
      [[eosio::action]]
      void stake( const name& owner, const asset& amount);

      /**
       *
       */
//      [[eosio::action]]
//      void unstake( const name& owner, const asset& amount);



   private:

      static constexpr symbol STAKED_SHARE_SYMBOL = symbol(symbol_code("SEOS"), 4);
      static constexpr symbol TOKEN_SHARE_SYMBOL = symbol(symbol_code("SPIEOS"), 4);

      static constexpr uint32_t PROXY_VOTE_TOKEN_SHARE_REDUCE_PERCENT = 25; // 25% of staking share

      static constexpr uint32_t SCO_START_TIMESTAMP = 1590969600; // June 1, 2020 12:00:00 AM (GMT)
      static constexpr uint32_t SCO_END_TIMESTAMP = 1622505600; // June 1, 2021 12:00:00 AM (GMT)

      static constexpr int64_t PIEOS_DIST_STAKE_COIN_OFFERING = 128'000'000'0000ll;
      static constexpr int64_t PIEOS_DIST_STABILITY_FUND = 18'000'000'0000ll;
      static constexpr int64_t PIEOS_DIST_MARKETING_OPERATION_FUND = 18'000'000'0000ll;
      static constexpr int64_t PIEOS_DIST_DEVELOPMENT_TEAM = 36'000'000'0000ll;

      /**
       * total_staked - symbol:(EOS,4), sum of the `staked` of every stake-account
       * total_proxy_vote - symbol:(EOS,4), sum of the `proxy_vote` of every stake-account
       * total_staked_share - symbol:(SEOS,4), sum of the `staked_share` amount of every stake-account
       * total_token_share - symbol:(SPIEOS,4), sum of the `token_share` amount of every stake-account
       * last_total_issued - symbol:(PIEOS,4), total accumulated PIEOS token amount issued on this SCO contract until the `last_issue_time`
       * last_issue_time - last token issue block timestamp
       */
      struct [[eosio::table]] stake_pool {
         asset            total_staked;
         asset            total_proxy_vote;
         asset            total_staked_share;
         asset            total_token_share;
         asset            last_total_issued;
         block_timestamp  last_issue_time;

         uint64_t primary_key() const { return 0; }
      };

      typedef eosio::multi_index< "stakepool"_n, stake_pool > stake_pool_global;
      stake_pool_global _stake_pool_db;

      /**
       * staked - symbol:(EOS,4), current staked EOS token amount
       * proxy_vote - symbol:(EOS,4), amount of proxy vote, the EOS amount staked through eosio.system for BP voting
       * staked_share - symbol:(SEOS,4), share of staked EOS token plus contract eos profit (EOSREX and BP voting rewards profit) currently held on this PIEOS SCO contract account
       * token_share - symbol:(SPIEOS,4), share of newly-minted SCO token (PIEOS) balance held on this PIEOS SCO contract account
       * last_stake_time - last EOS stake block timestamp
       */
      struct [[eosio::table]] stake_account {
         asset            staked;
         asset            proxy_vote;
         asset            staked_share;
         asset            token_share;
         block_timestamp  last_stake_time;

         uint64_t primary_key() const { return token_share.symbol.code().raw(); }
      };

      typedef eosio::multi_index< "staked"_n, stake_account > stake_accounts;

      /**
       * issued - symbol:(PIEOS,4)
       */
      struct [[eosio::table]] reserved_vesting {
         asset    issued;

         uint64_t primary_key() const { return issued.symbol.code().raw(); }
      };
      typedef eosio::multi_index< "reserved"_n, reserved_vesting > reserved_vesting_accounts;


      struct [[eosio::table]] token_balance {
         asset    balance;

         uint64_t primary_key()const { return balance.symbol.code().raw(); }
      };

      typedef eosio::multi_index< "tokenbal"_n, token_balance > token_balance_table;

      static constexpr name FOR_EOS_STAKED_SCO = name("stake.pieos");
      static constexpr name FOR_PROXY_VOTE_SCO = name("proxy.pieos");

      static constexpr int64_t FLAG_NORMAL_USER_ACCOUNT = -1;
      static constexpr int64_t FLAG_BP_VOTE_REWARD_ACCOUNT_FOR_EOS_STAKED_SCO = -2;
      static constexpr int64_t FLAG_BP_VOTE_REWARD_ACCOUNT_FOR_PROXY_VOTE_SCO = -3;

      void sub_token_balance( const name& owner, const asset& value );
      void add_token_balance( const name& owner, const asset& value, const name& ram_payer );
      asset get_token_balance( const name& account, const symbol& symbol );

      void set_account_type( const name& account, const int64_t type_flag );
      bool is_account_type( const name& account, const int64_t type_flag );

      bool stake_pool_initialized() const { return _stake_pool_db.begin() != _stake_pool_db.end(); }

      struct share_received {
         asset staked_share;
         asset token_share;
      };

      share_received add_to_stake_pool( const asset& stake );
      void add_to_stake_balance( const name& owner, const asset& stake, const asset& stake_share_received, const asset& token_share_received );


   };

}
