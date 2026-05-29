#include "../include/Database.h"
#include <limits>
#include <stdexcept>

// Generic
Database::DBResult<bool> Database::del(std::string key) {
    auto it = db.find(key);
    if (it == db.end()) {
        return false;
    }

    db.erase(it);
    return true;
}

Database::DBResult<bool> Database::exists(std::string key) {
    return db.find(key) != db.end();
}

Database::DBResult<std::optional<std::string>> Database::type(std::string key) {
    auto it = db.find(key);
    if (it == db.end()) {
        return std::nullopt;
    }

    return std::string{value_names[it->second.index()]};
}

Database::DBResult<size_t> Database::size() {
    return db.size();
}

Database::DBResult<std::vector<std::string>> Database::keys() {
    std::vector<std::string> keys;
    keys.reserve(db.size());

    for (const auto& [key, val] : db) {
        keys.push_back(key);
    }

    return keys;
}

// Strings
Database::DBResult<std::optional<std::string>> Database::get(std::string key) {
    auto it = db.find(key);
    if (it == db.end()) {
        return std::nullopt;
    }
    
    if (!std::holds_alternative<std::string>(it->second)) {
        return DBError::WRONGTYPE;
    }

    return std::get<std::string>(it->second);
}

// String set is one of the only commands that disregards underlying type
void Database::set(std::string key, Value val) {
    db.insert_or_assign(key, val);
}

Database::DBResult<bool> Database::setnx(std::string key, Value val) {
    auto it = db.find(key);
    if (it != db.end()) {
        return false;
    }

    db.emplace(key, val);
    return true;
}

Database::DBResult<size_t> Database::append(std::string key, std::string suffix) {
    auto it = db.find(key);
    if (it == db.end()) {
        db.emplace(key, suffix);
        return suffix.size();
    }

    if (!std::holds_alternative<std::string>(it->second)) {
        return DBError::WRONGTYPE;
    }

    std::string& val = std::get<std::string>(it->second);
    val += suffix;
    return val.size();
}

Database::DBResult<size_t> Database::strlen(std::string key) {
    auto it = db.find(key);
    if (it == db.end()) {
        return size_t{0};
    }
    
    if (!std::holds_alternative<std::string>(it->second)) {
        return DBError::WRONGTYPE;
    }

    return std::get<std::string>(it->second).size();
}

Database::DBResult<long long> Database::incrby(std::string key,
        long long delta) {
    auto it = db.find(key);
    if (it == db.end()) {
        db.emplace(key, std::to_string(delta));
        return delta;
    }

    if (!std::holds_alternative<std::string>(it->second)) {
        return DBError::WRONGTYPE;
    }

    try {
        long long val = std::stoll(std::get<std::string>(it->second));

        long long max = std::numeric_limits<long long>::max();
        long long min = std::numeric_limits<long long>::min();
        if ((delta > 0 && val > max - delta) || 
                (delta < 0 && val < min - delta)) {
            return DBError::OUTOFRANGE;
        }

        val += delta;
        it->second = std::to_string(val);
        return val;
    }
    catch (const std::invalid_argument& e) {
        return DBError::NOTINTEGER;
    }
    catch (const std::out_of_range& e) {
        return DBError::OUTOFRANGE;
    }
}

// Lists
Database::DBResult<size_t> Database::lpush(std::string key,
        std::vector<std::string> vals) {
    auto it = db.find(key);
    if (it == db.end()) {
        it = db.emplace(key, ListValue{}).first;
    }

    if (!std::holds_alternative<ListValue>(it->second)) {
        return DBError::WRONGTYPE;
    }

    std::deque<std::string>& list = std::get<ListValue>(it->second).elements;

    for (std::string val : vals) {
        list.push_front(val);
    }

    return list.size();
}

Database::DBResult<size_t> Database::rpush(std::string key,
        std::vector<std::string> vals) {
    auto it = db.find(key);
    if (it == db.end()) {
        it = db.emplace(key, ListValue{}).first;
    }

    if (!std::holds_alternative<ListValue>(it->second)) {
        return DBError::WRONGTYPE;
    }

    std::deque<std::string>& list = std::get<ListValue>(it->second).elements;

    for (std::string val : vals) {
        list.push_back(val);
    }

    return list.size();
}

