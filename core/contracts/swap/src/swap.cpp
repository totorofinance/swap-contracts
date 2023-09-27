#include <swap.hpp>
#include <eosio/system.hpp>

ACTION swap::createpair(name creator, extended_symbol token0, extended_symbol token1) {
    require_auth(creator);
    check(get_config("status") == 1, "contract under maintenance");
    check(token0 != token1, "can not submit with same token");
    check(token0.get_contract() != LP_TOKEN_CONTRACT && token1.get_contract() != LP_TOKEN_CONTRACT, "can not create lp token pool now");
    auto supply0 = get_supply(token0.get_contract(), token0.get_symbol().code());
    auto supply1 = get_supply(token1.get_contract(), token1.get_symbol().code());
    check(supply0.symbol == token0.get_symbol() && supply1.symbol == token1.get_symbol(), "invalid symbol");

    auto hash = str_hash(token0, token1);
    auto byhash_idx = _pairs.get_index<"byhash"_n>();
    auto itr = byhash_idx.find(hash);
    // check(false, itr->code.to_string() + ":" + std::to_string(hash) + "," + std::to_string(itr->hash()));
    while (itr != byhash_idx.end() && itr->hash() == hash) {
        check(!is_same_pair(token0, token1, itr->token0, itr->token1), "pair already exists");
        itr++;
    }

    auto pair_id = _pairs.available_primary_key();
    if (pair_id == 0) {
        pair_id = 1;
    }
    
    auto sym = symbol(id2code(pair_id), 0);
    auto timestamp = current_block_time();
    _pairs.emplace(creator, [&](auto &a) {
        a.id = pair_id;
        a.code = sym.code();
        a.token0 = token0;
        a.token1 = token1;
        a.reserve0.symbol = token0.get_symbol();
        a.reserve1.symbol = token1.get_symbol();
        a.created_time = timestamp;
        a.updated_time = timestamp;
    });

    // create log
    auto logdata = std::make_tuple(pair_id, creator, token0, token1);
    action(permission_level{_self, "active"_n}, LOG_CONTRACT, "createpair"_n, logdata).send();

    // create symbol
    auto max_supply = asset(1000000000000000000, sym);
    auto create_data = std::make_tuple(_self, max_supply);
    action(permission_level{_self, "active"_n}, LP_TOKEN_CONTRACT, "create"_n, create_data).send();
}

ACTION swap::removepair(uint64_t pair_id) {
    require_auth(get_manager());
    check(get_config("status") == 1, "contract under maintenance");
    auto p_itr = _pairs.require_find(pair_id, "pair does not exist");
    check(p_itr->total_liquidity == 0, "unable to remove active pair");
    
    auto now_time = current_time_point().sec_since_epoch();
    auto create_time = p_itr->created_time.to_time_point().sec_since_epoch();
    // check(now_time - create_time > 60, "Please do not remove pair immediately"); // only admin can call now

    deposits_mi deposits(_self, pair_id);
    check(deposits.begin() == deposits.end(), "can not remove the pair which still have deposits");

    // remove lptoken
    action(permission_level{_self, "active"_n}, LP_TOKEN_CONTRACT, "destroy"_n, std::make_tuple(p_itr->code)).send();

    _pairs.erase(p_itr);
}

ACTION swap::refund(name owner, uint64_t pair_id) {
    require_auth(owner);

    check(get_config("status") == 1, "contract under maintenance");
    deposits_mi deposits(_self, pair_id);
    auto itr = deposits.require_find(owner.value, "you don't have any deposit");
    auto p_itr = _pairs.require_find(pair_id, "pair does not exist");

    if (itr->quantity0.amount > 0) {
        transfer_to(p_itr->token0.get_contract(), owner, itr->quantity0, std::string("Totoro cancel refund"));
    }
    if (itr->quantity1.amount > 0) {
        transfer_to(p_itr->token1.get_contract(), owner, itr->quantity1, std::string("Totoro cancel refund"));
    }
    deposits.erase(itr);
}

ACTION swap::setconfig(name key, uint64_t value) {
    require_auth(get_manager());
    auto itr = _configs.find(name(key).value);
    if (itr == _configs.end()) {
        _configs.emplace(_self, [&](auto &a) {
            a.key = key;
            a.value = value;
        });
    } else {
        _configs.modify(itr, same_payer, [&](auto &a) {
            a.value = value;
        });
    }
}

ACTION swap::setname(name key, name value) {
    setconfig(key, value.value);
}


