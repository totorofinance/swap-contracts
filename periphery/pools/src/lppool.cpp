#include <lppool.hpp>
#include <safemath.hpp>

void lppool::createpool(extended_symbol token, uint32_t weight, uint32_t begin_time, uint32_t end_time) {
    require_auth(_global.admin);
    check(_global.status == 1, "create deos not enable now");

    check(weight > 0, "weight need greater than 0");
    
    auto symcode = token.get_symbol().code();
    stats stattabl(token.get_contract(), symcode.raw());
    stattabl.require_find(symcode.raw(), "token does not exists");

    uint64_t pool_id = get_pool_id(token.get_contract(), symcode);
    check(pool_id == 0, "pool token already exists");

    // update
    update_pools();

    // create pool
    pool_id = _pool_table.available_primary_key();
    if (pool_id == 0) {
        pool_id = 1;
    }
    auto now_time = current_time_point().sec_since_epoch();
    _pool_table.emplace(_self, [&](auto &a) {
        a.id = pool_id;
        a.token = token;
        a.weight = weight;
        a.acc_reward_per_lp = 0;
        a.updated_time = now_time > begin_time ? now_time : begin_time;
        a.begin_time = begin_time;
        a.end_time = end_time;
    });

    _global.total_weight += weight;
    _global_table.set(_global, _self);
}

void lppool::modifyweight(uint64_t pool_id, uint32_t weight) {
    require_auth(_global.admin);
    check(_global.status == 1, "modify pool deos not enable now");

    // update
    update_pools();

    auto p_itr = _pool_table.require_find(pool_id, "pool does not exists");

    _global.total_weight -= p_itr->weight;
    _global.total_weight += weight;
    _global_table.set(_global, _self);

    _pool_table.modify(p_itr, same_payer, [&](auto &a) {
        a.weight = weight;
    });

}

void lppool::modifytime(uint64_t pool_id, uint32_t begin_time, uint32_t end_time) {
    require_auth(_global.admin);
    check(_global.status == 1, "modify pool deos not enable now");

    // update
    update_pools();

    auto p_itr = _pool_table.require_find(pool_id, "pool does not exists");

    // todo: 这边要重新判断，开始后不能修改开始时间（只允许在开始前或者结束后）  updated_time > begin_time  -> begin_time
    auto now_time = time_point_sec(current_time_point()).sec_since_epoch();
    auto is_started = now_time >= p_itr->begin_time;
    auto is_stoped = now_time > p_itr->end_time;
    check(end_time > now_time, "end time must be greater than now");
    check(begin_time < end_time, "end time must be greater than start time");
    if (begin_time > p_itr->updated_time) {
        // check(is_stoped, "only modify begin time when stoped");
    }
    _pool_table.modify(p_itr, same_payer, [&](auto &a) {
        a.begin_time = begin_time;
        a.end_time = end_time;
        if (!is_started || (is_stoped && begin_time > a.updated_time)) {
            // check(false, "update updated_time");
            a.updated_time = begin_time;
        }
    });
}

void lppool::removepool(uint64_t pool_id) {
    require_auth(_global.admin);
    check(_global.status == 1, "remove pool deos not enable now");

    auto p_itr = _pool_table.require_find(pool_id, "pool does not exists");
    check(p_itr->weight == 0, "only pools with a weight of 0 can be deleted");

    check(p_itr->depositors == 0, "only pools without participants can be deleted");
    _pool_table.erase(p_itr);
}

void lppool::claim(uint64_t pool_id, const name &owner) {

    check(_global.status == 1, "claim deos not enable now");
    require_auth(owner);

    infos infos_table(_self, owner.value);
    auto iitr = infos_table.find(pool_id);
    uint64_t token = iitr == infos_table.end() ? 0 : iitr->token;
    update_user(pool_id, owner, token, token, 0, 0);
    
    rewards reward_table(_self, owner.value);
    auto itr = reward_table.require_find(pool_id, "no reward");
    uint64_t reward_amount = itr->released;
    check(reward_amount > 0, "no reward");

    uint64_t released = itr->unclaimed * 4 / 10;
    uint64_t locked = itr->unclaimed - released;

    add_lock(pool_id, owner, locked);

    reward_table.modify(itr, same_payer, [&](auto &a) {
        a.claimed += a.unclaimed;
        a.released += released;
        a.locked += locked;
        a.unclaimed = 0;
    });

    

    print_f("reward %", asset(reward_amount, TTR_SYMBOL));

    // transfer to user
    inline_transfer(TTR_TOKEN_CONTRACT, _self, owner, asset(reward_amount, TTR_SYMBOL), "TTR reward");
}