Database::DBResult<std::optional<std::string>> Database::lpop(std::string key) {
    auto it = db.find(key);
    if (it == db.end()) {
        return std::nullopt;
    }

    if (!std::holds_alternative<ListValue>(it->second)) {
        return DBError::WRONGTYPE;
    }

    std::deque<std::string>& list = std::get<ListValue>(it->second).elements;

    Value ret = list.front();
    list.pop_front();

    if (list.empty()) {
        db.erase(it);
    }

    return std::get<std::string>(ret);
}

Database::DBResult<std::optional<std::string>> Database::rpop(std::string key) {
    auto it = db.find(key);
    if (it == db.end()) {
        return std::nullopt;
    }

    if (!std::holds_alternative<ListValue>(it->second)) {
        return DBError::WRONGTYPE;
    }

    std::deque<std::string>& list = std::get<ListValue>(it->second).elements;

    Value ret = list.back();
    list.pop_back();

    if (list.empty()) {
        db.erase(it);
    }

    return std::get<std::string>(ret);
}

Database::DBResult<std::vector<std::string>> Database::lrange(std::string key,
        long long start, long long stop) {

    std::vector<std::string> ret;

    auto it = db.find(key);
    if (it == db.end()) {
        return ret;
    }
    if (!std::holds_alternative<ListValue>(it->second)) {
        return DBError::WRONGTYPE;
    }

    std::deque<std::string>& list = std::get<ListValue>(it->second).elements;
    long long n = list.size();

    if (start >= n) {
        return ret;
    }
    if (stop < 0) {
        stop = n+stop;
    }
    else {
        stop = std::min(n-1, stop);
    }
    if (start < 0) {
        start = std::max(0LL, n+start);
    }


    for (long long i = start; i <= stop; i++) {
        ret.push_back(list[i]);
    }

    return ret;
}

Database::DBResult<size_t> Database::llen(std::string key) {
    auto it = db.find(key);
    if (it == db.end()) {
        return size_t{0};
    }
    if (!std::holds_alternative<ListValue>(it->second)) {
        return DBError::WRONGTYPE;
    }

    std::deque<std::string>& list = std::get<ListValue>(it->second).elements;
    return list.size();
}

Database::DBResult<std::optional<std::string>> Database::lindex(std::string key,
        long long idx) {
    auto it = db.find(key);
    if (it == db.end()) {
        return std::nullopt;
    }
    if (!std::holds_alternative<ListValue>(it->second)) {
        return DBError::WRONGTYPE;
    }

    std::deque<std::string>& list = std::get<ListValue>(it->second).elements;
    long long n = list.size();

    if (idx < 0) {
        idx = n+idx;
    }
    if (idx >= n || idx < 0) {
        return std::nullopt;
    }

    return list[idx];
}

// Hashes
Database::DBResult<size_t> Database::hset(std::string key,
        std::vector<std::pair<std::string, std::string>> field_vals) {
    auto it = db.find(key);
    if (it == db.end()) {
        it = db.emplace(key, HashValue{}).first;
    }
    if (!std::holds_alternative<HashValue>(it->second)) {
        return DBError::WRONGTYPE;
    }

    auto& fields = std::get<HashValue>(it->second).fields;
    size_t added = 0;
    for (auto& [field, val] : field_vals) {
        // insert_or_assign's bool is true only when a new key was inserted
        if (fields.insert_or_assign(field, val).second) {
            added++;
        }
    }
    return added;
}

Database::DBResult<std::optional<std::string>> Database::hget(std::string key,
        std::string field) {
    auto it = db.find(key);
    if (it == db.end()) {
        return std::nullopt;
    }
    if (!std::holds_alternative<HashValue>(it->second)) {
        return DBError::WRONGTYPE;
    }

    auto& fields = std::get<HashValue>(it->second).fields;
    auto fit = fields.find(field);
    if (fit == fields.end()) {
        return std::nullopt;
    }
    return fit->second;
}

