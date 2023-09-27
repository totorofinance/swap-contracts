# swap contracts project

## Build

### How to Build -
   - cd to 'build' directory
   - run the command 'cmake ..'
   - run the command 'make'

### After build -
   - The built smart contract is under the 'build' directory
   - You can then do a 'set contract' action with 'cleos' and point in to the './build/xxx' directory


## How to use

### setup
cleos push action swap.ttr setname '["manager", "admin.ttr"]' -p admin.ttr  
cleos push action swap.ttr setname '["fee.account", "fees.ttr"]' -p admin.ttr  
cleos push action swap.ttr setconfig '["fee.protocol", 10]' -p admin.ttr  
cleos push action swap.ttr setconfig '["fee.trade", 20]' -p admin.ttr  
cleos push action swap.ttr setconfig '["status", 1]' -p admin.ttr  

// additional  
cleos push action swap.ttr setname '["mine.account", "mine.ttr"]' -p admin.ttr  
cleos push action swap.ttr setconfig '["mine.status", 1]' -p admin.ttr  
cleos push action swap.ttr setname '["orac.account", "oracle.ttr"]' -p admin.ttr  
cleos push action swap.ttr setconfig '["orac.status", 1]' -p admin.ttr  

### create pair
cleos push action swap.ttr createpair '{"creator":"init.ttr","token0":{"contract":"eosio.token","sym":"4,EOS"},"token1":{"contract":"tethertether","sym":"4,USDT"}}' -p init.ttr  

### remove pair
cleos push action swap.ttr removepair '{"pair_id":1}' -p admin.ttr  

### add liquidity
cleos push action eosio.token transfer '["init.ttr","swap.ttr","1000.0000 EOS","deposit:1"]' -p init.ttr  
cleos push action tethertether transfer '["init.ttr","swap.ttr","2000.0000 USDT","deposit:1"]' -p init.ttr  
cleos push action swap.ttr addliquidity '["init.ttr",1]' -p init.ttr  

### cancle deposit order
cleos push action swap.ttr cancel '["init.ttr",1]' -p init.ttr  

### remove liquidity
cleos push action lptoken.ttr transfer '["init.ttr","swap.ttr","14832396 LPA",""]' -p init.ttr  

### swap
cleos push action eosio.token transfer '["init.ttr","swap.ttr","1.0000 EOS","swap:1"]' -p init.ttr  
cleos push action tethertether transfer '["init.ttr","swap.ttr","1.0000 USDT","swap:1"]' -p init.ttr  

### multi-path swap
cleos push action tethertether transfer '["init.ttr","swap.ttr","10.0000 USDT","swap:1-2"]' -p init.ttr  

 