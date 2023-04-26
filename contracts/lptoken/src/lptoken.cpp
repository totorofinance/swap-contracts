#include <lptoken.hpp>

void lptoken::create(const name &issuer, const asset &maximum_supply) {
    require_auth(SWAP_CONTRACT);

    check(issuer == SWAP_CONTRACT, "wrong issuer");

    auto sym = maximum_supply.symbol;
    check(sym.is_valid(), "invalid symbol name");
    check(maximum_supply.is_valid(), "invalid supply");
    check(maximum_supply.amount > 0, "max-supply must be positive");

    stats statstable(get_self(), sym.code().raw());
    auto existing = statstable.find(sym.code().raw());
    check(existing == statstable.end(), "token with symbol already exists");

    statstable.emplace(get_self(), [&](auto &s) {
        s.supply.symbol = maximum_supply.symbol;
        s.max_supply = maximum_supply;
        s.issuer = issuer;
    });
}

void lptoken::issue(const name &to, const asset &quantity, const string &memo) {
    auto sym = quantity.symbol;
    check(sym.is_valid(), "invalid symbol name");
    check(memo.size() <= 256, "memo has more than 256 bytes");

    stats statstable(_self, sym.code().raw());
    auto existing = statstable.find(sym.code().raw());
    check(existing != statstable.end(), "token with symbol does not exist, create token before issue");
    const auto &st = *existing;

    check(st.issuer == to, "only can issue to issuer");

    require_auth(st.issuer);
    check(quantity.is_valid(), "invalid quantity");
    check(quantity.amount > 0, "must issue positive quantity");

    check(quantity.symbol == st.supply.symbol, "symbol precision mismatch");
    check(quantity.amount <= st.max_supply.amount - st.supply.amount, "quantity exceeds available supply");

    statstable.modify(st, same_payer, [&](auto &s) {
        s.supply += quantity;
    });

    add_balance(st.issuer, quantity, st.issuer);
}

void lptoken::transfer(const name &from, const name &to, const asset &quantity, const string &memo) {
    check(from != to, "cannot transfer to self");
    require_auth(from);
    check(is_account(to), "to account does not exist");
    auto sym = quantity.symbol.code();
    stats statstable(get_self(), sym.raw());
    const auto &st = statstable.get(sym.raw());

    require_recipient(from);
    require_recipient(to);

    check(quantity.is_valid(), "invalid quantity");
    check(quantity.amount > 0, "must transfer positive quantity");
    check(quantity.symbol == st.supply.symbol, "symbol precision mismatch");
    check(memo.size() <= 256, "memo has more than 256 bytes");

    auto payer = has_auth(to) ? to : from;

    auto balance0 = sub_balance(from, quantity);
    auto balance1 = add_balance(to, quantity, payer);

    // change log
    auto data = std::make_tuple(quantity.symbol.code(), from, to, quantity.amount, balance0, balance1);
    action(permission_level{_self, "active"_n}, LPNOTIFY_CONTRACT, "transferlog"_n, data).send();
}

uint64_t lptoken::sub_balance(const name &owner, const asset &value) {
    accounts from_acnts(get_self(), owner.value);

    const auto &from = from_acnts.get(value.symbol.code().raw(), "no balance object found");

    auto balance = from.balance.amount;
    check(balance >= value.amount, "overdrawn balance");

    if (balance - value.amount == 0) {
        from_acnts.erase(from);
    } else {
        auto costs = get_costs(value.symbol.code(), value.amount);
        from_acnts.modify(from, owner, [&](auto &a) {
            a.balance -= value;
            a.cost0 -= costs.cost0;
            a.cost1 -= costs.cost1;
        });
    }

    if (owner != _self && owner != SWAP_CONTRACT) {
        // notify pools
        auto data = std::make_tuple(_self, value.symbol.code(), owner, balance, balance - value.amount);
        action(permission_level{_self, "active"_n}, LPNOTIFY_CONTRACT, "tokenchange"_n, data).send();

    }
    return balance - value.amount;
}

uint64_t lptoken::add_balance(const name &owner, const asset &value, const name &ram_payer) {
    accounts to_acnts(get_self(), owner.value);
    auto to = to_acnts.find(value.symbol.code().raw());
    uint64_t pre_amount = 0;
    auto costs = get_costs(value.symbol.code(), value.amount);
    if (to == to_acnts.end()) {
        to_acnts.emplace(ram_payer, [&](auto &a) {
            a.balance = value;
            a.cost0 = costs.cost0;
            a.cost1 = costs.cost1;
        });
    } else {
        pre_amount = to->balance.amount;
        to_acnts.modify(to, same_payer, [&](auto &a) {
            a.balance += value;
            a.cost0 += costs.cost0;
            a.cost1 += costs.cost1;
        });
    }

    if (owner != _self && owner != SWAP_CONTRACT) {
        // notify pools
        auto data = std::make_tuple(_self, value.symbol.code(), owner, pre_amount, pre_amount + value.amount);
        action(permission_level{_self, "active"_n}, LPNOTIFY_CONTRACT, "tokenchange"_n, data).send();
    }
    return pre_amount + value.amount;
}

void lptoken::retire(const asset &quantity, const string &memo) {
    auto sym = quantity.symbol;
    check(sym.is_valid(), "invalid symbol name");
    check(memo.size() <= 256, "memo has more than 256 bytes");

    stats statstable(get_self(), sym.code().raw());
    auto existing = statstable.find(sym.code().raw());
    check(existing != statstable.end(), "token with symbol does not exist");
    const auto &st = *existing;

    require_auth(st.issuer);
    check(quantity.is_valid(), "invalid quantity");
    check(quantity.amount > 0, "must retire positive quantity");

    check(quantity.symbol == st.supply.symbol, "symbol precision mismatch");

    statstable.modify(st, same_payer, [&](auto &s) {
        s.supply -= quantity;
    });

    sub_balance(st.issuer, quantity);
}

void lptoken::destroy(symbol_code code) {
    require_auth(SWAP_CONTRACT);
    stats statstable(get_self(), code.raw());
    auto itr = statstable.require_find(code.raw(), "token does not exists");
    check(itr->supply.amount == 0, "Can not destroy non-empty token");
    statstable.erase(itr);
}