void swap::handle_transfer(name from, name to, asset quantity, string memo) {
    name code = get_first_receiver();
    if (from == _self || to != _self || from == name("fix.ttr")) {
        return;
    }
    require_auth(from);
    check(get_config("status") == 1, "contract under maintenance");
    if (code == name(LP_TOKEN_CONTRACT)) {
        handle_rmliquidity(from, code, quantity);
        return;
    }
    
    std::map<string, string> dict = mappify(memo);
    check(dict.size() >= 1, "invaild memo");
    if (dict.find("deposit") != dict.end()) {
        string sid = dict.find("deposit")->second;
        uint64_t pid = strtoull(sid.c_str(), NULL, 10);
        handle_deposit(pid, from, code, quantity);
    } else if (dict.find("swap") != dict.end()) {
        uint64_t min_amount = 0;
        auto itr = dict.find("min");
        if (itr != dict.end()) {
            min_amount = strtoull(itr->second.c_str(), NULL, 10);
        }
        string path = dict.find("swap")->second;
        
        std::vector<uint64_t> ids = split_ids(path, "-");
        handle_swap(ids, from, code, quantity, min_amount);
    } else {
        check(false, "invalid memo");
    }
}

void swap::handle_swap(std::vector<uint64_t> ids, name from, name contract, asset quantity, uint64_t min_amount) {
    extended_asset to_asset(quantity, contract);
    for (uint8_t i = 0; i < ids.size(); i++) {
        auto pair_id = ids[i];
        auto from_asset = to_asset;
        to_asset = swap_token(ids[i], from, from_asset.contract, from_asset.quantity);
        if (get_config("mine.status") == 1) {
             action(permission_level{_self, "active"_n}, get_account("mine.account"), name("mine"), std::make_tuple(from, pair_id, from_asset, to_asset)).send();
        }
    }
    check(min_amount == 0 || to_asset.quantity.amount >= min_amount, "returns less than expected");

    
    double amount_in_f = quantity.amount * 1.0 / pow(10, quantity.symbol.precision());
    double amount_out_f = to_asset.quantity.amount * 1.0  / pow(10, to_asset.quantity.symbol.precision());
    double price_f = amount_out_f / amount_in_f;
    string memo = string_format(
        string("Totoro swap from %s to %s with price %.8lf"), 
        quantity.symbol.code().to_string().c_str(), 
        to_asset.quantity.symbol.code().to_string().c_str(),
        price_f
    );
    transfer_to(to_asset.contract, from, to_asset.quantity, memo); 
}

void swap::handle_rmliquidity(name owner, name contract, asset quantity) {
    uint64_t pair_id = code2id(quantity.symbol.code().to_string().substr(2));
    auto data = make_tuple(quantity, string("retire LP token"));
    action(permission_level{_self, "active"_n}, LP_TOKEN_CONTRACT, "retire"_n, data).send();
    remove_liquidity(owner, pair_id, quantity.amount);
}

void swap::remove_liquidity(name owner, uint64_t pair_id, uint64_t amount) {
    require_auth(owner);
    check(get_config("status") == 1, "contract under maintenance");
    auto p_itr = _pairs.require_find(pair_id, "pair does not exist");
    uint64_t reserve0 = p_itr->reserve0.amount;
    uint64_t reserve1 = p_itr->reserve1.amount;
    uint64_t amount0 = (uint128_t)reserve0 * amount / p_itr->total_liquidity;
    uint64_t amount1 = (uint128_t)reserve1 * amount / p_itr->total_liquidity;
    check(amount0 > 0 && amount1 > 0 && amount0 <= reserve0 && amount1 <= reserve1, "insufficient liquidity");

    _pairs.modify(p_itr, same_payer, [&](auto &a) {
        a.total_liquidity = safe_sub(a.total_liquidity, amount);
    });

    transfer_to(p_itr->token0.get_contract(), owner, asset(amount0, p_itr->token0.get_symbol()), string("Totoro remove liquidity"));
    transfer_to(p_itr->token1.get_contract(), owner, asset(amount1, p_itr->token1.get_symbol()), string("Totoro remove liquidity"));

    uint64_t balance0 = safe_sub(reserve0, amount0);
    uint64_t balance1 = safe_sub(reserve1, amount1);
    update_pair(pair_id, balance0, balance1, reserve0, reserve1);

    // rmliquidity log
    auto quantity0 = asset(amount0, p_itr->token0.get_symbol());
    auto quantity1 = asset(amount1, p_itr->token1.get_symbol());
    auto data = std::make_tuple(pair_id, owner, amount, quantity0, quantity1, p_itr->total_liquidity, p_itr->token0.get_contract(), p_itr->token1.get_contract(), p_itr->reserve0, p_itr->reserve1);
    action(permission_level{_self, "active"_n}, LOG_CONTRACT, "rmliquidity"_n, data).send();
}

