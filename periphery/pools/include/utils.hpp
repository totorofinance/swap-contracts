#pragma once
#include <eosio/eosio.hpp>
#include <eosio/asset.hpp>
#include <structs.hpp>

using std::string;
using namespace eosio;

#define ID_LONG 10000000000LL   // 10^10

void inline_transfer(name contract, name from, name to, asset quantity, string memo) {
    auto data = std::make_tuple(from, to, quantity, memo);
    action(permission_level{from, "active"_n}, contract, "transfer"_n, data).send();
}

uint64_t symbol_to_id(const symbol &sym) {
  string s = sym.code().to_string().substr(2);
  uint64_t num = 0;
  for (int i = 0, j = 1; i < s.size(); i++, j *= 26) {
		num += (int)(s[s.size() - i - 1] - 64) * j;
	}
  return num;
}

uint128_t get_token_hash128(extended_symbol token) {
  return uint128_t(token.get_contract().value) << 64 | uint128_t(token.get_symbol().code().raw());
}

uint64_t get_tsk(uint64_t pool_id, uint64_t ts) {
  return pool_id * ID_LONG + ts / 86400 * 86400;
}

uint64_t get_tsk_week(uint64_t ts) {
    return ts / 604800 * 604800 + 604800;
}

symbol id_to_symbol(uint64_t number) {
    string str = "";
    while (number > 0) {
		int m = number % 26;
		if(m == 0) m = 26;
		str = (char)(m + 64) + str;
		number = (number - m) / 26;
    }
    str = "LP" + str;
    return symbol(str.c_str(), 0);
}

uint64_t get_eos_usdt_price(uint64_t key) {
    // 这边使用Defibox的预言机方案，后续考虑使用更加去中心化的预言机
    avgprices _avgprices(name("oracle.defi"), name("oracle.defi").value);
    auto price_itr = _avgprices.require_find(key, "no price found");
    check(price_itr->avg_price > 0 && price_itr->avg_price < 300000L, "price error"); // < 30 USDT
    return uint64_t(price_itr->avg_price);
}