Database::DBResult<size_t> Database::hdel(std::string key,
        std::vector<std::string> fields) {
    auto it = db.find(key);
    if (it == db.end()) {
        return size_t{0};
    }
    if (!std::holds_alternative<HashValue>(it->second)) {
        return DBError::WRONGTYPE;
    }

    auto& hash = std::get<HashValue>(it->second).fields;
    size_t removed = 0;
    for (const auto& field : fields) {
        removed += hash.erase(field); // erase returns count removed (0 or 1)
    }
    if (hash.empty()) {
        db.erase(it); // last field gone -> key disappears
    }
    return removed;
}

Database::DBResult<bool> Database::hexists(std::string key, std::string field) {
    auto it = db.find(key);
    if (it == db.end()) {
        return false;
    }
    if (!std::holds_alternative<HashValue>(it->second)) {
        return DBError::WRONGTYPE;
    }

    auto& fields = std::get<HashValue>(it->second).fields;
    return fields.find(field) != fields.end();
}

Database::DBResult<std::vector<std::pair<std::string, std::string>>>
        Database::hgetall(std::string key) {
    std::vector<std::pair<std::string, std::string>> ret;
    auto it = db.find(key);
    if (it == db.end()) {
        return ret;
    }
    if (!std::holds_alternative<HashValue>(it->second)) {
        return DBError::WRONGTYPE;
    }

    auto& fields = std::get<HashValue>(it->second).fields;
    ret.reserve(fields.size());
    for (const auto& [field, val] : fields) {
        ret.emplace_back(field, val);
    }
    return ret;
}

Database::DBResult<size_t> Database::hlen(std::string key) {
    auto it = db.find(key);
    if (it == db.end()) {
        return size_t{0};
    }
    if (!std::holds_alternative<HashValue>(it->second)) {
        return DBError::WRONGTYPE;
    }
    return std::get<HashValue>(it->second).fields.size();
}

Database::DBResult<std::vector<std::string>> Database::hkeys(std::string key) {
    std::vector<std::string> ret;
    auto it = db.find(key);
    if (it == db.end()) {
        return ret;
    }
    if (!std::holds_alternative<HashValue>(it->second)) {
        return DBError::WRONGTYPE;
    }

    auto& fields = std::get<HashValue>(it->second).fields;
    ret.reserve(fields.size());
    for (const auto& [field, val] : fields) {
        ret.push_back(field);
    }
    return ret;
}

Database::DBResult<std::vector<std::string>> Database::hvals(std::string key) {
    std::vector<std::string> ret;
    auto it = db.find(key);
    if (it == db.end()) {
        return ret;
    }
    if (!std::holds_alternative<HashValue>(it->second)) {
        return DBError::WRONGTYPE;
    }

    auto& fields = std::get<HashValue>(it->second).fields;
    ret.reserve(fields.size());
    for (const auto& [field, val] : fields) {
        ret.push_back(val);
    }
    return ret;
}

// Sets
Database::DBResult<size_t> Database::sadd(std::string key,
        std::vector<std::string> members) {
    auto it = db.find(key);
    if (it == db.end()) {
        it = db.emplace(key, SetValue{}).first;
    }
    if (!std::holds_alternative<SetValue>(it->second)) {
        return DBError::WRONGTYPE;
    }

    auto& set = std::get<SetValue>(it->second).members;
    size_t added = 0;
    for (const auto& member : members) {
        // insert's bool is true only when the element was newly added
        if (set.insert(member).second) {
            added++;
        }
    }

    return added;
}

Database::DBResult<size_t> Database::srem(std::string key, 
        std::vector<std::string> members) {
    auto it = db.find(key);
    if (it == db.end()) {
        return size_t{0};
    }
    if (!std::holds_alternative<SetValue>(it->second)) {
        return DBError::WRONGTYPE;
    }

    auto& set = std::get<SetValue>(it->second).members;
    size_t removed = 0;
    for (const auto& member : members) {
        removed += set.erase(member);
    }

    if (set.empty()) {
        db.erase(it);
    }

    return removed;
}

Database::DBResult<std::vector<std::string>> Database::smembers(std::string key) {
    std::vector<std::string> ret;
    auto it = db.find(key);
    if (it == db.end()) {
        return ret;
    }
    if (!std::holds_alternative<SetValue>(it->second)) {
        return DBError::WRONGTYPE;
    }

    auto& set = std::get<SetValue>(it->second).members;
    ret.reserve(set.size());
    for (const auto& member : set) {
        ret.push_back(member);
    }
    return ret;
}

