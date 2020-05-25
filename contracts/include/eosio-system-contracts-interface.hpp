#pragma once

#include <eosio/eosio.hpp>
#include <eosio/asset.hpp>

#include <pieos.hpp>

namespace pieos::eosiosystem {

   using namespace eosio;

   ///////////////////////////////////////////////////////
   /// eosio token contract (`eosio.token`)

   struct account {
      asset    balance;

      uint64_t primary_key()const { return balance.symbol.code().raw(); }
   };

   typedef eosio::multi_index< "accounts"_n, account > accounts_table;

   struct currency_stats {
      asset    supply;
      asset    max_supply;
      name     issuer;

      uint64_t primary_key()const { return supply.symbol.code().raw(); }
   };

   typedef eosio::multi_index< "stat"_n, currency_stats > stats_table;

   class token_contract_action_interface {
   public:

      /**
       *  This action issues to `to` account a `quantity` of tokens.
       *
       * @param to - the account to issue tokens to, it must be the same as the issuer,
       * @param quntity - the amount of tokens to be issued,
       * @memo - the memo string that accompanies the token issue transaction.
       */
      virtual void issue( const name& to, const asset& quantity, const string& memo );

      /**
       * Allows `from` account to transfer to `to` account the `quantity` tokens.
       * One account is debited and the other is credited with quantity tokens.
       *
       * @param from - the account to transfer from,
       * @param to - the account to be transferred to,
       * @param quantity - the quantity of tokens to be transferred,
       * @param memo - the memo string to accompany the transaction.
       */
      virtual void transfer( const name&    from,
                             const name&    to,
                             const asset&   quantity,
                             const string&  memo );
   };

   using token_issue_action = eosio::action_wrapper<"issue"_n, &token_contract_action_interface::issue>;
   using token_transfer_action = eosio::action_wrapper<"transfer"_n, &token_contract_action_interface::transfer>;

   asset get_token_balance_from_contract( const name& contract, const name& account, const symbol& symbol ) {
      accounts_table accounts(contract, account.value);
      auto itr = accounts.find(symbol.code().raw());
      if ( itr == accounts.end() ) {
         return asset( 0, symbol );
      }
      return itr->balance;
   }


   ///////////////////////////////////////////////////////
   /// eosio system contract (`eosio`)

   struct rex_pool {
      uint8_t    version = 0;
      asset      total_lent;
      asset      total_unlent;
      asset      total_rent;
      asset      total_lendable;
      asset      total_rex;
      asset      namebid_proceeds;
      uint64_t   loan_num = 0;

      uint64_t primary_key()const { return 0; }
   };

   typedef eosio::multi_index< "rexpool"_n, rex_pool > rex_pool_table;

   struct rex_balance {
      uint8_t version = 0;
      name    owner;
      asset   vote_stake;
      asset   rex_balance;
      int64_t matured_rex = 0;
      std::deque<std::pair<time_point_sec, int64_t>> rex_maturities; /// REX daily maturity buckets

      uint64_t primary_key()const { return owner.value; }
   };

   typedef eosio::multi_index< "rexbal"_n, rex_balance > rex_balance_table;

   class system_contract_action_interface {
   public:
      /**
       * Deposit to REX fund action. Deposits core tokens to user REX fund.
       * All proceeds and expenses related to REX are added to or taken out of this fund.
       * An inline transfer from 'owner' liquid balance is executed.
       * All REX-related costs and proceeds are deducted from and added to 'owner' REX fund,
       *    with one exception being buying REX using staked tokens.
       * Storage change is billed to 'owner'.
       *
       * @param owner - REX fund owner account,
       * @param amount - amount of tokens to be deposited.
       */
      virtual void deposit( const name& owner, const asset& amount );

      /**
       * Withdraw from REX fund action, withdraws core tokens from user REX fund.
       * An inline token transfer to user balance is executed.
       *
       * @param owner - REX fund owner account,
       * @param amount - amount of tokens to be withdrawn.
       */
      virtual void withdraw( const name& owner, const asset& amount );

