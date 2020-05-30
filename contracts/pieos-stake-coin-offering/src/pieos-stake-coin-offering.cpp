#include <pieos-stake-coin-offering.hpp>

#include <eosio-system-contracts-interface.hpp>

using namespace eosio;

namespace pieos {

   using namespace pieos::eosiosystem;

   pieos_sco::pieos_sco( name s, name code, datastream<const char*> ds )
   : contract(s, code, ds),
     _stake_pool_db(get_self(), get_self().value) {
   }

   // [[eosio::action]]
   void pieos_sco::init() {
      check( !stake_pool_initialized(), "stake pool already initialized" );
      require_auth( get_self() );

      /// initialize stake pool
      _stake_pool_db.emplace( get_self(), [&]( auto& sp ) {
         sp.total_staked            = asset( 0, EOS_SYMBOL );
         sp.total_staked_share      = asset( 0, STAKED_SHARE_SYMBOL );
         sp.total_proxy_vote        = asset( 0, EOS_SYMBOL );
         sp.total_proxy_vote_share  = asset( 0, PROXY_VOTE_SHARE_SYMBOL );
         sp.total_token_share       = asset( 0, TOKEN_SHARE_SYMBOL );
         sp.last_total_issued       = asset( 0, PIEOS_SYMBOL );
         sp.last_issue_time         = block_timestamp(0);
      });
   }

   // called when EOS token on eosio.token contract is transferred to this pieos-sco contract account
   void pieos_sco::receive_token( const name &from, const name &to, const asset &quantity,
                                  const std::string &memo ) {
      if ( quantity.symbol != EOS_SYMBOL || from == _self || to != _self || quantity.amount <= 0 ) {
         return;
      }

      if ( from == REX_FUND_ACCOUNT || is_account_type(from, FLAG_BP_VOTE_REWARD_ACCOUNT_FOR_EOS_STAKED_SCO ) ) {
         // add EOS token balance to internal account for EOS-staked SCO
         add_token_balance( FOR_EOS_STAKED_SCO, quantity, get_self() );
      } else if ( is_account_type(from, FLAG_BP_VOTE_REWARD_ACCOUNT_FOR_PROXY_VOTE_SCO ) ) {
         // add EOS token balance to internal account for Proxy-Vote SCO
         add_token_balance( FOR_PROXY_VOTE_SCO, quantity, get_self() );
      } else if ( from == REX_RAM_FUND_ACCOUNT ) {
         // add EOS token balance to internal account for contract admin
         add_token_balance( PIEOS_SCO_CONTRACT_ADMIN_ACCOUNT, quantity, get_self() );
      } else {
         // add EOS token balance to user account
         add_token_balance( from, quantity, from );
      }
   }

   // [[eosio::action]]
   void pieos_sco::stake( const name& owner, const asset& amount ) {
      check( amount.symbol == EOS_SYMBOL, "stake amount symbol precision mismatch" );
      check( amount.amount > 0, "invalid stake amount" );
      check_staking_allowed_account( owner );

      require_auth(owner);

      // subtract user's on-contract EOS balance which is being deposited to EOS REX fund.
      sub_token_balance( owner, amount );

      auto sp_itr = _stake_pool_db.begin();
      // issue PIEOS accrued since last issuance time (send inline token issue action to PIEOS token contract)
      issue_accrued_SCO_token( sp_itr );

      share_received received = add_to_stake_pool( amount, sp_itr );
      add_to_stake_balance( owner, amount, received.staked_share, received.token_share );

      // (inline actions) deposit rex-fund and buy rex from system contract to earn rex staking profit
      eosio_system_deposit_action deposit_act{ EOS_SYSTEM_CONTRACT, { { get_self(), "active"_n } } };
      deposit_act.send( get_self(), amount );

      eosio_system_buyrex_action buyrex_act{ EOS_SYSTEM_CONTRACT, { { get_self(), "active"_n } } };
      buyrex_act.send( get_self(), amount );
   }

   // [[eosio::action]]
   void pieos_sco::unstake( const name& owner, const asset& amount ) {
      check( amount.symbol == EOS_SYMBOL, "unstake amount symbol precision mismatch" );
      check( amount.amount > 0, "invalid unstake amount" );
      check( stake_pool_initialized(), "stake pool not initialized");
      check_staking_allowed_account( owner );

      require_auth(owner);

      auto sp_itr = _stake_pool_db.begin();

      // issue PIEOS accrued since last issuance time (send inline token issue action to PIEOS token contract)
      issue_accrued_SCO_token( sp_itr );

      const int64_t unstake_amount = amount.amount;
      auto unstake_outcome = unstake_from_stake_pool( owner, unstake_amount, sp_itr );

      if ( unstake_outcome.token_earned.amount > 0 ) {
         // transfer received PIEOS balance ownership from contract to user
         sub_token_balance( get_self(), unstake_outcome.token_earned );
         add_token_balance( owner, unstake_outcome.token_earned, owner );
      }

      if ( unstake_outcome.staked_and_profit_redeemed.amount > 0 ) {
         // redeemed EOS fund (original staked EOS + staking profits)
         asset redeemed_to_unstaker = unstake_outcome.staked_and_profit_redeemed;

         const int64_t eos_staking_profit = unstake_outcome.staked_and_profit_redeemed.amount - unstake_amount;
         const int64_t contract_profit = eos_staking_profit * EOS_REX_BP_VOTING_PROFIT_PERCENT_FOR_CONTRACT_ADMIN / 100;
         if ( contract_profit > 0 ) {
            redeemed_to_unstaker.amount -= contract_profit;
            add_token_balance( PIEOS_SCO_CONTRACT_ADMIN_ACCOUNT, asset( contract_profit, EOS_SYMBOL ), get_self() );
         }
         // add user's on-contract EOS balance
         add_token_balance( owner, redeemed_to_unstaker, owner );
      }

      if (unstake_outcome.rex_to_sell.amount > 0) {
         // (inline action) sell rex to receive EOS
         eosio_system_sellrex_action sellrex_act{ EOS_SYSTEM_CONTRACT, { { get_self(), "active"_n } } };
         sellrex_act.send( get_self(), amount );
      }
   }