void swap::handle_deposit(uint64_t pair_id, name owner, name contract, asset quantity) {
    auto p_itr = _pairs.require_find(pair_id, "pair does not exist");

    extended_symbol input(quantity.symbol, contract);
    check(input == p_itr->token0 || input == p_itr->token1, "invalid deposit");

    deposits_mi deposits(_self, pair_id);
    auto itr = deposits.find(owner.value);
    if (itr == deposits.end()) {
        itr = deposits.emplace(_self, [&](auto &a) {
            a.owner = owner;
            a.quantity0.symbol = p_itr->token0.get_symbol();
            a.quantity1.symbol = p_itr->token1.get_symbol();
        });
    }
    deposits.modify(itr, same_payer, [&](auto &a) {
        if (input == p_itr->token0) {
            a.quantity0 += quantity;
        } else if (input == p_itr->token1) {
            a.quantity1 += quantity;
        }
    });
}

void swap::addliquidity(name owner, uint64_t pair_id) {
    deposits_mi deposits(_self, pair_id);
    auto d_itr = deposits.require_find(owner.value, "you don't have any deposit");
    auto p_itr = _pairs.require_find(pair_id, "pair does not exist");
    check(d_itr->quantity0.amount > 0 && d_itr->quantity1.amount > 0, "you need have both tokens");

    uint64_t amount0 = d_itr->quantity0.amount;
    uint64_t amount1 = d_itr->quantity1.amount;
    uint64_t amount0_refund = 0;
    uint64_t amount1_refund = 0;
    uint64_t reserve0 = p_itr->reserve0.amount;
    uint64_t reserve1 = p_itr->reserve1.amount;

    // calc amount0 and amount1
    if (reserve0 > 0 || reserve1 > 0) {
        uint128_t amount_temp = (uint128_t)amount0 * reserve1 / reserve0;
        check(amount_temp < asset::max_amount, "input amount too large");
        uint64_t amount1_matched = amount_temp;
        if (amount1_matched <= amount1) {
            amount1_refund = amount1 - amount1_matched;
            amount1 = amount1_matched;
        } else {
            amount_temp = (uint128_t)amount1 * reserve0 / reserve1;
            check(amount_temp < asset::max_amount, "input amount too large");
            uint64_t amount0_matched = amount_temp;
            amount0_refund = amount0 - amount0_matched;
            amount0 = amount0_matched;
        }
    }

    // calc liquidity
    uint64_t liquidity = 0;
    uint64_t total_liquidity = p_itr->total_liquidity;
    if (total_liquidity == 0) {
        liquidity = sqrt((uint128_t)amount0 * amount1);
        check(liquidity >= MINIMUM_LIQUIDITY, "insufficient liquidity minted");
    } else {
        liquidity = std::min((uint128_t)amount0 * total_liquidity / reserve0, (uint128_t)amount1 * total_liquidity / reserve1);
    }

    deposits.erase(d_itr);

    _pairs.modify(p_itr, same_payer, [&](auto &a) {
        a.total_liquidity = safe_add(a.total_liquidity, liquidity);
    });

    uint64_t balance0 = safe_add(reserve0, amount0);
    uint64_t balance1 = safe_add(reserve1, amount1);
    update_pair(pair_id, balance0, balance1, reserve0, reserve1);

    // refund
    if (amount0_refund > 0) {
        transfer_to(p_itr->token0.get_contract(), owner, asset(amount0_refund, p_itr->token0.get_symbol()), string("Totoro deposit refund"));
    }
    if (amount1_refund > 0) {
        transfer_to(p_itr->token1.get_contract(), owner, asset(amount1_refund, p_itr->token1.get_symbol()), string("Totoro deposit refund"));
    }

    // issue lp tokens
    auto lp_issue = asset(liquidity, symbol(p_itr->code, 0));
    auto data = make_tuple(_self, lp_issue, string("Issue LP token"));
    action(permission_level{_self, "active"_n}, LP_TOKEN_CONTRACT, "issue"_n, data).send();
    inline_transfer(LP_TOKEN_CONTRACT, _self, owner, lp_issue, string("Issue LP token"));

    // addliquidity log
    auto quantity0 = asset(amount0, p_itr->token0.get_symbol());
    auto quantity1 = asset(amount1, p_itr->token1.get_symbol());
    auto data2 = std::make_tuple(pair_id, owner, liquidity, quantity0, quantity1, p_itr->total_liquidity, p_itr->token0.get_contract(), p_itr->token1.get_contract(), p_itr->reserve0, p_itr->reserve1);
    action(permission_level{_self, "active"_n}, LOG_CONTRACT, "addliquidity"_n, data2).send();

} 