      /**
       * Buyrex action, buys REX in exchange for tokens taken out of user's REX fund by transfering
       * core tokens from user REX fund and converts them to REX stake. By buying REX, user is
       * lending tokens in order to be rented as CPU or NET resourses.
       * Storage change is billed to 'from' account.
       *
       * @param from - owner account name,
       * @param amount - amount of tokens taken out of 'from' REX fund.
       *
       * @pre A voting requirement must be satisfied before action can be executed.
       * @pre User must vote for at least 21 producers or delegate vote to proxy before buying REX.
       *
       * @post User votes are updated following this action.
       * @post Tokens used in purchase are added to user's voting power.
       * @post Bought REX cannot be sold before 4 days counting from end of day of purchase.
       */
      virtual void buyrex( const name& from, const asset& amount );

      /**
       * Sellrex action, sells REX in exchange for core tokens by converting REX stake back into core tokens
       * at current exchange rate. If order cannot be processed, it gets queued until there is enough
       * in REX pool to fill order, and will be processed within 30 days at most. If successful, user
       * votes are updated, that is, proceeds are deducted from user's voting power. In case sell order
       * is queued, storage change is billed to 'from' account.
       *
       * @param from - owner account of REX,
       * @param rex - amount of REX to be sold.
       */
      virtual void sellrex( const name& from, const asset& rex );

      /**
       * Updaterex action, updates REX owner vote weight to current value of held REX tokens.
       *
       * @param owner - REX owner account.
       */
      virtual void updaterex( const name& owner );

      /**
       * Vote producer action, votes for a set of producers. This action updates the list of `producers` voted for,
       * for `voter` account. If voting for a `proxy`, the producer votes will not change until the
       * proxy updates their own vote. Voter can vote for a proxy __or__ a list of at most 30 producers.
       * Storage change is billed to `voter`.
       *
       * @param voter - the account to change the voted producers for,
       * @param proxy - the proxy to change the voted producers for,
       * @param producers - the list of producers to vote for, a maximum of 30 producers is allowed.
       *
       * @pre Producers must be sorted from lowest to highest and must be registered and active
       * @pre If proxy is set then no producers can be voted for
       * @pre If proxy is set then proxy account must exist and be registered as a proxy
       * @pre Every listed producer or proxy must have been previously registered
       * @pre Voter must authorize this action
       * @pre Voter must have previously staked some EOS for voting
       * @pre Voter->staked must be up to date
       *
       * @post Every producer previously voted for will have vote reduced by previous vote weight
       * @post Every producer newly voted for will have vote increased by new vote amount
       * @post Prior proxy will proxied_vote_weight decremented by previous vote weight
       * @post New proxy will proxied_vote_weight incremented by new vote weight
       */
      virtual void voteproducer( const name& voter, const name& proxy, const std::vector<name>& producers );
   };

   using eosio_system_deposit_action = eosio::action_wrapper<"deposit"_n, &system_contract_action_interface::deposit>;
   using eosio_system_withdraw_action = eosio::action_wrapper<"withdraw"_n, &system_contract_action_interface::withdraw>;
   using eosio_system_buyrex_action = eosio::action_wrapper<"buyrex"_n, &system_contract_action_interface::buyrex>;
   using eosio_system_sellrex_action = eosio::action_wrapper<"sellrex"_n, &system_contract_action_interface::sellrex>;
   using eosio_system_updaterex_action = eosio::action_wrapper<"updaterex"_n, &system_contract_action_interface::updaterex>;
   using eosio_system_voteproducer_action = eosio::action_wrapper<"voteproducer"_n, &system_contract_action_interface::voteproducer>;

   asset get_rex_to_eos_balance( const name& account ) {
      rex_balance_table rex_balances( EOS_SYSTEM_CONTRACT, EOS_SYSTEM_CONTRACT.value );
      auto rb_itr = rex_balances.find( account.value );
      if ( rb_itr == rex_balances.end() ) {
         return asset( 0, EOS_SYMBOL );
      }

      rex_pool_table rex_pool( EOS_SYSTEM_CONTRACT, EOS_SYSTEM_CONTRACT.value );
      auto rp_itr = rex_pool.begin();
      if ( rp_itr == rex_pool.end() ) {
         return asset( 0, EOS_SYMBOL );
      }

      const int64_t S0 = rp_itr->total_lendable.amount;
      const int64_t R0 = rp_itr->total_rex.amount;
      const int64_t eos_share = (uint128_t(rb_itr->rex_balance.amount) * S0) / R0;
      return asset( eos_share, EOS_SYMBOL );
   }

} // namespace pieos::eosiosystem
