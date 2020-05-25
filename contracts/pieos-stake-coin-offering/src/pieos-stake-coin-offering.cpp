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
         sp.total_staked         = asset( 0, EOS_SYMBOL );
         sp.total_proxy_vote     = asset( 0, EOS_SYMBOL );
         sp.total_staked_share   = asset( 0, STAKED_SHARE_SYMBOL );
         sp.total_token_share    = asset( 0, TOKEN_SHARE_SYMBOL );
         sp.last_total_issued    = asset( 0, PIEOS_SYMBOL );
         sp.last_issue_time      = block_timestamp(0);
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
      } else {
         // add EOS token balance to user account
         add_token_balance( from, quantity, from );
      }
   }

   // [[eosio::action]]
   void pieos_sco::stake( const name& owner, const asset& amount) {
      check( amount.symbol == EOS_SYMBOL, "stake amount symbol precision mismatch" );
      check( amount.amount > 0, "invalid stake amount" );
      check( is_account_type(owner, FLAG_NORMAL_USER_ACCOUNT) && owner != FOR_EOS_STAKED_SCO && owner != FOR_PROXY_VOTE_SCO, "stake not allowed for this account" );

      require_auth(owner);

      // subtract user's EOS balances
      sub_token_balance( owner, amount );

      share_received received = add_to_stake_pool( amount );
      add_to_stake_balance( owner, amount, received.staked_share, received.token_share );

      // deposit rex-fund and buy rex from system contract to earn rex staking profit
      eosio_system_deposit_action deposit_act{ EOS_SYSTEM_CONTRACT, { { get_self(), "active"_n } } };
      deposit_act.send( get_self(), amount );

      eosio_system_buyrex_action buyrex_act{ EOS_SYSTEM_CONTRACT, { { get_self(), "active"_n } } };
      buyrex_act.send( get_self(), amount );
   }

//   // [[eosio::action]]
//   void pieos_sco::unstake( const name& owner, const asset& amount) {
//
//   }


   void pieos_sco::add_token_balance( const name& owner, const asset& value, const name& ram_payer ) {
      token_balance_table token_balance_db( get_self(), owner.value );
      auto to = token_balance_db.find( value.symbol.code().raw() );
      if( to == token_balance_db.end() ) {
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

      token_balance_db.modify( from, owner, [&]( auto& a ) {
         a.balance -= value;
      });
   }

   asset pieos_sco::get_token_balance( const name& account, const symbol& symbol ) {
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
      if( itr == token_balance_db.end() ) {
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

   bool pieos_sco::is_account_type( const name& account, const int64_t type_flag ) {
      token_balance_table token_balance_db(get_self(), account.value);
      auto itr = token_balance_db.find(0);
      if ( itr == token_balance_db.end() ) {
         return type_flag == FLAG_NORMAL_USER_ACCOUNT;
      }
      return itr->balance.amount == type_flag;
   }

   /**
    * @brief Updates stake pool balances upon EOS staking
    *
    * @param stake - amount of EOS tokens staked
    *
    * @return share_received - calculated amount of SEOS, SPIEOS tokens received
    */
   pieos_sco::share_received pieos_sco::add_to_stake_pool( const asset& stake ) {
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

      auto itr = _stake_pool_db.begin();

      int64_t total_staked_amount = itr->total_staked.amount;
      int64_t total_proxy_vote_amount = itr->total_proxy_vote.amount;
      int64_t total_staked_share_amount = itr->total_staked_share.amount;
      int64_t total_token_share_amount = itr->total_token_share.amount;

      if (total_staked_share_amount == 0) {
         received.staked_share.amount = share_ratio * stake.amount;
         total_staked_share_amount = received.staked_share.amount;
      } else {

         asset total_rex_to_eos_balance = get_rex_to_eos_balance( get_self() );
         asset eos_balance_for_stake_sco = get_token_balance( FOR_EOS_STAKED_SCO, EOS_SYMBOL );

         const int64_t total_eos_amount_for_staked = total_rex_to_eos_balance.amount + eos_balance_for_stake_sco.amount;
         const int64_t E0 = total_eos_amount_for_staked;
         const int64_t E1 = E0 + stake.amount;
         const int64_t SS0 = total_staked_share_amount;
         const int64_t SS1 = (uint128_t(E1) * SS0) / E0;
         received.staked_share.amount = SS1 - SS0;
         total_staked_share_amount = SS1;
      }

      if (total_token_share_amount == 0) {
         received.token_share.amount = stake.amount * share_ratio;
         total_token_share_amount = received.token_share.amount;
      } else {
         const int64_t E0 = total_staked_amount + (total_proxy_vote_amount * PROXY_VOTE_TOKEN_SHARE_REDUCE_PERCENT / 100);
         const int64_t E1 = E0 + stake.amount;
         const int64_t TS0 = total_token_share_amount;
         const int64_t TS1 = (uint128_t(E1) * TS0) / E0;
         received.token_share.amount = TS1 - TS0;
         total_token_share_amount = TS1;
      }

      total_staked_amount += stake.amount;

      _stake_pool_db.modify( itr, same_payer, [&]( auto& sp ) {
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
      if( sa_itr == stake_accounts_db.end() ) {
         stake_accounts_db.emplace( owner, [&]( auto& sa ){
            sa.staked = stake;
            sa.proxy_vote = asset( 0, EOS_SYMBOL );
            sa.staked_share = stake_share_received;
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


} /// namespace pieos

extern "C" {
   void apply(uint64_t receiver, uint64_t code, uint64_t action) {
      if( code == EOS_TOKEN_CONTRACT.value && action == "transfer"_n.value ) {
         eosio::execute_action( name(receiver), name(code), &pieos::pieos_sco::receive_token );
      }
      if ( code == receiver ) {
         switch (action) {
            EOSIO_DISPATCH_HELPER(pieos::pieos_sco, (init)(stake) )
         }
      }
      eosio_exit(0);
   }
}