void lppool::claimall(const name &owner) {
    check(_global.status == 1, "claim deos not enable now");
    require_auth(owner);

    update_user_all(owner);
    
    rewards reward_table(_self, owner.value);
    
    uint64_t reward_amount = 0;

    auto itr = reward_table.begin();
    while (itr != reward_table.end()) {
        uint64_t released = itr->unclaimed * 4 / 10;
        uint64_t locked = itr->unclaimed - released;

        reward_amount += released;
        add_lock(itr->pool_id, owner, locked);

        reward_table.modify(itr, same_payer, [&](auto &a) {
            a.claimed += a.unclaimed;
            a.released += released;
            a.locked += locked;
            a.unclaimed = 0;
        });
        itr++;
    }

    // release locks
    locks lock_table(_self, owner.value);
    auto itr2 = lock_table.begin();
    uint64_t nowts = current_time_point().sec_since_epoch();
    while (itr2 != lock_table.end()) {
        if (itr2->tsk <= nowts) {
            reward_amount += (itr2->total - itr2->claimed);
            itr2 = lock_table.erase(itr2);
        } else {
            uint64_t released = uint128_t(itr2->tsk - nowts) * itr2->total / (86400 * 180);
            if (released > itr2->claimed) {
                uint64_t claimed = released - itr2->claimed;
                reward_amount += claimed;
                lock_table.modify(itr2, same_payer, [&](auto &a) {
                    a.claimed += claimed;
                });
            }
            
            itr2++;
        }
    }

    check(reward_amount > 0, "no reward");

    print_f("reward %", asset(reward_amount, TTR_SYMBOL));

    // transfer to user
    inline_transfer(TTR_TOKEN_CONTRACT, _self, owner, asset(reward_amount, TTR_SYMBOL), "TTR reward");
}

void lppool::withdraw(uint64_t pool_id, const name &owner, uint64_t token) {
    check(_global.status == 1, "withdraw deos not enable now");
    require_auth(owner);

    uint64_t withdraw_tokens = token;

    auto p_itr = _pool_table.require_find(pool_id, "pool not found");

    tokens tokens_table(_self, owner.value);
    auto ut_itr = tokens_table.begin();
    uint64_t total_withdraw_tokens = 0;
    uint64_t real_withdraw_tokens = 0;
    uint64_t nowts = current_time_point().sec_since_epoch();
    while (withdraw_tokens > 0 && ut_itr != tokens_table.end()) {
        uint64_t elapsed = nowts - ut_itr->tsk % ID_LONG;
        uint64_t item_tokens = ut_itr->token;
        if (withdraw_tokens >= item_tokens) {
            withdraw_tokens -= item_tokens;
            ut_itr = tokens_table.erase(ut_itr);
        } else {
            item_tokens = withdraw_tokens;
            withdraw_tokens = 0;
            tokens_table.modify(ut_itr, same_payer, [&](auto &a) {
                a.token -= item_tokens;
            });
        }
        total_withdraw_tokens += item_tokens;
        real_withdraw_tokens += calc_withdraw_amount(item_tokens, elapsed);
    }
    check(withdraw_tokens == 0, "insufficient balance");
    check(real_withdraw_tokens > 0, "no tokens found");
    
    int64_t withdraw_amount = calc_reserve_amount(p_itr, real_withdraw_tokens);
    uint64_t fees = calc_reserve_amount(p_itr, total_withdraw_tokens - real_withdraw_tokens);
    

    infos infos_table(_self, owner.value);
    auto iitr = infos_table.require_find(pool_id);
    update_user(pool_id, owner, iitr->token, iitr->token - total_withdraw_tokens, withdraw_amount * -1, fees);

    // check(false, string("amount:") + std::to_string(withdraw_amount));

    // transfer to user
    inline_transfer(p_itr->token.get_contract(), _self, owner, asset(withdraw_amount, p_itr->token.get_symbol()), "withdraw");
}

