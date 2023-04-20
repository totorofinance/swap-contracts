#pragma once
#include <eosio/eosio.hpp>
#include <eosio/asset.hpp>

using std::string;
using namespace eosio;

CONTRACT logs : public contract {
public:
   using contract::contract;
   static constexpr name SWAP_CONTRACT {"swap.ttr"_n};

   ACTION createpair(uint64_t pair_id, name creator, extended_symbol token0, extended_symbol token1) {
      require_auth(SWAP_CONTRACT);
   }

   ACTION addliquidity(uint64_t pair_id, name owner, uint64_t liquidity, asset quantity0, asset quantity1, uint64_t total_liquidity, name contract0, name contract1, asset reserve0, asset reserve1) {
      require_auth(SWAP_CONTRACT);
   }

   ACTION rmliquidity(uint64_t pair_id, name owner, uint64_t liquidity, asset quantity0, asset quantity1, uint64_t total_liquidity, name contract0, name contract1, asset reserve0, asset reserve1) {
      require_auth(SWAP_CONTRACT);
   }

   ACTION swap(uint64_t pair_id, name owner, name contract_in, asset quantity_in, name contract_out, asset quantity_out, asset fee, double trade_price, uint64_t total_liquidity, asset reserve0, asset reserve1) {
      require_auth(SWAP_CONTRACT);
   }

};