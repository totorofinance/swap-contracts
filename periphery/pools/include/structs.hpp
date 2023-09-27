#pragma once
#include <eosio/eosio.hpp>
#include <eosio/asset.hpp>

using std::string;
using namespace eosio;

struct account {
    asset balance;
    uint64_t primary_key() const { return balance.symbol.code().raw(); }
};
struct currency_stats {
    asset supply;
    asset max_supply;
    name issuer;
    uint64_t primary_key() const { return supply.symbol.code().raw(); }
};
struct pair {
    uint64_t id;
    symbol_code code;
    extended_symbol token0;
    extended_symbol token1;
    asset reserve0;
    asset reserve1;
    uint64_t total_liquidity;
    uint32_t last_update_time;
    uint32_t create_time;
    uint64_t primary_key() const { return id; }
};

struct avgprice {
    uint64_t key;
    name submitter;
    string period;
    uint64_t acc_price;
    uint64_t avg_price;
    time_point_sec last_update;
    uint64_t primary_key() const { return key; }
};


typedef eosio::multi_index<"accounts"_n, account> accounts;
typedef eosio::multi_index<"stat"_n, currency_stats> stats;
typedef multi_index<"pairs"_n, pair> pairs;
typedef multi_index<"avgprices"_n, avgprice> avgprices;