void lppool::ondeposit(name from, name to, asset quantity, string memo) {
    if (from == _self || to != _self) {
        return;
    }
    check(_global.status == 1, "deposit deos not enable now");
    if (from == name("token.ttr")) {
        return;
    }

    auto code = get_first_receiver();
    auto pool_id = get_pool_id(code, quantity.symbol.code());
    check(pool_id > 0, "pool not found");

    auto p_itr = _pool_table.find(pool_id);

    // save balance
    auto token_amount = calc_token_amount(p_itr, quantity.amount);

    tokens baltbl(_self, from.value);
    auto tsk = get_tsk(pool_id, current_time_point().sec_since_epoch());
    auto b_itr = baltbl.find(tsk);
    uint64_t pre_token = 0;
    if (b_itr == baltbl.end()) {
        baltbl.emplace(_self, [&](auto &a) {
            a.tsk = tsk;
            a.token = token_amount;
        });
    } else {
        pre_token = b_itr->token;
        baltbl.modify(b_itr, same_payer, [&](auto &a) {
           a.token += token_amount;
        });
    }

    // check(false, string("~") + std::to_string(tsk));

    // update
    update_user(pool_id, from, pre_token, pre_token + token_amount, quantity.amount, 0);

}

void lppool::rewardlog(uint64_t pool_id, const name &owner, const asset &reward) {
    require_auth(_self);
}


void lppool::update_user(uint64_t pool_id, const name &owner, uint64_t pre_token, uint64_t now_token, int64_t amount_changed, uint64_t fees) {
    
    auto p_itr = _pool_table.find(pool_id);
    if (p_itr == _pool_table.end()) {
        // 如果没有pool，不需要update
        return;
    }

    infos infos_table(_self, owner.value);
    auto ur_itr = infos_table.find(pool_id);
    if (ur_itr == infos_table.end()) {
        ur_itr = infos_table.emplace(_self, [&](auto &a) {
            a.pool_id = pool_id;
            a.debt = 0;
        });
    }

    int64_t depositors_changed = 0;
    if (pre_token == 0 && now_token > 0) {
        depositors_changed = 1;
    } else if (pre_token > 0 && now_token == 0) {
        depositors_changed = -1;
    }
    int64_t token_changed = int64_t(now_token) - pre_token;
    // check(false, string("token_changed:") + std::to_string(token_changed));

    update_pool(pool_id, token_changed, amount_changed, depositors_changed, fees);
    

    uint128_t reward = safemath128::mul(pre_token, p_itr->acc_reward_per_lp) / BASE_NUMBER;
    check(reward <= asset::max_amount, "reward too large");
    
    if (reward > ur_itr->debt) {
        // add reward to user
        uint64_t pending = (uint64_t)reward - ur_itr->debt;
        add_reward(p_itr->id, owner, pending);
    }

    // check(false, string("now_token:") + std::to_string(now_token) + ","  + std::to_string(uint64_t(p_itr->acc_reward_per_lp)));
    
    if (now_token == 0) {
        infos_table.erase(ur_itr);
    } else {
        infos_table.modify(ur_itr, same_payer, [&](auto &a) {
            a.debt = now_token * p_itr->acc_reward_per_lp / BASE_NUMBER;
            a.token = now_token;
        });
    }
    // check(false, "error here");

}

void lppool::update_user_all(const name &owner) {
    infos infos_table(_self, owner.value);
    auto itr = infos_table.begin();
    while (itr != infos_table.end()) {
        update_user(itr->pool_id, owner, itr->token, itr->token, 0, 0);
        itr++;
    }
}

void lppool::update_pools() {
    auto itr = _pool_table.begin();
    while (itr != _pool_table.end()) {
        update_pool(itr->id, 0, 0, 0, 0);
        itr++;
    }
}