Database::DBResult<bool> Database::sismember(std::string key, std::string member) {
    auto it = db.find(key);
    if (it == db.end()) {
        return false;
    }
    if (!std::holds_alternative<SetValue>(it->second)) {
        return DBError::WRONGTYPE;
    }

    auto& set = std::get<SetValue>(it->second).members;
    return set.find(member) != set.end();
}

Database::DBResult<size_t> Database::scard(std::string key) {
    auto it = db.find(key);
    if (it == db.end()) {
        return size_t{0};
    }
    if (!std::holds_alternative<SetValue>(it->second)) {
        return DBError::WRONGTYPE;
    }
    return std::get<SetValue>(it->second).members.size();
}

// Sorted Sets
Database::DBResult<size_t> Database::zadd(std::string key, double score, 
        std::string member) {
    auto it = db.find(key);
    if (it == db.end()) {
        it = db.emplace(key, SortedSetValue{}).first;
    }

    if (!std::holds_alternative<SortedSetValue>(it->second)) {
        return DBError::WRONGTYPE;
    }

    SortedSetValue& sset = std::get<SortedSetValue>(it->second);

    auto member_it = sset.members.find(member);

    size_t added = 0;
    if (member_it != sset.members.end()) {
        sset.ranks.erase({member_it->second, member_it->first});
    }
    else {
        member_it = sset.members.emplace(member, score).first;
        added = 1;
    }

    member_it->second = score;
    sset.ranks.insert({score, member});

    return added;
}

Database::DBResult<std::vector<std::pair<std::string, double>>> Database::zrange(
        std::string key, long long start, long long stop) {
    std::vector<std::pair<std::string, double>> ret;
    auto it = db.find(key);
    if (it == db.end()) {
        return ret;
    }
    if (!std::holds_alternative<SortedSetValue>(it->second)) {
        return DBError::WRONGTYPE;
    }
    SortedSetValue& sset = std::get<SortedSetValue>(it->second);
    long long n = sset.members.size();

    if (start >= n) {
        return ret;
    }
    if (stop < 0) {
        stop = n+stop;
    }
    else {
        stop = std::min(n-1, stop);
    }
    if (start < 0) {
        start = std::max(0LL, n+start);
    }
    if (start > stop) {
        return ret;
    }

    ret.reserve(stop-start+1);
    auto it_lo = sset.ranks.find_by_order(start);
    auto it_hi = std::next(sset.ranks.find_by_order(stop));

    for (; it_lo != it_hi; it_lo++) {
        ret.push_back({it_lo->second, it_lo->first});
    }

    return ret;
}

Database::DBResult<std::optional<double>> Database::zscore(std::string key, 
        std::string member) {
    auto it = db.find(key);
    if (it == db.end()) {
        return std::nullopt;
    }
    if (!std::holds_alternative<SortedSetValue>(it->second)) {
        return DBError::WRONGTYPE;
    }
    SortedSetValue& sset = std::get<SortedSetValue>(it->second);
    auto member_it = sset.members.find(member);
    if (member_it == sset.members.end()) {
        return std::nullopt;
    }

    return member_it->second;
}

Database::DBResult<size_t> Database::zrem(std::string key, 
        std::vector<std::string> members) {
    auto it = db.find(key);
    if (it == db.end()) {
        return size_t{0};
    }
    if (!std::holds_alternative<SortedSetValue>(it->second)) {
        return DBError::WRONGTYPE;
    }

    size_t removed = 0;
    SortedSetValue& sset = std::get<SortedSetValue>(it->second);

    for (const std::string& member : members) {
        auto member_it = sset.members.find(member);
        if (member_it == sset.members.end()) {
            continue;
        }

        sset.ranks.erase({member_it->second, member_it->first});
        sset.members.erase(member_it);
        removed++;
    }

    if (sset.members.empty()) {
        db.erase(it);
    }

    return removed;
}

Database::DBResult<size_t> Database::zcard(std::string key) {
    auto it = db.find(key);
    if (it == db.end()) {
        return size_t{0};
    }
    if (!std::holds_alternative<SortedSetValue>(it->second)) {
        return DBError::WRONGTYPE;
    }

    return std::get<SortedSetValue>(it->second).members.size();
}