extended_asset swap::swap_token(uint64_t pair_id, name from, name contract, asset quantity) {
    auto p_itr = _pairs.require_find(pair_id, "pair does not exist");
    bool is_token0 = contract == p_itr->token0.get_contract() && quantity.symbol == p_itr->token0.get_symbol();
    bool is_token1 = contract == p_itr->token1.get_contract()  && quantity.symbol == p_itr->token1.get_symbol();
    check(is_token0 || is_token1, "contract or symbol error");

    uint64_t amount_in = quantity.amount;
    uint64_t protocol_fee = (uint128_t)amount_in *  get_config("fee.protocol") / 10000;
    uint64_t trade_fee = (uint128_t)amount_in *  get_config("fee.trade") / 10000;
    
    check(trade_fee + protocol_fee > 0, "swap amount too small");
    if (protocol_fee > 0) {
        amount_in -= protocol_fee;
        transfer_to(contract, get_account("fee.account"), asset(protocol_fee, quantity.symbol), string("Totoro swap protocol fee"));
    }
    
    uint64_t reserve0 = p_itr->reserve0.amount;
    uint64_t reserve1 = p_itr->reserve1.amount;
    uint64_t amount_out = 0;
    extended_asset output;
    uint64_t balance0;
    uint64_t balance1;
    if (is_token0) {
        amount_out = get_output(amount_in, reserve0, reserve1, trade_fee);
        check(amount_out >= 0, "insufficient output amount");
        output.contract = p_itr->token1.get_contract();
        output.quantity = asset(amount_out, p_itr->token1.get_symbol());
        balance0 = safe_add(reserve0, amount_in);
        balance1 = safe_sub(reserve1, amount_out);
    } else {
        amount_out = get_output(amount_in, reserve1, reserve0, trade_fee);
        check(amount_out >= 0, "insufficient output amount");
        output.contract = p_itr->token0.get_contract();
        output.quantity = asset(amount_out, p_itr->token0.get_symbol());
        balance0 = safe_sub(reserve0, amount_out);
        balance1 = safe_add(reserve1, amount_in);
    }
    update_pair(pair_id, balance0, balance1, reserve0, reserve1);

    // logs
    double amount_in_f = amount_in * 1.0 / pow(10, quantity.symbol.precision());
    double amount_out_f = amount_out * 1.0  / pow(10, output.quantity.symbol.precision());
    double trade_price = amount_out_f / amount_in_f;

    auto token_in = is_token0 ? p_itr->token0 : p_itr->token1;
    auto token_out = is_token0 ? p_itr->token1 : p_itr->token0;
    auto data = std::make_tuple(pair_id, from, token_in.get_contract(), quantity, token_out.get_contract(), output.quantity, asset(trade_fee + protocol_fee, quantity.symbol), trade_price, p_itr->total_liquidity, p_itr->reserve0, p_itr->reserve1);
    action(permission_level{_self, "active"_n}, LOG_CONTRACT, "swap"_n, data).send();

    if (get_config("orac.status") == 1) {
        auto push_data = std::make_tuple(pair_id, reserve0, reserve1, balance0, balance1);
        action(permission_level{_self, "active"_n}, get_account("orac.account"), "update"_n, push_data).send();
    }

    return output;
}

void swap::update_pair(uint64_t pair_id, uint64_t balance0, uint64_t balance1, uint64_t reserve0, uint64_t reserve1) {
    check(balance0 >= 0 && balance1 >= 0, "update usertokens error");
    auto p_itr = _pairs.require_find(pair_id, "pair does not exist");
    _pairs.modify(p_itr, same_payer, [&](auto &a) {
        a.reserve0.amount = balance0;
        a.reserve1.amount = balance1;
        a.updated_time = current_block_time();
    });
}

