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
       * @brief [Admin] Initialize contract state.
       *
       * only the contract account owner can initialize.
       * Contract owner initializes the PIEOS SCO(Stake-Coin-Offering) contract state to activate the SCO contract services.
       *
       * @pre stake pool must not be already initialized
       */
      [[eosio::action]]
      void init();

      /**
       * @brief Open Stake Account
       *
       * Allows `ram_payer` to create an staking account `owner` records with zero balances
       *
       * @param owner - the account to be created,
       * @param ram_payer - the account that supports the cost of this action.
       *
       */
      [[eosio::action]]
      void open( const name& owner, const name& ram_payer );

      /**
       * @brief Close Stake Account
       *
       * This action is the opposite for open, it closes the account `owner` and delete all the 'owner'-related records
       *
       * @param owner - the owner account to execute the close action for,
       */
      [[eosio::action]]
      void close( const name& owner );

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
      void stake( const name& owner, const asset& amount );

      /**
       * @brief Unstake EOS tokens on PIEOS SCO(Stake-Coin-Offering) contract to redeem staked EOS tokens and receive PIEOS tokens
       *
       * {{owner}} unstakes the EOS amount of {{amount}} from the staking pool on the PIEOS SCO(Stake-Coin-Offering) contract.
       * The {{owner}} receives the redeemed EOS fund including original staked EOS and staking profits, and earned PIEOS token from the contract.
       * The amount of the received SEOS represents the ownership of the {{owner}}’s staked EOS tokens and the profits(excluding contract operation costs) from the staked EOS (EOS-REX profits and BP voting rewards).
       * SPIEOS owner gets the newly-issued PIEOS tokens proportional to their SCO-staked EOS token amount and the staking time span, inversely proportional to the total amount of EOS tokens being staked by all SCO participants.
       *
       * @param owner - account unstaking its staked EOS fund
       * @param amount - unstaking EOS balance
       *
       * @pre the staking EOS amount must be equal or less than the owner's staked EOS amount
       */
      [[eosio::action]]
      void unstake( const name& owner, const asset& amount );

      /**
       * @brief Update the current proxy voting amount of account {{nowrap $action.account}}
       *
       * The PIEOS-proxy account can run `proxyvoted` action to allocate a PIEOS token share amount
       * to the PIEOS SCO participant {{$action.account}} who proxy-voted to the PIEOS-proxy account.
       *
       * @param account - the account that proxy-voted to PIEOS proxy account.
       * @param proxy_vote - the maximum supply set for the token created.
       *
       * @pre Transaction must be signed by PIEOS proxy voting account
       */
      [[eosio::action]]
      void proxyvoted( const name&  account,
                       const asset& proxy_vote );

      /**
       * @brief Withdraw EOS fund or PIEOS tokens from PIEOS SCO(Stake-Coin-Offering) Contract
       *
       *  {{owner}} withdraws the EOS or PIEOS token amount of {{amount}} from the SCO contract.
       *
       * @param owner - account withdrawing its tokens
       * @param amount - withdrawing token balance (EOS or PIEOS)
       */
      [[eosio::action]]
      void withdraw( const name& owner, const asset& amount );

      /**
       * @brief Claim vested/reserved PIEOS token balance
       *
       * {{account}} claims its vested/reserved PIEOS token amount of {{amount}} from the SCO contract.
       *
       * @param account - account claiming its vested PIEOS tokens
       * @param amount - PIEOS token balance
       */
      [[eosio::action]]
      void claimvested( const name& account, const asset& amount );

      /**
       * @brief Update REX for contract account
       *
       * Sends `updaterex` action to the system contract with contract's active permission
       *
       * @param updater - account executing updaterex action
       */
      [[eosio::action]]
      void updaterex( const name& updater );

      /**
       * @brief [Admin] Set Account Type
       *
       * The PIEOS SCO contract admin account sets a account type for an account {{account}}
       *
       * @param account - account name
       * @param type - account type flag
       */
      [[eosio::action]]
      void setacctype( const name& account, const uint32_t type );

      /**
       * @brief [Admin] Sell RAM
       *
       * The PIEOS SCO contract admin account sends `sellram` action to the system contract with contract's active permission
       *
       * @param bytes - the amount of ram to sell in bytes.
       */
      [[eosio::action]]
      void sellram( const int64_t bytes );

      /**
       * @brief [Admin] Vote Producer or Proxy
       *
       * The PIEOS SCO contract admin account sends `voteproducer` action to the system contract with contract's active permission
       *
       * @param proxy - the proxy to change the voted producers for,
       * @param producers - the list of producers to vote for, a maximum of 30 producers is allowed.
       */
      [[eosio::action]]
      void voteproducer( const name& proxy, const std::vector<name>& producers );


   private:

      static constexpr symbol STAKED_SHARE_SYMBOL = symbol(symbol_code("S" CORE_TOKEN_SYMBOL_STR ), 4);
      static constexpr symbol PROXY_VOTE_SHARE_SYMBOL = symbol(symbol_code("SPROXY"), 4);
      static constexpr symbol TOKEN_SHARE_SYMBOL = symbol(symbol_code("S" PIEOS_SYMBOL_STR ), 4);

      static constexpr int64_t STAKE_AMOUNT_SCALE_TO_GENERATED_SCO_TOKEN_AMOUNT = 10000;
      static constexpr int32_t PROXY_VOTE_TOKEN_SHARE_REDUCE_PERCENT = 2500; // weight 25.00% of EOS staking share

      //static constexpr uint32_t SCO_START_TIMESTAMP = 1593561600; // July 1, 2020 12:00:00 AM (GMT)
      //static constexpr uint32_t SCO_END_TIMESTAMP = 1625097600; // July 1, 2021 12:00:00 AM (GMT)

      static constexpr uint32_t SCO_START_TIMESTAMP = 1592211600; // June 15, 2020 18:00:00 AM (GMT+09:00)
      static constexpr uint32_t SCO_END_TIMESTAMP = 1592298000; // June 16, 2020 18:00:00 AM (GMT+09:00)

      static constexpr int64_t PIEOS_DIST_STAKE_COIN_OFFERING       = 128'000'000'0000ll;
      static constexpr int64_t PIEOS_DIST_STABILITY_FUND            = 18'000'000'0000ll;
      static constexpr int64_t PIEOS_DIST_MARKETING_OPERATION_FUND  = 18'000'000'0000ll;
      static constexpr int64_t PIEOS_DIST_DEVELOPMENT_TEAM          = 36'000'000'0000ll;

      static constexpr name PIEOS_STABILITY_FUND_ACCOUNT       = name("pieosstbfund");
      static constexpr name PIEOS_MARKETING_OPERATION_ACCOUNT  = name("pieosmarketi");
      static constexpr name PIEOS_DEVELOPMENT_TEAM_ACCOUNT     = name("pieosdevteam");

      // The PIEOS SCO contract admin account has right to execute restricted administration operations for PIEOS SCO contract
      // such as designating BP-voting reward sending account, resource(RAM) management, REX order management, BP vote management.
      // The admin account exists because the ownership of PIEOS SCO contract account will be resigned to EOS block producers
      static constexpr name PIEOS_SCO_CONTRACT_ADMIN_ACCOUNT   = name("pieosadminac");

      static constexpr int32_t EOS_REX_BP_VOTING_PROFIT_PERCENT_FOR_CONTRACT_ADMIN = 10; // 10% of EOS REX + BP voting profits


      /**
       * total_staked - symbol:(EOS,4), sum of the `staked` of every stake-account
       * total_staked_share - symbol:(SEOS,4), sum of the `staked_share` amount of every stake-account
       * core_token_for_staked - symbol:(EOS, 4), EOS balance of BP voting reward profits for SCO-staked accounts
       * total_proxy_vote - symbol:(EOS,4), sum of the `proxy_vote` of every stake-account
       * total_proxy_vote_share - symbol:(SPROXY,4), sum of the `proxy_vote_share` amount of every stake-account
       * core_token_for_proxy_vote - symbol:(EOS, 4), EOS balance of proxy BP voting reward profits for proxy-vote staking accounts
       * total_token_share - symbol:(SPIEOS,4), sum of the `token_share` amount of every stake-account
       * sco_token_unredeemed - symbol:(PIEOS,4), current unredeemed PIEOS token balance
       * last_total_issued - symbol:(PIEOS,4), total accumulated PIEOS token amount issued on this SCO contract until the `last_issue_time`
       * last_issue_time - last token issue block timestamp
       */
      struct [[eosio::table]] stake_pool {
         asset            total_staked;
         asset            total_staked_share;
         asset            core_token_for_staked;
         asset            total_proxy_vote;
         asset            total_proxy_vote_share;
         asset            core_token_for_proxy_vote;
         asset            total_token_share;
         asset            sco_token_unredeemed;
         asset            last_total_issued;
         block_timestamp  last_issue_time;

         uint64_t primary_key() const { return 0; }
      };

      typedef eosio::multi_index< "stakepool"_n, stake_pool > stake_pool_global;
      stake_pool_global _stake_pool_db;

      /**
       * core_token_bal - symbol:(EOS,4), on-contract EOS token balance, which can be withdrawn from contract account
       * sco_token_bal - symbol:(PIEOS,4), on-contract received PIEOS token balance, which can be withdrawn from contract account
       * staked - symbol:(EOS,4), current staked EOS token amount
       * staked_share - symbol:(SEOS,4), share of staked EOS token plus contract eos profit (EOSREX and BP voting rewards profit) currently held on this PIEOS SCO contract account
       * proxy_vote - symbol:(EOS,4), amount of proxy vote, the EOS amount staked through eosio.system for BP voting
       * proxy_vote_share - symbol:(SPROXY,4), share of proxy voting BP reward profit (EOS transferred from accounts having account-type as ACCOUNT_TYPE_BP_VOTE_REWARD_ACCOUNT_FOR_PROXY_VOTE_SCO)
       * token_share - symbol:(SPIEOS,4), share of newly-minted SCO token (PIEOS) balance held on this PIEOS SCO contract account
       * last_stake_time - last EOS stake block timestamp
       */
      struct [[eosio::table]] stake_account {
         asset            core_token_bal;
         asset            sco_token_bal;
         asset            staked;
         asset            staked_share;
         asset            proxy_vote;
         asset            proxy_vote_share;
         asset            token_share;
         block_timestamp  last_stake_time;

         uint64_t primary_key() const { return PIEOS_SYMBOL.code().raw(); }
      };

      typedef eosio::multi_index< "stakeaccount"_n, stake_account > stake_accounts;

      /**
       * issued - symbol:(PIEOS,4)
       */
      struct [[eosio::table]] reserved_vesting {
         asset    issued;

         uint64_t primary_key() const { return issued.symbol.code().raw(); }
      };
      typedef eosio::multi_index< "reserved"_n, reserved_vesting > reserved_vesting_accounts;


      struct [[eosio::table]] account_type {
         uint32_t acc_type;

         uint64_t primary_key()const { return 0; }
      };

      typedef eosio::multi_index< "acctype"_n, account_type > account_type_table;

      static constexpr uint32_t ACCOUNT_TYPE_NORMAL_USER_ACCOUNT = 0;
      static constexpr uint32_t ACCOUNT_TYPE_BP_VOTE_REWARD_ACCOUNT_FOR_EOS_STAKED_SCO = 1;
      static constexpr uint32_t ACCOUNT_TYPE_BP_VOTE_REWARD_ACCOUNT_FOR_PROXY_VOTE_SCO = 2;

      void add_on_contract_token_balance( const name& owner, const asset& value, const name& ram_payer );
      void sub_on_contract_token_balance( const name& owner, const asset& value );
      //asset get_on_contract_token_balance( const name& account, const symbol& symbol ) const;

      void set_account_type( const name& account, const uint32_t account_type );
      bool is_account_type( const name& account, const uint32_t account_type ) const;
      void check_staking_allowed_account( const name& account ) const;

      bool stake_pool_initialized() const { return _stake_pool_db.begin() != _stake_pool_db.end(); }
      asset get_total_core_token_amount_for_staked( const stake_pool_global::const_iterator& sp_itr ) const;

      void require_auth_of_owner_or_admin_after_sco_period( const name& owner );

      void stake_core_token( const name& owner, const asset& stake, const stake_pool_global::const_iterator& sp_itr );

      struct unstake_core_token_outcome {
         asset staked_and_profit_redeemed;  // symbol:(EOS,4) - original staked EOS + staking profits
         asset token_earned;                // symbol:(PIEOS,4) - received PIEOS token balance
         asset rex_to_sell;                 // symbol:(REX,4) - REX amount to sell
         asset rex_sold_core_token;         // symbol:(EOS,4) - core token proceeds from the REX to be sold
      };
      unstake_core_token_outcome unstake_core_token( const name& owner, const int64_t unstake_amount, const stake_pool_global::const_iterator& sp_itr );

      void stake_by_proxy_vote( const name& account, const int64_t stake_proxy_vote_amount, const stake_pool_global::const_iterator& sp_itr );

      struct unstake_by_proxy_outcome {
         asset proxy_vote_profit_redeemed;  // symbol:(EOS,4) - original staked EOS + staking profits
         asset token_earned;                // symbol:(PIEOS,4) - received PIEOS token balance
      };
      unstake_by_proxy_outcome unstake_by_proxy_vote( const name& account, const int64_t unstake_proxy_vote_amount, const stake_pool_global::const_iterator& sp_itr );

      void issue_accrued_SCO_token( const stake_pool_global::const_iterator& sp_itr );
   };

}
