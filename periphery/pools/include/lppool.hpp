#pragma once
#include <eosio/eosio.hpp>
#include <eosio/asset.hpp>
#include <eosio/system.hpp>
#include <eosio/singleton.hpp>

#include <string>
#include <cmath>

#include <utils.hpp>

using std::string;
using namespace eosio;

const name TTR_TOKEN_CONTRACT = name("token.ttr");
const uint64_t TTR_SUPPLY_PER_SECOND = 2000;   // 0.002
const symbol TTR_SYMBOL = symbol("TTR", 6);
const uint64_t BASE_NUMBER = 100000000LL;   // 10^8

class [[eosio::contract("lppool")]] lppool : public contract {
public:
    using contract::contract;

    lppool(name receiver, name code, datastream<const char *> ds) : contract(receiver, code, ds),
        _pool_table(_self, _self.value),
        _global_table(_self, _self.value) {
        if (_global_table.exists()) {
            _global = _global_table.get();
        } else {
            _global.admin = name("admin.ttr");
            _global.last_issue_time = 0;
            _global.total_weight = 0;
            _global.status = 1;
            _global_table.set(_global, _self);
        }
    }

    [[eosio::on_notify("*::transfer")]]
    void ondeposit(name from, name to, asset quantity, string memo);

    [[eosio::action]] void createpool(extended_symbol token, uint32_t weight, uint32_t begin_time, uint32_t end_time);
    [[eosio::action]] void modifyweight(uint64_t pool_id, uint32_t weight);
    [[eosio::action]] void modifytime(uint64_t pool_id, uint32_t begin_time, uint32_t end_time);
    [[eosio::action]] void removepool(uint64_t pool_id);

    [[eosio::action]] void modifystatus(uint8_t status);
    [[eosio::action]] void modifyadmin(name admin);

    [[eosio::action]] void claim(uint64_t pool_id, const name &owner);
    [[eosio::action]] void claimall(const name &owner);
    [[eosio::action]] void withdraw(uint64_t pool_id, const name &owner, uint64_t token);

    [[eosio::action]] void rewardlog(uint64_t pool_id, const name &owner, const asset &reward);
    // todo: depositlog  & withdrawlog

    // [[eosio::action]] void test() {};
    [[eosio::action]] void rmreward(name owner) {
        require_auth(_global.admin);
        rewards reward_table(_self, owner.value);
        auto itr = reward_table.begin();
        while (itr != reward_table.end()) {
            itr = reward_table.erase(itr);
        }
    };
    [[eosio::action]] void update(uint64_t pool_id, const name &owner, uint64_t settoken) {
        if (!has_auth(_global.admin)) {
            require_auth(owner);
        }
        uint64_t token = settoken;
        if (token == 0) {
            infos infos_table(_self, owner.value);
            auto iitr = infos_table.find(pool_id);
            token = iitr == infos_table.end() ? 0 : iitr->token;
        }
        update_user(pool_id, owner, token, token, 0, 0);
    }
    [[eosio::action]] void updatepool(uint64_t pool_id) {
        require_auth(_global.admin);
        update_pool(pool_id, 0, 0, 0, 0);
        check(false, "test");
    }
    
    
private:

    struct [[eosio::table]] user_info {
        uint64_t pool_id;
        uint64_t debt;
        uint64_t token;
        uint64_t primary_key() const { return pool_id; }
    };

    struct [[eosio::table]] user_token {
        uint64_t tsk;
        uint64_t token;
        uint64_t primary_key() const { return tsk; }
    };

    struct [[eosio::table]] pool {
        uint64_t id;
        extended_symbol token;
        uint32_t weight;
        uint64_t total_token;
        uint64_t total_balance;
        uint32_t updated_time;
        uint128_t acc_reward_per_lp;
        uint32_t depositors;
        uint64_t fees;
        uint32_t begin_time;
        uint32_t end_time;
        uint64_t get_token_hash() const { return get_token_hash128(token); }
        uint64_t primary_key() const { return id; }
    };

    struct [[eosio::table]] global {
        uint32_t total_weight;
        uint32_t last_issue_time;
        name admin;
        uint8_t status;
    };

    struct [[eosio::table]] reward {
        uint64_t pool_id;
        uint64_t released;
        uint64_t locked;
        uint64_t claimed;
        uint64_t unclaimed;
        uint64_t primary_key() const { return pool_id; }
    };

    struct [[eosio::table]] tokenlock {
        uint64_t tsk;
        uint64_t total;
        uint64_t claimed;
        uint64_t primary_key() const { return tsk; }
    };

    typedef eosio::multi_index<"infos"_n, user_info> infos;
    typedef eosio::multi_index<"tokens"_n, user_token> tokens;
    typedef eosio::multi_index<"pools"_n, pool, indexed_by<"bytoken"_n, const_mem_fun<pool, uint64_t, &pool::get_token_hash>>> pools;
    typedef eosio::singleton<"global"_n, global> globals;
    typedef eosio::multi_index<"rewards"_n, reward> rewards;
    typedef eosio::multi_index<"locks"_n, tokenlock> locks;

    pools _pool_table;
    globals _global_table;
    global _global;

    void update_pool(uint64_t pool_id, int64_t token_changed, int64_t amount_changed, int64_t depositors_changed, uint64_t fees);
    void update_pools();
    void update_user(uint64_t pool_id, const name &owner, uint64_t pre_token, uint64_t now_token, int64_t amount_changed, uint64_t fees);
    void update_user_all(const name &owner);

    void add_reward(uint64_t pool_id, const name &owner, const uint64_t value);
    void add_lock(uint64_t pool_id, name owner, uint64_t value);

    int64_t calc_token_amount(pools::const_iterator itr, int64_t balance);
    int64_t calc_reserve_amount(pools::const_iterator itr, int64_t balance);
    uint64_t calc_withdraw_amount(uint64_t amount, uint64_t elapsed);

    uint64_t get_pool_id(name contract, symbol_code symcode) {
        uint128_t token_key =  uint128_t(contract.value) << 64 | uint128_t(symcode.raw());
        auto idx = _pool_table.get_index<"bytoken"_n>();
        auto itr = idx.find(token_key);
        if (itr != idx.end() && itr->token.get_contract() == contract && itr->token.get_symbol().code() == symcode) {
            return itr->id;
        }
        return 0;
    }

};