void lppool::update_pool(uint64_t pool_id, int64_t token_changed, int64_t amount_changed, int64_t depositors_changed, uint64_t fees) {
    auto p_itr = _pool_table.find(pool_id);
    if (p_itr == _pool_table.end()) {
        return;
    }

    if (token_changed < 0 || amount_changed < 0) {
        uint64_t practical_amount = calc_reserve_amount(p_itr, token_changed * -1);
        print_f("token %, amount %, fees %", token_changed * -1, amount_changed * -1, fees);
        check(p_itr->total_token >= token_changed * -1, "error token");
        check(p_itr->total_balance >= amount_changed * -1, "error amount");
    }

    auto current_time =  current_time_point().sec_since_epoch();
    bool update_reward = current_time > p_itr->begin_time && current_time > p_itr->updated_time && p_itr->total_token > 0;

    if (current_time < p_itr->begin_time) {
        // 尚未开始
        current_time = p_itr->begin_time;
    }
    if (current_time > p_itr->end_time) {
        // 已经结束
        current_time = p_itr->end_time;
    }

    uint128_t ttr_reward = 0;
    if (update_reward) {
        uint32_t seconds = current_time - p_itr->updated_time;
        if (p_itr->weight > 0 && _global.total_weight > 0) {
            uint128_t ttr_issued = safemath128::mul((uint128_t)seconds, TTR_SUPPLY_PER_SECOND);
            ttr_reward =  safemath128::mul(ttr_issued, p_itr->weight) / _global.total_weight;
        }

        // issue TTR
        if (_global.last_issue_time == 0) {
            _global.last_issue_time = p_itr->begin_time;
        }
        // check(false, std::to_string(current_time - _global.last_issue_time));
        if (current_time > _global.last_issue_time) {
            auto issue_amount = TTR_SUPPLY_PER_SECOND * (current_time - _global.last_issue_time);
            auto issue_quantity = asset(issue_amount, TTR_SYMBOL);
            _global.last_issue_time = current_time;
            _global_table.set(_global, _self);

            auto data = std::make_tuple(_self, issue_quantity, string("liquidity mine"));
            action(permission_level{_self, "active"_n}, TTR_TOKEN_CONTRACT, "issue"_n, data).send();
        }
    }

    // update
    _pool_table.modify(p_itr, same_payer, [&](auto &a) {
        if (a.total_token > 0) {
            a.acc_reward_per_lp = safemath128::add(a.acc_reward_per_lp, safemath128::mul(ttr_reward, BASE_NUMBER) / a.total_token);
        } else {
            a.acc_reward_per_lp = 0;
        }
        a.depositors += depositors_changed;
        a.total_token += token_changed;
        if (a.depositors == 0) {
            a.total_token = 0;
        }
        a.total_balance += amount_changed;
        a.fees += fees;
        a.updated_time = current_time;
    });
    print_f("acc_reward_per_lp %\n", p_itr->acc_reward_per_lp);
}

void lppool::add_reward(uint64_t pool_id, const name &owner, const uint64_t value) {
    if (value == 0) {
        return;
    }
    printf("reward: %s %s\n", owner.to_string().c_str(), asset(value, TTR_SYMBOL).to_string().c_str());

    rewards reward_table(_self, owner.value);
    auto ritr = reward_table.find(pool_id);
    if (ritr == reward_table.end()) {
        reward_table.emplace(_self, [&](auto &a) {
            a.pool_id = pool_id;
            a.unclaimed = value;
        });
    } else {
        reward_table.modify(ritr, same_payer, [&](auto &a) {
            a.unclaimed += value;
        });
    }
   
    auto data = std::make_tuple(pool_id, owner, asset(value, TTR_SYMBOL));
    action(permission_level{_self, "active"_n}, _self, "rewardlog"_n, data).send();
}

void lppool::add_lock(uint64_t pool_id, name owner, uint64_t value) {
    locks lock_table(_self, owner.value);
    uint64_t expire_ts = current_time_point().sec_since_epoch() + 86400 * 180; // 锁定180天
    auto tsk = get_tsk_week(expire_ts);
    auto itr = lock_table.find(tsk);
    if (itr == lock_table.end()) {
        lock_table.emplace(_self, [&](auto &a) {
            a.tsk = tsk;
            a.claimed = 0;
            a.total = value;
        });
    } else {
        lock_table.modify(itr, same_payer, [&](auto &a) {
            a.total += value;
        });
    }
}

void lppool::modifystatus(uint8_t status) {
    require_auth(_global.admin);

    _global.status = status;
    _global_table.set(_global, _self);
}

void lppool::modifyadmin(name admin) {
    require_auth(_global.admin);

    _global.admin = admin;
    _global_table.set(_global, _self);
}

int64_t lppool::calc_token_amount(pools::const_iterator itr, int64_t balance) {
    if (itr->total_token == 0 || itr->total_balance == 0) {
        return balance * 100;
    }
    uint64_t rate = uint128_t(itr->total_token) * 100000000LL / itr->total_balance;
    return uint128_t(balance) * rate / 100000000LL;
}

int64_t lppool::calc_reserve_amount(pools::const_iterator itr, int64_t token) {
    if (itr->total_balance == 0) {
        return 0;
    }
    uint64_t rate = uint128_t(itr->total_token) * 100000000LL / itr->total_balance;
    return uint128_t(token) * 100000000LL / rate;
}

uint64_t lppool::calc_withdraw_amount(uint64_t amount, uint64_t elapsed) {
    uint128_t ratio = 100000;
    if (elapsed < 604800) {
        ratio = 99000;
    } else if (elapsed < 1209600) {
        ratio = 99500;
    } else if (elapsed < 1814400) {
        ratio = 99250;
    } else if (elapsed < 2419200) {
        ratio = 99875;
    }
    return amount * ratio / 100000LL;
}
