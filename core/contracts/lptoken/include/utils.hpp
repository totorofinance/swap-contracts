#pragma once
#include <eosio/eosio.hpp>
#include <eosio/asset.hpp>

using std::string;
using namespace eosio;

const name SWAP_CONTRACT = name("swap.ttr");
const name LPNOTIFY_CONTRACT = name("lpnotify.ttr");

struct liquidity_cost {
    uint64_t cost0;
    uint64_t cost1;
};

struct pair {
    uint64_t id;
    symbol_code code;
    extended_symbol token0;
    extended_symbol token1;
    asset reserve0;
    asset reserve1;
    uint64_t total_liquidity;
    block_timestamp block_time_create;
    block_timestamp block_time_last;
    uint64_t primary_key() const { return id; }
};
typedef multi_index<"pairs"_n, pair> pairs_mi;

uint64_t code2id(symbol_code code) {
    string s = code.to_string().substr(2);
    uint64_t id = 0;
    for (int i = 0, j = 1; i < s.size(); i++, j *= 26) {
		id += (int)(s[s.size() - i - 1] - 64) * j;
	}
    return id;
}

liquidity_cost get_costs(symbol_code code, uint64_t liquidity) {
    uint64_t pair_id = code2id(code);
    pairs_mi pairs_tb(SWAP_CONTRACT, SWAP_CONTRACT.value);
    auto itr = pairs_tb.find(pair_id);
    liquidity_cost costs;
    costs.cost0 = (uint128_t)liquidity * itr->reserve0.amount / itr->total_liquidity;
    costs.cost1 = (uint128_t)liquidity * itr->reserve1.amount / itr->total_liquidity;
    return costs;
}

