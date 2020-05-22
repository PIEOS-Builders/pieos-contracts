#include <pieos-stake-coin-offering.hpp>

#include <eosio-system-contracts-interface.hpp>

using namespace eosio;

namespace pieos {

   using namespace pieos::eosiosystem;

   pieos_sco::pieos_sco( name s, name code, datastream<const char*> ds )
   : contract(s, code, ds),
     _stake_pool_db(get_self(), get_self().value)
   {
   }

   void pieos_sco::init() {
      check( !stake_pool_initialized(), "stake pool already initialized" );
      require_auth( get_self() );

      /// initialize stake pool
      _stake_pool_db.emplace( get_self(), [&]( auto& sp ) {
         sp.total_staked         = asset( 0, EOS_SYMBOL );
         sp.total_proxy_vote     = asset( 0, EOS_SYMBOL );
         sp.proxy_vote_proceeds  = asset( 0, EOS_SYMBOL );
         sp.total_staked_share   = asset( 0, STAKED_SHARE_SYMBOL );
         sp.total_token_share    = asset( 0, TOKEN_SHARE_SYMBOL );
         sp.last_total_issued    = asset( 0, PIEOS_SYMBOL );
         sp.last_issue_time      = time_point_sec(0);
      });
   }

   // called when EOS token on eosio.token contract is transferred to this pieos-sco contract account
   void pieos_sco::receive_token( const name &from, const name &to, const asset &quantity,
                                  const std::string &memo ) {
      if ( from == _self || to != _self || quantity.amount <= 0 )
         return;

      if ( quantity.symbol != EOS_SYMBOL || memo.c_str() != string("stake") )
         return;

      share_received received = add_to_stake_pool( quantity );
      add_to_stake_balance( from, quantity, received.staked_share, received.token_share );

      // deposit rex-fund and buy rex to earn rex staking profit
      eosio_system_deposit_action deposit_act{ EOS_SYSTEM_CONTRACT, { { get_self(), "active"_n } } };
      deposit_act.send( get_self(), quantity );

      eosio_system_buyrex_action buyrex_act{ EOS_SYSTEM_CONTRACT, { { get_self(), "active"_n } } };
      buyrex_act.send( get_self(), quantity );
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
      int64_t proxy_vote_proceeds_amount = itr->proxy_vote_proceeds.amount;
      int64_t total_staked_share_amount = itr->total_staked_share.amount;
      int64_t total_token_share_amount = itr->total_token_share.amount;

      if (total_staked_share_amount == 0) {
         received.staked_share.amount = share_ratio * stake.amount;
         total_staked_share_amount = received.staked_share.amount;
      } else {
         const int64_t total_eos_amount_for_staked = 1; //contract_total_REX_to_EOS + contract_EOS_balance - proxy_vote_proceeds_amount;
         //const int64_t new_rex_amount = calculate new rex amount from stake.amount;
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
 * @param owner - account name of REX owner
 * @param stake - amount EOS tokens staked
 * @param stake_share_received - amount of received SEOS tokens
 * @param token_share_received - amount of received SPIEOS tokens
 */
   void pieos_sco::add_to_stake_balance( const name& owner, const asset& stake, const asset& stake_share_received, const asset& token_share_received ) {

      const time_point_sec ct = current_time_point();

      stake_accounts stake_accounts_db( get_self(), owner.value );
      auto sa_itr = stake_accounts_db.find( owner.value );
      auto stake_account = stake_accounts_db.find( token_share_received.symbol.code().raw() );
      if( stake_account == stake_accounts_db.end() ) {
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
            EOSIO_DISPATCH_HELPER(pieos::pieos_sco, (init) )
         }
      }
      eosio_exit(0);
   }
}