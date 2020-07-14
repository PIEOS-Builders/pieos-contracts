# pieos-contracts
PIEOS smart contracts on EOSIO blockchain

## Build
EOSIO.CDT 1.7.x required
```shell script
./build.sh
```

## PIEOS SCO(Stake-Coin-Offering) Token Distribution Contract

### EOS Mainnet Deployment
* [https://bloks.io/account/pieosdistsco](https://bloks.io/account/pieosdistsco)

### Source Codes
* [contracts/pieos-stake-coin-offering/](https://github.com/PIEOS-Builders/pieos-contracts/tree/master/contracts/pieos-stake-coin-offering)

### EOSIO Actions
| action | description |
|--------|-------------|
| *open*  | Open Stake Account |
| *close* | Close Stake Account |
| eosio.token handler | receiving EOS from staking user, EOS REX account and BP voting profit distributors |
| *stake* | Stake EOS on SCO(Stake-Coin-Offering) Contract |
| *unstake* | Unstake EOS on SCO(Stake-Coin-Offering) Contract |
| *proxyvoted* | Update Proxy Voting Amount (only PIEOS proxy account can execute) |
| *withdraw* | Withdraw EOS or PIEOS Token |
| *claimvested* | Claim Vested PIEOS Token |
| *updaterex* | Update REX For Contract Account |
| *init* | [Admin] Initialize Contract State |
| *setacctype* | [Admin] Set Account Type |
| *sellram* | [Admin] Sell RAM |
| *voteproducer* | [Admin] Vote Producer or Proxy |


## PIEOS Governance Token Contract

### EOS Mainnet Deployment
* [https://bloks.io/account/pieostokenct](https://bloks.io/account/pieostokenct)

### Source Codes
* [contracts/pieos-governance-token/](https://github.com/PIEOS-Builders/pieos-contracts/tree/master/contracts/pieos-governance-token)

### EOSIO Actions
| action | description |
|--------|-------------|
| *create* | Create New Token |
| *issue* | Issue Tokens into Circulation |
| *open* | Open Token Balance |
| *close* | Close Token Balance |
| *transfer* | Transfer Tokens |
| *retire* | Remove Tokens from Circulation |