   // [[eosio::action]]
   void pieos_sco::proxyvoted( const name&   account,
                               const asset&  proxy_vote ) {
      check( proxy_vote.symbol == EOS_SYMBOL, "proxy vote symbol precision mismatch" );
      check( proxy_vote.amount < 100000000'0000, "exceeds maximum proxy vote amount" );
      check( stake_pool_initialized(), "stake pool not initialized");
      check_staking_allowed_account( account );

      require_auth( PIEOS_PROXY_VOTING_ACCOUNT );

      auto sp_itr = _stake_pool_db.begin();

      asset current_proxy_vote( 0, EOS_SYMBOL );
      {
         stake_accounts stake_accounts_db( get_self(), account.value );
         auto sa_itr = stake_accounts_db.find( TOKEN_SHARE_SYMBOL.code().raw() );
         current_proxy_vote.amount = (sa_itr == stake_accounts_db.end()) ? 0 : sa_itr->proxy_vote.amount;
      }

      asset proxy_vote_delta = proxy_vote - current_proxy_vote;
      check( proxy_vote.amount == 0 || proxy_vote_delta.amount >= 1'0000 || proxy_vote_delta.amount < -1'0000, "invalid proxy_vote_delta" );

      // issue PIEOS accrued since last issuance time (send inline token issue action to PIEOS token contract)
      issue_accrued_SCO_token( sp_itr );

      if ( proxy_vote_delta.amount > 0 ) {
         stake_by_proxy_vote( account, proxy_vote_delta.amount, sp_itr );
      } else {
         const int64_t unstake_proxy_vote_amount = -proxy_vote_delta.amount;
         auto unstake_by_proxy_outcome = unstake_by_proxy_vote( account, unstake_proxy_vote_amount, sp_itr );

         if ( unstake_by_proxy_outcome.token_earned.amount > 0 ) {
            // transfer received PIEOS balance ownership from contract to user
            sub_token_balance( get_self(), unstake_by_proxy_outcome.token_earned );
            add_token_balance( account, unstake_by_proxy_outcome.token_earned, get_self() );
         }

         if ( unstake_by_proxy_outcome.proxy_vote_profit_redeemed.amount > 0 ) {
            // redeemed proxy-vote profit
            asset redeemed_to_unstaker = unstake_by_proxy_outcome.proxy_vote_profit_redeemed;

            const int64_t contract_profit = unstake_by_proxy_outcome.proxy_vote_profit_redeemed.amount * EOS_REX_BP_VOTING_PROFIT_PERCENT_FOR_CONTRACT_ADMIN / 100;
            if ( contract_profit > 0 ) {
               redeemed_to_unstaker.amount -= contract_profit;
               add_token_balance( PIEOS_SCO_CONTRACT_ADMIN_ACCOUNT, asset( contract_profit, EOS_SYMBOL ), get_self() );
            }
            // add user's on-contract EOS balance
            add_token_balance( account, redeemed_to_unstaker, get_self() );
         }
      }
   }

   // [[eosio::action]]
   void pieos_sco::withdraw( const name& owner, const asset& amount ) {
      check( amount.symbol == EOS_SYMBOL || amount.symbol == PIEOS_SYMBOL, "withdrawal amount symbol must be EOS or PIEOS" );
      check( amount.amount > 0, "invalid withdrawal amount" );
      check_staking_allowed_account( owner );

      require_auth(owner);

      // adjust token balance on PIEOS contract
      sub_token_balance( owner, amount );

      if ( amount.symbol == EOS_SYMBOL ) {
         asset contract_eos_balance = get_token_balance_from_contract( EOS_TOKEN_CONTRACT, get_self(), EOS_SYMBOL );
         check ( amount <= contract_eos_balance, "not enough SCO contract's EOS balance because of pending REX sell orders" );

         token_transfer_action transfer_act{ EOS_SYSTEM_CONTRACT, { { get_self(), "active"_n } } };
         transfer_act.send( get_self(), owner, amount, "PIEOS SCO" );
      } else if ( amount.symbol == PIEOS_SYMBOL ) {
         token_transfer_action transfer_act{ PIEOS_TOKEN_CONTRACT, { { get_self(), "active"_n } } };
         transfer_act.send( get_self(), owner, amount, "PIEOS SCO" );
      }
   }

   // [[eosio::action]]
   void pieos_sco::claimvested( const name& account, const asset& amount ) {
      check( amount.symbol == PIEOS_SYMBOL, "claim amount symbol precision mismatch" );
      check( amount.amount > 0, "invalid claim amount" );

      require_auth( account );

      const block_timestamp sco_start_block { time_point_sec(SCO_START_TIMESTAMP) };
      const block_timestamp sco_end_block { time_point_sec(SCO_END_TIMESTAMP) };
      const int64_t total_sco_time_period = sco_end_block.slot - sco_start_block.slot;
      block_timestamp current_block = current_block_time();

      check( current_block.slot > sco_start_block.slot, "claim not allowed before SCO start" );

      reserved_vesting_accounts vesting_accounts_db( get_self(), account.value );
      auto va_itr = vesting_accounts_db.find( amount.symbol.code().raw() );
      int64_t already_claimed = (va_itr == vesting_accounts_db.end())? 0 : va_itr->issued.amount;

      int64_t max_claimable = 0;

      if ( account == PIEOS_MARKETING_OPERATION_ACCOUNT ) {
         max_claimable = PIEOS_DIST_MARKETING_OPERATION_FUND;
      } else if ( account == PIEOS_STABILITY_FUND_ACCOUNT ) {
         max_claimable = PIEOS_DIST_STABILITY_FUND;
         check( current_block.slot > sco_start_block.slot + uint32_t(total_sco_time_period) / 2, "PIEOS stability fund locked until the mid point of SCO period" );
      } else if ( account == PIEOS_DEVELOPMENT_TEAM_ACCOUNT ) {
         if ( current_block.slot > sco_end_block.slot ) {
            current_block.slot = sco_end_block.slot;
         }
         const int64_t elapsed = current_block.slot - sco_start_block.slot;
         max_claimable = (uint128_t(PIEOS_DIST_DEVELOPMENT_TEAM) * elapsed) / total_sco_time_period;
      } else {
         check( false, "not reserved vesting account" );
      }

      check( already_claimed + amount.amount <= max_claimable, "exceeds max claimable token amount" );

      if ( va_itr == vesting_accounts_db.end() ) {
         vesting_accounts_db.emplace( get_self(), [&]( auto& va ){
            va.issued = amount;
         });
      } else {
         vesting_accounts_db.modify( va_itr, same_payer, [&]( auto& va ) {
            va.issued += amount;
         });
      }

      // (inline actions) issue and transfer PIEOS tokens
      token_issue_action token_issue_act{ PIEOS_TOKEN_CONTRACT, { { get_self(), "active"_n } } };
      token_issue_act.send(get_self(), amount, "issue vested PIEOS" );

      token_transfer_action transfer_act{ PIEOS_TOKEN_CONTRACT, { { get_self(), "active"_n } } };
      transfer_act.send( get_self(), account, amount, "claim vested PIEOS" );
   }

   // [[eosio::action]]
   void pieos_sco::updaterex( const name& updater ) {
      require_auth( updater );
      eosio_system_updaterex_action updaterex_act{ EOS_SYSTEM_CONTRACT, { { get_self(), "active"_n } } };
      updaterex_act.send( get_self() );
   }

   // [[eosio::action]]
   void pieos_sco::setacctype( const name& account, const int64_t type ) {
      require_auth( PIEOS_SCO_CONTRACT_ADMIN_ACCOUNT );
      set_account_type( account, type );
   }

   // [[eosio::action]]
   void pieos_sco::sellram( const int64_t bytes ) {
      require_auth( PIEOS_SCO_CONTRACT_ADMIN_ACCOUNT );
      eosio_system_sellram_action sellram_act{ EOS_SYSTEM_CONTRACT, { { get_self(), "active"_n } } };
      sellram_act.send( get_self(), bytes );
   }

   // [[eosio::action]]
   void pieos_sco::voteproducer( const name& proxy, const std::vector<name>& producers ) {
      require_auth( PIEOS_SCO_CONTRACT_ADMIN_ACCOUNT );
      eosio_system_voteproducer_action voteproducer_act{ EOS_SYSTEM_CONTRACT, { { get_self(), "active"_n } } };
      voteproducer_act.send( get_self(), proxy, producers );
   }


   /////////////////////////////////////////////////////////////////////////

   void pieos_sco::add_token_balance( const name& owner, const asset& value, const name& ram_payer ) {
      token_balance_table token_balance_db( get_self(), owner.value );
      auto to = token_balance_db.find( value.symbol.code().raw() );
      if ( to == token_balance_db.end() ) {
         token_balance_db.emplace( ram_payer, [&]( auto& a ){
            a.balance = value;
         });
      } else {
         token_balance_db.modify( to, same_payer, [&]( auto& a ) {
            a.balance += value;
         });
      }
   }

   void pieos_sco::sub_token_balance( const name& owner, const asset& value ) {
      token_balance_table token_balance_db( get_self(), owner.value );

      const auto& from = token_balance_db.get( value.symbol.code().raw(), "no token balance object found" );
      check( from.balance.amount >= value.amount, "overdrawn balance" );

      if ( from.balance.amount == value.amount ) {
         token_balance_db.erase( from );
      } else {
         token_balance_db.modify( from, owner, [&]( auto& a ) {
            a.balance -= value;
         });
      }
   }

   asset pieos_sco::get_token_balance( const name& account, const symbol& symbol ) const {
      token_balance_table token_balance_db(get_self(), account.value);
      auto itr = token_balance_db.find(symbol.code().raw());
      if ( itr == token_balance_db.end() ) {
         return asset( 0, symbol );
      }
      return itr->balance;
   }

   void pieos_sco::set_account_type( const name& account, const int64_t type_flag ) {
      token_balance_table token_balance_db( get_self(), account.value );
      auto itr = token_balance_db.find( 0 );
      if ( itr == token_balance_db.end() ) {
         if ( type_flag != FLAG_NORMAL_USER_ACCOUNT ) {
            token_balance_db.emplace( get_self(), [&]( auto& a ){
               a.balance = asset( type_flag, symbol(0) );
            });
         }
      } else {
         if ( type_flag == FLAG_NORMAL_USER_ACCOUNT ) {
            token_balance_db.erase(itr);
         } else {
            token_balance_db.modify( itr, same_payer, [&]( auto& a ) {
               a.balance = asset( type_flag, symbol(0) );
            });
         }
      }
   }

   bool pieos_sco::is_account_type( const name& account, const int64_t type_flag ) const {
      token_balance_table token_balance_db(get_self(), account.value);
      auto itr = token_balance_db.find(0);
      if ( itr == token_balance_db.end() ) {
         return type_flag == FLAG_NORMAL_USER_ACCOUNT;
      }
      return itr->balance.amount == type_flag;
   }

   void pieos_sco::check_staking_allowed_account( const name& account ) const {
      check( is_account_type(account, FLAG_NORMAL_USER_ACCOUNT) && account != get_self() && account != FOR_EOS_STAKED_SCO && account != FOR_PROXY_VOTE_SCO, "staking not allowed for this account" );
   }

   asset pieos_sco::get_total_eos_amount_for_staked() const {
      asset total_rex_to_eos_balance = get_total_rex_to_eos_balance( get_self() );
      asset eos_balance_for_stake_sco = get_token_balance( FOR_EOS_STAKED_SCO, EOS_SYMBOL );
      asset total_eos_balance_for_staked = total_rex_to_eos_balance + eos_balance_for_stake_sco;
      return total_eos_balance_for_staked;
   }

   /**
    * @brief Updates stake pool balances upon EOS staking
    *
    * @param stake - amount of EOS tokens staked
    *
    * @return share_received - calculated amount of SEOS, SPIEOS tokens received
    */
   pieos_sco::share_received pieos_sco::add_to_stake_pool( const asset& stake, const stake_pool_global::const_iterator& sp_itr ) {
      check( stake_pool_initialized(), "stake pool not initialized");
      check( stake.symbol == EOS_SYMBOL, "stake symbol precision mismatch" );
      check( stake.amount >= 1'0000, "stake, proxy_vote under minimum delta amount" );

      /**
       * The maximum supply of core token (EOS,4) is 10^10 tokens (10 billion tokens), i.e., maximum amount
       * of indivisible units is 10^14. share_ratio = 10^4 sets the upper bound on (SEOS,4) indivisible units to
       * 10^18 and that is within the maximum allowable amount field of asset type which is set to 2^62
       * (approximately 4.6 * 10^18)
       */
      const int64_t share_ratio = 10000;

      share_received received { asset( 0, STAKED_SHARE_SYMBOL ), asset ( 0, TOKEN_SHARE_SYMBOL ) };

      int64_t total_staked_amount = sp_itr->total_staked.amount;
      int64_t total_proxy_vote_amount = sp_itr->total_proxy_vote.amount;
      int64_t total_staked_share_amount = sp_itr->total_staked_share.amount;
      int64_t total_token_share_amount = sp_itr->total_token_share.amount;

      if ( total_staked_share_amount == 0 ) {
         received.staked_share.amount = share_ratio * stake.amount;
         total_staked_share_amount = received.staked_share.amount;
      } else {
         asset total_eos_balance_for_staked = get_total_eos_amount_for_staked();

         const int64_t E0 = total_eos_balance_for_staked.amount;
         const int64_t E1 = E0 + stake.amount;
         const int64_t SS0 = total_staked_share_amount;
         const int64_t SS1 = (uint128_t(E1) * SS0) / E0;

         received.staked_share.amount = SS1 - SS0;
         total_staked_share_amount = SS1;
      }

      if ( total_token_share_amount == 0 ) {
         received.token_share.amount = stake.amount * share_ratio;
         total_token_share_amount = received.token_share.amount;
      } else {
         const int64_t total_weighted_staking_amount = total_staked_amount + (total_proxy_vote_amount * PROXY_VOTE_TOKEN_SHARE_REDUCE_PERCENT / 100);
         const int64_t total_unredeemed_sco_token_amount = get_token_balance( get_self(), PIEOS_SYMBOL ).amount;
         const int64_t EP0 = total_weighted_staking_amount + total_unredeemed_sco_token_amount; // weighted EOS amount + PIEOS amount
         const int64_t EP1 = EP0 + stake.amount;
         const int64_t TS0 = total_token_share_amount;
         const int64_t TS1 = (uint128_t(EP1) * TS0) / EP0;

         received.token_share.amount = TS1 - TS0;
         total_token_share_amount = TS1;
      }

      total_staked_amount += stake.amount;

      _stake_pool_db.modify( sp_itr, same_payer, [&]( auto& sp ) {
         sp.total_staked.amount       = total_staked_amount;
         sp.total_staked_share.amount = total_staked_share_amount;
         sp.total_token_share.amount  = total_token_share_amount;
      });

      return received;
   }

   /**
    * @brief Updates owner stake balances upon EOS staking
    *
    * @param owner - staking account name
    * @param stake - amount EOS tokens staked
    * @param stake_share_received - amount of received SEOS tokens
    * @param token_share_received - amount of received SPIEOS tokens
    */
   void pieos_sco::add_to_stake_balance( const name& owner, const asset& stake, const asset& stake_share_received, const asset& token_share_received ) {
      const block_timestamp ct = current_block_time();

      stake_accounts stake_accounts_db( get_self(), owner.value );
      auto sa_itr = stake_accounts_db.find( token_share_received.symbol.code().raw() );
      if ( sa_itr == stake_accounts_db.end() ) {
         stake_accounts_db.emplace( owner, [&]( auto& sa ){
            sa.staked = stake;
            sa.staked_share = stake_share_received;
            sa.proxy_vote = asset( 0, EOS_SYMBOL );
            sa.proxy_vote_share = asset( 0, PROXY_VOTE_SHARE_SYMBOL );
            sa.token_share = token_share_received;
            sa.last_stake_time = ct;
         });
      } else {
         stake_accounts_db.modify( sa_itr, same_payer, [&]( auto& sa ) {
            sa.staked.amount += stake.amount;
            sa.staked_share.amount += stake_share_received.amount;
            sa.token_share.amount += token_share_received.amount;
            sa.last_stake_time = ct;
         });
      }
   }

   /**
    * @brief processes unstaking transaction.
    * The staked shares and token shares proportional to the unstaking proportion of the user's total staked EOS
    * are redeemed to the EOS fund (original staked EOS + staking profits) and the earned PIEOS tokens.
    * Corresponding table data updates are exucuted.
    *
    * @param owner - account unstaking its staked EOS fund
    * @param unstake_amount - unstaking amount
    * @return unstake_outcome
    *   : redeemed - symbol:(EOS,4) - original staked EOS + staking profits
    *   : token_earned - symbol:(PIEOS, 4) - received PIEOS token balance
    *
    * @pre unstake_amount must be equal or less than the owner's staked amount(EOS)
    */
   pieos_sco::unstake_outcome pieos_sco::unstake_from_stake_pool( const name& owner, const int64_t unstake_amount, const stake_pool_global::const_iterator& sp_itr ) {
      stake_accounts stake_accounts_db( get_self(), owner.value );
      auto sa_itr = stake_accounts_db.require_find( TOKEN_SHARE_SYMBOL.code().raw(), "stake account record not found" );

      int64_t stake_account_staked_amount = sa_itr->staked.amount;
      int64_t stake_account_staked_share_amount = sa_itr->staked_share.amount;
      const int64_t stake_account_proxy_vote_amount = sa_itr->proxy_vote.amount;
      int64_t stake_account_token_share_amount = sa_itr->token_share.amount;

      check( unstake_amount <= stake_account_staked_amount, "not enough staked balance" );

      time_point_sec ct_sec(current_time_point());
      time_point_sec rex_maturity_last_buyrex = get_rex_maturity(sa_itr->last_stake_time);

      check( ct_sec > rex_maturity_last_buyrex, "cannot run unstake until rex maturity time" );

      int64_t total_staked_amount = sp_itr->total_staked.amount;
      int64_t total_proxy_vote_amount = sp_itr->total_proxy_vote.amount;
      int64_t total_staked_share_amount = sp_itr->total_staked_share.amount;
      int64_t total_token_share_amount = sp_itr->total_token_share.amount;

      const int64_t staked_share_to_redeem = (uint128_t(unstake_amount) * stake_account_staked_share_amount) / stake_account_staked_amount;
      const int64_t token_share_to_redeem = (uint128_t(unstake_amount) * stake_account_token_share_amount) / (stake_account_staked_amount + (stake_account_proxy_vote_amount * PROXY_VOTE_TOKEN_SHARE_REDUCE_PERCENT / 100));

      unstake_outcome outcome { asset( 0, EOS_SYMBOL ), asset ( 0, PIEOS_SYMBOL ), asset( 0, REX_SYMBOL ) };

      if ( staked_share_to_redeem > 0 ) {
         asset rex_balance = get_rex_balance( get_self() );
         asset rex_eos_balance = rex_to_eos_balance( rex_balance );
         asset eos_balance_for_stake_sco = get_token_balance( FOR_EOS_STAKED_SCO, EOS_SYMBOL );
         asset total_eos_balance_for_staked = rex_eos_balance + eos_balance_for_stake_sco;

         const int64_t E0 = total_eos_balance_for_staked.amount;
         const int64_t SS0 = total_staked_share_amount;
         const int64_t eos_proceeds  = (uint128_t(staked_share_to_redeem) * E0) / SS0;
         const int64_t SS1 = SS0 - staked_share_to_redeem;
         //const int64_t E1 = E0 - eos_proceeds;

         outcome.staked_and_profit_redeemed.amount = eos_proceeds;

         const int64_t rex_amount_to_sell = (uint128_t(staked_share_to_redeem) * rex_balance.amount) / total_staked_share_amount;
         outcome.rex_to_sell.amount = rex_amount_to_sell;

         stake_account_staked_share_amount -= staked_share_to_redeem;
         total_staked_share_amount = SS1;
      }

      if ( token_share_to_redeem > 0 ) {
         const int64_t total_weighted_staking_amount = total_staked_amount + (total_proxy_vote_amount * PROXY_VOTE_TOKEN_SHARE_REDUCE_PERCENT / 100);
         const int64_t total_unredeemed_sco_token_amount = get_token_balance( get_self(), PIEOS_SYMBOL ).amount;

         const int64_t EP0 = total_weighted_staking_amount + total_unredeemed_sco_token_amount; // weighted EOS amount + PIEOS amount
         const int64_t TS0 = total_token_share_amount;
         const int64_t p  = (uint128_t(token_share_to_redeem) * EP0) / TS0;
         const int64_t redeemed_token_amount  = p - unstake_amount; // newly issued tokens since staked
         const int64_t TS1 = TS0 - token_share_to_redeem;
         //const int64_t EP1 = EP0 - p;

         outcome.token_earned.amount = redeemed_token_amount;

         stake_account_token_share_amount -= token_share_to_redeem;
         total_token_share_amount = TS1;
      }

      stake_account_staked_amount -= unstake_amount;
      total_staked_amount -= unstake_amount;

      _stake_pool_db.modify( sp_itr, same_payer, [&]( auto& sp ) {
         sp.total_staked.amount       = total_staked_amount;
         sp.total_staked_share.amount = total_staked_share_amount;
         sp.total_token_share.amount  = total_token_share_amount;
      });

      if ( stake_account_staked_amount == 0
           && stake_account_staked_share_amount == 0
           && stake_account_token_share_amount == 0
           && sa_itr->proxy_vote.amount == 0 && sa_itr->proxy_vote_share.amount == 0 ) {
         stake_accounts_db.erase( sa_itr );
      } else {
         stake_accounts_db.modify( sa_itr, same_payer, [&]( auto& sa ) {
            sa.staked.amount        = stake_account_staked_amount;
            sa.staked_share.amount  = stake_account_staked_share_amount;
            sa.token_share.amount   = stake_account_token_share_amount;
         });
      }

      return outcome;
   }

   /**
    * @brief update stake pool, stake account balances for proxy-voting staking event notified
    * the proxy-voted account receives token shares(SPIEOS) and proxy-vote shares(SPROXY)
    *
    * @param account - proxy-voted account
    * @param stake_proxy_vote_amount - added proxy-voting amount
    */
   void pieos_sco::stake_by_proxy_vote( const name& account, const int64_t stake_proxy_vote_amount, const stake_pool_global::const_iterator& sp_itr ) {
      const int64_t share_ratio = 10000;

      const int64_t total_staked_amount = sp_itr->total_staked.amount;
      int64_t total_proxy_vote_amount = sp_itr->total_proxy_vote.amount;
      int64_t total_proxy_vote_share_amount = sp_itr->total_proxy_vote_share.amount;
      int64_t total_token_share_amount = sp_itr->total_token_share.amount;

      const int64_t stake_proxy_vote_weighted = stake_proxy_vote_amount * PROXY_VOTE_TOKEN_SHARE_REDUCE_PERCENT / 100;

      int64_t received_token_share_amount = 0;
      if ( total_token_share_amount == 0 ) {
         received_token_share_amount = share_ratio * stake_proxy_vote_weighted;
         total_token_share_amount = received_token_share_amount;
      } else {
         const int64_t total_weighted_staking_amount = total_staked_amount + (total_proxy_vote_amount * PROXY_VOTE_TOKEN_SHARE_REDUCE_PERCENT / 100);
         const int64_t total_unredeemed_sco_token_amount = get_token_balance( get_self(), PIEOS_SYMBOL ).amount;

         const int64_t EP0 = total_weighted_staking_amount + total_unredeemed_sco_token_amount; // weighted EOS amount + PIEOS amount
         const int64_t EP1 = EP0 + stake_proxy_vote_weighted;
         const int64_t TS0 = total_token_share_amount;
         const int64_t TS1 = (uint128_t(EP1) * TS0) / EP0;

         received_token_share_amount = TS1 - TS0;
         total_token_share_amount = TS1;
      }

      int64_t received_proxy_vote_share_amount = 0;
      if ( total_proxy_vote_share_amount == 0 ) {
         received_proxy_vote_share_amount = share_ratio * stake_proxy_vote_amount;
         total_proxy_vote_share_amount = received_proxy_vote_share_amount;
      } else {
         const int64_t total_unredeemed_proxy_vote_profit_amount = get_token_balance( FOR_PROXY_VOTE_SCO, EOS_SYMBOL ).amount;

         const int64_t E0 = total_proxy_vote_amount + total_unredeemed_proxy_vote_profit_amount;
         const int64_t E1 = E0 + stake_proxy_vote_amount;
         const int64_t PVS0 = total_proxy_vote_share_amount;
         const int64_t PVS1 = (uint128_t(E1) * PVS0) / E0;

         received_proxy_vote_share_amount = PVS1 - PVS0;
         total_proxy_vote_share_amount = PVS1;
      }

      total_proxy_vote_amount += stake_proxy_vote_amount;

      _stake_pool_db.modify( sp_itr, same_payer, [&]( auto& sp ) {
         sp.total_proxy_vote.amount  = total_proxy_vote_amount;
         sp.total_proxy_vote_share.amount  = total_proxy_vote_share_amount;
         sp.total_token_share.amount = total_token_share_amount;
      });

      stake_accounts stake_accounts_db( get_self(), account.value );
      auto sa_itr = stake_accounts_db.find( TOKEN_SHARE_SYMBOL.code().raw() );

      // update stake account balances
      if ( sa_itr == stake_accounts_db.end() ) {
         stake_accounts_db.emplace( get_self(), [&]( auto& sa ){
            sa.staked = asset( 0, EOS_SYMBOL );
            sa.staked_share = asset( 0, STAKED_SHARE_SYMBOL );
            sa.proxy_vote = asset( stake_proxy_vote_amount, EOS_SYMBOL );
            sa.proxy_vote_share = asset( received_proxy_vote_share_amount, PROXY_VOTE_SHARE_SYMBOL );
            sa.token_share = asset( received_token_share_amount, TOKEN_SHARE_SYMBOL );
            sa.last_stake_time = block_timestamp(0);
         });
      } else {
         stake_accounts_db.modify( sa_itr, same_payer, [&]( auto& sa ) {
            sa.proxy_vote.amount += stake_proxy_vote_amount;
            sa.proxy_vote_share.amount = received_proxy_vote_share_amount;
            sa.token_share.amount += received_token_share_amount;
         });
      }
   }

   /**
    * @brief update stake pool, stake account balances for withdrawn proxy-voting (unstaking) event notified
    * the proxy-voted account receives the earned PIEOS tokens(redeemed from SCO token shares(SPIEOS))
    * and proxy-vote profits(EOS)(redeemed from account's proxy-vote shares(SPROXY))
    *
    * @param account - proxy-vote-withdrawn account
    * @param unstake_proxy_vote_amount - withdrawn proxy-voting amount
    * @return unstake_by_proxy_outcome
    *    proxy_vote_profit_redeemed - symbol:(EOS,4), original staked EOS + staking profits
    *    token_earned - symbol:(PIEOS, 4), received PIEOS token balance
    */
   pieos_sco::unstake_by_proxy_outcome pieos_sco::unstake_by_proxy_vote( const name& account, const int64_t unstake_proxy_vote_amount, const stake_pool_global::const_iterator& sp_itr ) {
      stake_accounts stake_accounts_db( get_self(), account.value );
      auto sa_itr = stake_accounts_db.require_find( TOKEN_SHARE_SYMBOL.code().raw(), "stake account record not found" );

      const int64_t stake_account_staked_amount = sa_itr->staked.amount;
      const int64_t stake_account_staked_share_amount = sa_itr->staked_share.amount;
      int64_t stake_account_proxy_vote_amount = sa_itr->proxy_vote.amount;
      int64_t stake_account_proxy_vote_share_amount = sa_itr->proxy_vote_share.amount;
      int64_t stake_account_token_share_amount = sa_itr->token_share.amount;

      check( unstake_proxy_vote_amount <= stake_account_proxy_vote_amount, "not enough staked proxy vote balance" );

      unstake_by_proxy_outcome outcome { asset( 0, EOS_SYMBOL ), asset ( 0, PIEOS_SYMBOL ) };

      const int64_t total_staked_amount = sp_itr->total_staked.amount;
      int64_t total_proxy_vote_amount = sp_itr->total_proxy_vote.amount;
      int64_t total_proxy_vote_share_amount = sp_itr->total_proxy_vote_share.amount;
      int64_t total_token_share_amount = sp_itr->total_token_share.amount;

      const int64_t unstake_proxy_vote_weighted = unstake_proxy_vote_amount * PROXY_VOTE_TOKEN_SHARE_REDUCE_PERCENT / 100;

      const int64_t token_share_to_redeem = (uint128_t(unstake_proxy_vote_weighted) * stake_account_token_share_amount) / (stake_account_staked_amount + (stake_account_proxy_vote_amount * PROXY_VOTE_TOKEN_SHARE_REDUCE_PERCENT / 100));
      const int64_t proxy_vote_share_to_redeem = (uint128_t(unstake_proxy_vote_amount) * stake_account_proxy_vote_share_amount) / stake_account_proxy_vote_amount;

      if ( token_share_to_redeem > 0 ) {
         const int64_t total_weighted_staking_amount = total_staked_amount + (total_proxy_vote_amount * PROXY_VOTE_TOKEN_SHARE_REDUCE_PERCENT / 100);
         const int64_t total_unredeemed_sco_token_amount = get_token_balance( get_self(), PIEOS_SYMBOL ).amount;

         const int64_t EP0 = total_weighted_staking_amount + total_unredeemed_sco_token_amount; // weighted EOS amount + PIEOS amount
         const int64_t TS0 = total_token_share_amount;
         const int64_t p  = (uint128_t(token_share_to_redeem) * EP0) / TS0;
         const int64_t TS1 = TS0 - token_share_to_redeem;
         //const int64_t EP1 = EP0 - p;

         const int64_t redeemed_token_amount  = p - unstake_proxy_vote_weighted; // newly issued tokens since proxy-vote staked
         outcome.token_earned.amount = redeemed_token_amount;

         stake_account_token_share_amount -= token_share_to_redeem;
         total_token_share_amount = TS1;
      }

      if ( proxy_vote_share_to_redeem > 0 ) {
         const int64_t total_unredeemed_proxy_vote_profit_amount = get_token_balance( FOR_PROXY_VOTE_SCO, EOS_SYMBOL ).amount;

         const int64_t E0 = total_proxy_vote_amount + total_unredeemed_proxy_vote_profit_amount;
         const int64_t PVS0 = total_proxy_vote_share_amount;
         const int64_t p  = (uint128_t(proxy_vote_share_to_redeem) * E0) / PVS0;
         const int64_t PVS1 = PVS0 - proxy_vote_share_to_redeem;
         //const int64_t E1 = E0 - p;

         const int64_t redeemed_proxy_vote_profit_amount = p - unstake_proxy_vote_amount; // newly added proxy-vote profits since proxy-vote staked
         outcome.proxy_vote_profit_redeemed.amount = redeemed_proxy_vote_profit_amount;

         stake_account_proxy_vote_share_amount -= proxy_vote_share_to_redeem;
         total_proxy_vote_share_amount = PVS1;
      }

      stake_account_proxy_vote_amount -= unstake_proxy_vote_amount;
      total_proxy_vote_amount -= unstake_proxy_vote_amount;

      _stake_pool_db.modify( sp_itr, same_payer, [&]( auto& sp ) {
         sp.total_proxy_vote.amount        = total_proxy_vote_amount;
         sp.total_proxy_vote_share.amount  = total_proxy_vote_share_amount;
         sp.total_token_share.amount       = total_token_share_amount;
      });

      if ( stake_account_staked_amount == 0
           && stake_account_staked_share_amount == 0
           && stake_account_token_share_amount == 0
           && stake_account_proxy_vote_amount == 0
           && stake_account_proxy_vote_share_amount == 0 ) {
         stake_accounts_db.erase( sa_itr );
      } else {
         stake_accounts_db.modify( sa_itr, same_payer, [&]( auto& sa ) {
            sa.proxy_vote.amount        = stake_account_proxy_vote_amount;
            sa.proxy_vote_share.amount  = stake_account_proxy_vote_share_amount;
            sa.token_share.amount       = stake_account_token_share_amount;
         });
      }

      return outcome;
   }

   /**
    * @brief Issue new PIEOS allocated to PIEOS SCO distribution, accrued since last issuance time
    */
   void pieos_sco::issue_accrued_SCO_token( const stake_pool_global::const_iterator& sp_itr ) {
      check( stake_pool_initialized(), "stake pool not initialized");

      const block_timestamp sco_start_block { time_point_sec(SCO_START_TIMESTAMP) };
      const block_timestamp sco_end_block { time_point_sec(SCO_END_TIMESTAMP) };

      block_timestamp last_issue_block = sp_itr->last_issue_time;
      block_timestamp current_block = current_block_time();

      if ( current_block.slot == last_issue_block.slot
           || current_block.slot <= sco_start_block.slot
           || last_issue_block.slot >= sco_end_block.slot ) {
         return;
      }

      if ( current_block.slot > sco_end_block.slot ) {
         current_block.slot = sco_end_block.slot;
      }

      if ( last_issue_block.slot < sco_start_block.slot ) {
         last_issue_block.slot = sco_start_block.slot;
      }

      const int64_t elapsed = current_block.slot - last_issue_block.slot;
      const int64_t total_sco_time_period = sco_end_block.slot - sco_start_block.slot;
      const int64_t token_issue_amount = (uint128_t(PIEOS_DIST_STAKE_COIN_OFFERING) * elapsed) / total_sco_time_period;

      if ( token_issue_amount > 0 ) {
         asset issued(token_issue_amount, PIEOS_SYMBOL );
         add_token_balance( get_self(), issued, get_self() ); // add unredeemed(unclaimed) PIEOS SCO token balance

         token_issue_action token_issue_act{ PIEOS_TOKEN_CONTRACT, { { get_self(), "active"_n } } };
         token_issue_act.send(get_self(), issued, "PIEOS SCO" );
      }

      _stake_pool_db.modify( sp_itr, same_payer, [&]( auto& sp ) {
         sp.last_total_issued.amount += token_issue_amount;
         sp.last_issue_time = current_block;
      });
   }

} /// namespace pieos

extern "C" {
   void apply(uint64_t receiver, uint64_t code, uint64_t action) {
      if ( code == EOS_TOKEN_CONTRACT.value && action == "transfer"_n.value ) {
         eosio::execute_action( name(receiver), name(code), &pieos::pieos_sco::receive_token );
      }
      if ( code == receiver ) {
         switch (action) {
            EOSIO_DISPATCH_HELPER(pieos::pieos_sco, (init)(stake)(unstake)(proxyvoted)(withdraw)(claimvested)(updaterex)(setacctype)(sellram)(voteproducer) )
         }
      }
      eosio_exit(0);
   }
}