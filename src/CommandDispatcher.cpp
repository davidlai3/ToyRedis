#include <cctype>
#include <functional>
#include <limits>
#include <string>
#include <unordered_map>

#include "../include/CommandDispatcher.h"

namespace {

    inline constexpr const char* invalid_msg = "INVALID Command format is invalid for this command";
    inline constexpr const char* wrongtype_msg = "WRONGTYPE Operation against a key holding the wrong kind of value";
    inline constexpr const char* dne_msg = "DOES NOT EXIST Requested element does not exist";
    inline constexpr const char* oor_msg = "OUT OF RANGE Operation pushes value past 64-bit integer bounds";
    inline constexpr const char* notinteger_msg = "NOT INTEGER Target element cannot be converted to an integer";

    // Helpers

    // Translate a database error into the matching RESP error reply.
    RespValue db_error_resp(Database::DBError err) {
        switch (err) {
            case Database::DBError::WRONGTYPE:  return RespValue{RespSimpleError{wrongtype_msg}};
            case Database::DBError::OUTOFRANGE: return RespValue{RespSimpleError{oor_msg}};
            case Database::DBError::NOTINTEGER: return RespValue{RespSimpleError{notinteger_msg}};
        }
        return RespValue{RespSimpleError{invalid_msg}}; // unreachable; silences -Wreturn-type
    }

    // Parse a signed integer argument. Returns false on malformed/overflow input.
    bool parse_ll(std::string_view sv, long long& out) {
        try {
            size_t consumed = 0;
            out = std::stoll(std::string{sv}, &consumed);
            return consumed == sv.size(); // reject trailing garbage like "12x"
        } catch (...) {
            return false;
        }
    }

    // Parse a floating-point argument (for sorted-set scores).
    bool parse_double(std::string_view sv, double& out) {
        try {
            size_t consumed = 0;
            out = std::stod(std::string{sv}, &consumed);
            return consumed == sv.size();
        } catch (...) {
            return false;
        }
    }

    // Format a score the way Redis does-ish: whole numbers print without a
    // decimal point, fractionals drop trailing zeros.
    std::string format_double(double d) {
        if (d == static_cast<long long>(d)) {
            return std::to_string(static_cast<long long>(d));
        }
        std::string s = std::to_string(d);
        s.erase(s.find_last_not_of('0') + 1, std::string::npos);
        if (!s.empty() && s.back() == '.') {
            s.pop_back();
        }
        return s;
    }

    // Health Commands
    RespValue echo(const std::vector<std::string_view>& args, Database& db) {
        if (args.size() != 2) {
            return RespValue{RespSimpleError{invalid_msg}};
        }
        // ECHO returns the message as a bulk string (it is arbitrary user data).
        return RespValue{RespBulkString{std::string{args[1]}}};
    }

    RespValue ping(const std::vector<std::string_view>& args, Database& db) {
        if (args.size() == 1) {
            return RespValue{RespSimpleString{"PONG"}};
        }
        if (args.size() == 2) {
            return RespValue{RespBulkString{std::string{args[1]}}};
        }
        return RespValue{RespSimpleError{invalid_msg}};
    }

    // Generic Commands (no WRONGTYPE possible)
    RespValue exists(const std::vector<std::string_view>& args, Database& db) {
        size_t n = args.size();
        if (n < 2) {
            return RespValue{RespSimpleError{invalid_msg}};
        }
        long long count = 0;
        for (size_t i = 1; i < n; i++) {
            count += std::get<bool>(db.exists(std::string{args[i]}));
        }
        return RespValue{RespInteger{count}};
    }

    RespValue dbsize(const std::vector<std::string_view>& args, Database& db) {
        if (args.size() != 1) {
            return RespValue{RespSimpleError{invalid_msg}};
        }
        return RespValue{RespInteger{static_cast<long long>(std::get<size_t>(db.size()))}};
    }

    RespValue del(const std::vector<std::string_view>& args, Database& db) {
        size_t n = args.size();
        if (n < 2) {
            return RespValue{RespSimpleError{invalid_msg}};
        }
        long long removed = 0;
        for (size_t i = 1; i < n; i++) {
            removed += std::get<bool>(db.del(std::string{args[i]}));
        }
        return RespValue{RespInteger{removed}};
    }

    RespValue keys(const std::vector<std::string_view>& args, Database& db) {
        if (args.size() != 2) {
            return RespValue{RespSimpleError{invalid_msg}};
        }
        // TODO: only the "*" pattern is meaningfully supported; we return all keys.
        auto all = std::get<std::vector<std::string>>(db.keys());
        std::vector<RespValue> ret;
        ret.reserve(all.size());
        for (std::string& key : all) {
            ret.push_back(RespValue{RespBulkString{std::move(key)}});
        }
        return RespValue{RespArray{std::move(ret)}};
    }

    RespValue type(const std::vector<std::string_view>& args, Database& db) {
        if (args.size() != 2) {
            return RespValue{RespSimpleError{invalid_msg}};
        }
        auto str_type = std::get<std::optional<std::string>>(db.type(std::string{args[1]}));
        if (!str_type) {
            return RespValue{RespSimpleString{"none"}};
        }
        return RespValue{RespSimpleString{str_type.value()}};
    }

    // String Commands
    RespValue append(const std::vector<std::string_view>& args, Database& db) {
        if (args.size() != 3) {
            return RespValue{RespSimpleError{invalid_msg}};
        }
        auto ret = db.append(std::string{args[1]}, std::string{args[2]});
        if (auto* err = std::get_if<Database::DBError>(&ret)) {
            return db_error_resp(*err);
        }
        return RespValue{RespInteger{static_cast<long long>(std::get<size_t>(ret))}};
    }

    RespValue incrby_impl(Database& db, std::string_view key, long long delta) {
        auto ret = db.incrby(std::string{key}, delta);
        if (auto* err = std::get_if<Database::DBError>(&ret)) {
            return db_error_resp(*err);
        }
        return RespValue{RespInteger{std::get<long long>(ret)}};
    }

    RespValue incr(const std::vector<std::string_view>& args, Database& db) {
        if (args.size() != 2) {
            return RespValue{RespSimpleError{invalid_msg}};
        }
        return incrby_impl(db, args[1], 1);
    }

    RespValue decr(const std::vector<std::string_view>& args, Database& db) {
        if (args.size() != 2) {
            return RespValue{RespSimpleError{invalid_msg}};
        }
        return incrby_impl(db, args[1], -1);
    }

    RespValue incrby(const std::vector<std::string_view>& args, Database& db) {
        if (args.size() != 3) {
            return RespValue{RespSimpleError{invalid_msg}};
        }
        long long delta;
        if (!parse_ll(args[2], delta)) {
            return RespValue{RespSimpleError{notinteger_msg}};
        }
        return incrby_impl(db, args[1], delta);
    }

    RespValue decrby(const std::vector<std::string_view>& args, Database& db) {
        if (args.size() != 3) {
            return RespValue{RespSimpleError{invalid_msg}};
        }
        long long delta;
        if (!parse_ll(args[2], delta)) {
            return RespValue{RespSimpleError{notinteger_msg}};
        }
        // DECRBY x == INCRBY -x. Guard against negating LLONG_MIN (UB).
        if (delta == std::numeric_limits<long long>::min()) {
            return RespValue{RespSimpleError{oor_msg}};
        }
        return incrby_impl(db, args[1], -delta);
    }

    RespValue get(const std::vector<std::string_view>& args, Database& db) {
        if (args.size() != 2) {
            return RespValue{RespSimpleError{invalid_msg}};
        }
        auto ret = db.get(std::string{args[1]});
        if (auto* err = std::get_if<Database::DBError>(&ret)) {
            return db_error_resp(*err);
        }
        auto opt = std::get<std::optional<std::string>>(ret);
        if (!opt) {
            return RespValue{RespNullBulkString{}}; // missing key -> nil
        }
        return RespValue{RespBulkString{opt.value()}};
    }

    RespValue getdel(const std::vector<std::string_view>& args, Database& db) {
        if (args.size() != 2) {
            return RespValue{RespSimpleError{invalid_msg}};
        }
        auto ret = db.get(std::string{args[1]});
        if (auto* err = std::get_if<Database::DBError>(&ret)) {
            return db_error_resp(*err);
        }
        auto opt = std::get<std::optional<std::string>>(ret);
        if (!opt) {
            return RespValue{RespNullBulkString{}};
        }
        db.del(std::string{args[1]});
        return RespValue{RespBulkString{opt.value()}};
    }

    RespValue set(const std::vector<std::string_view>& args, Database& db) {
        if (args.size() != 3) {
            return RespValue{RespSimpleError{invalid_msg}};
        }
        db.set(std::string{args[1]}, std::string{args[2]});
        return RespValue{RespSimpleString{"OK"}};
    }

    RespValue setnx(const std::vector<std::string_view>& args, Database& db) {
        if (args.size() != 3) {
            return RespValue{RespSimpleError{invalid_msg}};
        }
        auto ret = db.setnx(std::string{args[1]}, std::string{args[2]});
        if (auto* err = std::get_if<Database::DBError>(&ret)) {
            return db_error_resp(*err);
        }
        return RespValue{RespInteger{std::get<bool>(ret) ? 1 : 0}};
    }

    RespValue strlen(const std::vector<std::string_view>& args, Database& db) {
        if (args.size() != 2) {
            return RespValue{RespSimpleError{invalid_msg}};
        }
        auto ret = db.strlen(std::string{args[1]});
        if (auto* err = std::get_if<Database::DBError>(&ret)) {
            return db_error_resp(*err);
        }
        return RespValue{RespInteger{static_cast<long long>(std::get<size_t>(ret))}};
    }

    // List Commands
    RespValue lindex(const std::vector<std::string_view>& args, Database& db) {
        if (args.size() != 3) {
            return RespValue{RespSimpleError{invalid_msg}};
        }
        long long idx;
        if (!parse_ll(args[2], idx)) {
            return RespValue{RespSimpleError{notinteger_msg}};
        }
        auto ret = db.lindex(std::string{args[1]}, idx);
        if (auto* err = std::get_if<Database::DBError>(&ret)) {
            return db_error_resp(*err);
        }
        auto opt = std::get<std::optional<std::string>>(ret);
        if (!opt) {
            return RespValue{RespNullBulkString{}};
        }
        return RespValue{RespBulkString{opt.value()}};
    }

    RespValue llen(const std::vector<std::string_view>& args, Database& db) {
        if (args.size() != 2) {
            return RespValue{RespSimpleError{invalid_msg}};
        }
        auto ret = db.llen(std::string{args[1]});
        if (auto* err = std::get_if<Database::DBError>(&ret)) {
            return db_error_resp(*err);
        }
        return RespValue{RespInteger{static_cast<long long>(std::get<size_t>(ret))}};
    }

    RespValue lpop(const std::vector<std::string_view>& args, Database& db) {
        if (args.size() != 2) {
            return RespValue{RespSimpleError{invalid_msg}};
        }
        auto ret = db.lpop(std::string{args[1]});
        if (auto* err = std::get_if<Database::DBError>(&ret)) {
            return db_error_resp(*err);
        }
        auto opt = std::get<std::optional<std::string>>(ret);
        if (!opt) {
            return RespValue{RespNullBulkString{}};
        }
        return RespValue{RespBulkString{opt.value()}};
    }

    RespValue rpop(const std::vector<std::string_view>& args, Database& db) {
        if (args.size() != 2) {
            return RespValue{RespSimpleError{invalid_msg}};
        }
        auto ret = db.rpop(std::string{args[1]});
        if (auto* err = std::get_if<Database::DBError>(&ret)) {
            return db_error_resp(*err);
        }
        auto opt = std::get<std::optional<std::string>>(ret);
        if (!opt) {
            return RespValue{RespNullBulkString{}};
        }
        return RespValue{RespBulkString{opt.value()}};
    }

    RespValue lpush(const std::vector<std::string_view>& args, Database& db) {
        size_t n = args.size();
        if (n < 3) {
            return RespValue{RespSimpleError{invalid_msg}};
        }
        std::vector<std::string> vals;
        vals.reserve(n - 2);
        for (size_t i = 2; i < n; i++) {
            vals.emplace_back(args[i]);
        }
        auto ret = db.lpush(std::string{args[1]}, std::move(vals));
        if (auto* err = std::get_if<Database::DBError>(&ret)) {
            return db_error_resp(*err);
        }
        return RespValue{RespInteger{static_cast<long long>(std::get<size_t>(ret))}};
    }

    RespValue rpush(const std::vector<std::string_view>& args, Database& db) {
        size_t n = args.size();
        if (n < 3) {
            return RespValue{RespSimpleError{invalid_msg}};
        }
        std::vector<std::string> vals;
        vals.reserve(n - 2);
        for (size_t i = 2; i < n; i++) {
            vals.emplace_back(args[i]);
        }
        auto ret = db.rpush(std::string{args[1]}, std::move(vals));
        if (auto* err = std::get_if<Database::DBError>(&ret)) {
            return db_error_resp(*err);
        }
        return RespValue{RespInteger{static_cast<long long>(std::get<size_t>(ret))}};
    }

    RespValue lrange(const std::vector<std::string_view>& args, Database& db) {
        if (args.size() != 4) {
            return RespValue{RespSimpleError{invalid_msg}};
        }
        long long start, stop;
        if (!parse_ll(args[2], start) || !parse_ll(args[3], stop)) {
            return RespValue{RespSimpleError{notinteger_msg}};
        }
        auto ret = db.lrange(std::string{args[1]}, start, stop);
        if (auto* err = std::get_if<Database::DBError>(&ret)) {
            return db_error_resp(*err);
        }
        auto items = std::get<std::vector<std::string>>(ret);
        std::vector<RespValue> arr;
        arr.reserve(items.size());
        for (std::string& s : items) {
            arr.push_back(RespValue{RespBulkString{std::move(s)}});
        }
        return RespValue{RespArray{std::move(arr)}};
    }

    // Hash Commands
    RespValue hset(const std::vector<std::string_view>& args, Database& db) {
        size_t n = args.size();
        // key + at least one field/value pair, and pairs must be complete
        if (n < 4 || (n - 2) % 2 != 0) {
            return RespValue{RespSimpleError{invalid_msg}};
        }
        std::vector<std::pair<std::string, std::string>> field_vals;
        field_vals.reserve((n - 2) / 2);
        for (size_t i = 2; i + 1 < n; i += 2) {
            field_vals.emplace_back(std::string{args[i]}, std::string{args[i + 1]});
        }
        auto ret = db.hset(std::string{args[1]}, std::move(field_vals));
        if (auto* err = std::get_if<Database::DBError>(&ret)) {
            return db_error_resp(*err);
        }
        return RespValue{RespInteger{static_cast<long long>(std::get<size_t>(ret))}};
    }

    RespValue hget(const std::vector<std::string_view>& args, Database& db) {
        if (args.size() != 3) {
            return RespValue{RespSimpleError{invalid_msg}};
        }
        auto ret = db.hget(std::string{args[1]}, std::string{args[2]});
        if (auto* err = std::get_if<Database::DBError>(&ret)) {
            return db_error_resp(*err);
        }
        auto opt = std::get<std::optional<std::string>>(ret);
        if (!opt) {
            return RespValue{RespNullBulkString{}};
        }
        return RespValue{RespBulkString{opt.value()}};
    }

    RespValue hdel(const std::vector<std::string_view>& args, Database& db) {
        size_t n = args.size();
        if (n < 3) {
            return RespValue{RespSimpleError{invalid_msg}};
        }
        std::vector<std::string> fields;
        fields.reserve(n - 2);
        for (size_t i = 2; i < n; i++) {
            fields.emplace_back(args[i]);
        }
        auto ret = db.hdel(std::string{args[1]}, std::move(fields));
        if (auto* err = std::get_if<Database::DBError>(&ret)) {
            return db_error_resp(*err);
        }
        return RespValue{RespInteger{static_cast<long long>(std::get<size_t>(ret))}};
    }

    RespValue hexists(const std::vector<std::string_view>& args, Database& db) {
        if (args.size() != 3) {
            return RespValue{RespSimpleError{invalid_msg}};
        }
        auto ret = db.hexists(std::string{args[1]}, std::string{args[2]});
        if (auto* err = std::get_if<Database::DBError>(&ret)) {
            return db_error_resp(*err);
        }
        return RespValue{RespInteger{std::get<bool>(ret) ? 1 : 0}};
    }

    RespValue hlen(const std::vector<std::string_view>& args, Database& db) {
        if (args.size() != 2) {
            return RespValue{RespSimpleError{invalid_msg}};
        }
        auto ret = db.hlen(std::string{args[1]});
        if (auto* err = std::get_if<Database::DBError>(&ret)) {
            return db_error_resp(*err);
        }
        return RespValue{RespInteger{static_cast<long long>(std::get<size_t>(ret))}};
    }

    RespValue hgetall(const std::vector<std::string_view>& args, Database& db) {
        if (args.size() != 2) {
            return RespValue{RespSimpleError{invalid_msg}};
        }
        auto ret = db.hgetall(std::string{args[1]});
        if (auto* err = std::get_if<Database::DBError>(&ret)) {
            return db_error_resp(*err);
        }
        auto pairs = std::get<std::vector<std::pair<std::string, std::string>>>(ret);
        std::vector<RespValue> arr;
        arr.reserve(pairs.size() * 2);
        for (auto& [field, val] : pairs) {
            arr.push_back(RespValue{RespBulkString{std::move(field)}});
            arr.push_back(RespValue{RespBulkString{std::move(val)}});
        }
        return RespValue{RespArray{std::move(arr)}};
    }

    RespValue hkeys(const std::vector<std::string_view>& args, Database& db) {
        if (args.size() != 2) {
            return RespValue{RespSimpleError{invalid_msg}};
        }
        auto ret = db.hkeys(std::string{args[1]});
        if (auto* err = std::get_if<Database::DBError>(&ret)) {
            return db_error_resp(*err);
        }
        auto items = std::get<std::vector<std::string>>(ret);
        std::vector<RespValue> arr;
        arr.reserve(items.size());
        for (std::string& s : items) {
            arr.push_back(RespValue{RespBulkString{std::move(s)}});
        }
        return RespValue{RespArray{std::move(arr)}};
    }

    RespValue hvals(const std::vector<std::string_view>& args, Database& db) {
        if (args.size() != 2) {
            return RespValue{RespSimpleError{invalid_msg}};
        }
        auto ret = db.hvals(std::string{args[1]});
        if (auto* err = std::get_if<Database::DBError>(&ret)) {
            return db_error_resp(*err);
        }
        auto items = std::get<std::vector<std::string>>(ret);
        std::vector<RespValue> arr;
        arr.reserve(items.size());
        for (std::string& s : items) {
            arr.push_back(RespValue{RespBulkString{std::move(s)}});
        }
        return RespValue{RespArray{std::move(arr)}};
    }

    // ---- Set Commands ------------------------------------------------------
    RespValue sadd(const std::vector<std::string_view>& args, Database& db) {
        size_t n = args.size();
        if (n < 3) {
            return RespValue{RespSimpleError{invalid_msg}};
        }
        std::vector<std::string> members;
        members.reserve(n - 2);
        for (size_t i = 2; i < n; i++) {
            members.emplace_back(args[i]);
        }
        auto ret = db.sadd(std::string{args[1]}, std::move(members));
        if (auto* err = std::get_if<Database::DBError>(&ret)) {
            return db_error_resp(*err);
        }
        return RespValue{RespInteger{static_cast<long long>(std::get<size_t>(ret))}};
    }

    RespValue srem(const std::vector<std::string_view>& args, Database& db) {
        size_t n = args.size();
        if (n < 3) {
            return RespValue{RespSimpleError{invalid_msg}};
        }
        std::vector<std::string> members;
        members.reserve(n - 2);
        for (size_t i = 2; i < n; i++) {
            members.emplace_back(args[i]);
        }
        auto ret = db.srem(std::string{args[1]}, std::move(members));
        if (auto* err = std::get_if<Database::DBError>(&ret)) {
            return db_error_resp(*err);
        }
        return RespValue{RespInteger{static_cast<long long>(std::get<size_t>(ret))}};
    }

    RespValue sismember(const std::vector<std::string_view>& args, Database& db) {
        if (args.size() != 3) {
            return RespValue{RespSimpleError{invalid_msg}};
        }
        auto ret = db.sismember(std::string{args[1]}, std::string{args[2]});
        if (auto* err = std::get_if<Database::DBError>(&ret)) {
            return db_error_resp(*err);
        }
        return RespValue{RespInteger{std::get<bool>(ret) ? 1 : 0}};
    }

    RespValue scard(const std::vector<std::string_view>& args, Database& db) {
        if (args.size() != 2) {
            return RespValue{RespSimpleError{invalid_msg}};
        }
        auto ret = db.scard(std::string{args[1]});
        if (auto* err = std::get_if<Database::DBError>(&ret)) {
            return db_error_resp(*err);
        }
        return RespValue{RespInteger{static_cast<long long>(std::get<size_t>(ret))}};
    }

    RespValue smembers(const std::vector<std::string_view>& args, Database& db) {
        if (args.size() != 2) {
            return RespValue{RespSimpleError{invalid_msg}};
        }
        auto ret = db.smembers(std::string{args[1]});
        if (auto* err = std::get_if<Database::DBError>(&ret)) {
            return db_error_resp(*err);
        }
        auto items = std::get<std::vector<std::string>>(ret);
        std::vector<RespValue> arr;
        arr.reserve(items.size());
        for (std::string& s : items) {
            arr.push_back(RespValue{RespBulkString{std::move(s)}});
        }
        return RespValue{RespArray{std::move(arr)}};
    }

    // Sorted Set Commands
    RespValue zadd(const std::vector<std::string_view>& args, Database& db) {
        if (args.size() != 4) {
            return RespValue{RespSimpleError{invalid_msg}};
        }
        double score;
        if (!parse_double(args[2], score)) {
            return RespValue{RespSimpleError{notinteger_msg}};
        }
        auto ret = db.zadd(std::string{args[1]}, score, std::string{args[3]});
        if (auto* err = std::get_if<Database::DBError>(&ret)) {
            return db_error_resp(*err);
        }
        return RespValue{RespInteger{static_cast<long long>(std::get<size_t>(ret))}};
    }

    RespValue zcard(const std::vector<std::string_view>& args, Database& db) {
        if (args.size() != 2) {
            return RespValue{RespSimpleError{invalid_msg}};
        }
        auto ret = db.zcard(std::string{args[1]});
        if (auto* err = std::get_if<Database::DBError>(&ret)) {
            return db_error_resp(*err);
        }
        return RespValue{RespInteger{static_cast<long long>(std::get<size_t>(ret))}};
    }

    RespValue zscore(const std::vector<std::string_view>& args, Database& db) {
        if (args.size() != 3) {
            return RespValue{RespSimpleError{invalid_msg}};
        }
        auto ret = db.zscore(std::string{args[1]}, std::string{args[2]});
        if (auto* err = std::get_if<Database::DBError>(&ret)) {
            return db_error_resp(*err);
        }
        auto opt = std::get<std::optional<double>>(ret);
        if (!opt) {
            return RespValue{RespNullBulkString{}};
        }
        return RespValue{RespBulkString{format_double(opt.value())}};
    }

    RespValue zrem(const std::vector<std::string_view>& args, Database& db) {
        size_t n = args.size();
        if (n < 3) {
            return RespValue{RespSimpleError{invalid_msg}};
        }
        std::vector<std::string> members;
        members.reserve(n - 2);
        for (size_t i = 2; i < n; i++) {
            members.emplace_back(args[i]);
        }
        auto ret = db.zrem(std::string{args[1]}, std::move(members));
        if (auto* err = std::get_if<Database::DBError>(&ret)) {
            return db_error_resp(*err);
        }
        return RespValue{RespInteger{static_cast<long long>(std::get<size_t>(ret))}};
    }

    RespValue zrange(const std::vector<std::string_view>& args, Database& db) {
        size_t n = args.size();
        bool withscores = false;
        if (n == 5) {
            std::string opt;
            for (char c : args[4]) {
                opt.push_back(static_cast<char>(std::toupper(static_cast<unsigned char>(c))));
            }
            if (opt != "WITHSCORES") {
                return RespValue{RespSimpleError{invalid_msg}};
            }
            withscores = true;
        } else if (n != 4) {
            return RespValue{RespSimpleError{invalid_msg}};
        }

        long long start, stop;
        if (!parse_ll(args[2], start) || !parse_ll(args[3], stop)) {
            return RespValue{RespSimpleError{notinteger_msg}};
        }
        auto ret = db.zrange(std::string{args[1]}, start, stop);
        if (auto* err = std::get_if<Database::DBError>(&ret)) {
            return db_error_resp(*err);
        }
        auto pairs = std::get<std::vector<std::pair<std::string, double>>>(ret);
        std::vector<RespValue> arr;
        arr.reserve(withscores ? pairs.size() * 2 : pairs.size());
        for (auto& [member, score] : pairs) {
            arr.push_back(RespValue{RespBulkString{std::move(member)}});
            if (withscores) {
                arr.push_back(RespValue{RespBulkString{format_double(score)}});
            }
        }
        return RespValue{RespArray{std::move(arr)}};
    }

    using Handler = std::function<RespValue(
        const std::vector<std::string_view>&, Database&)
    >;

    static std::unordered_map<std::string, Handler> handler_map =
    {
        {"ECHO", echo}, {"PING", ping}, {"EXISTS", exists}, {"DBSIZE", dbsize},
        {"DEL", del}, {"KEYS", keys}, {"TYPE", type}, {"APPEND", append},
        {"DECR", decr}, {"DECRBY", decrby}, {"INCR", incr}, {"INCRBY", incrby},
        {"GET", get}, {"GETDEL", getdel}, {"SET", set}, {"SETNX", setnx},
        {"STRLEN", strlen}, {"LINDEX", lindex}, {"LLEN", llen}, {"LPOP", lpop},
        {"LPUSH", lpush}, {"LRANGE", lrange}, {"RPOP", rpop}, {"RPUSH", rpush},
        {"HDEL", hdel}, {"HEXISTS", hexists}, {"HGET", hget},
        {"HGETALL", hgetall}, {"HKEYS", hkeys}, {"HLEN", hlen}, {"HSET", hset},
        {"HVALS", hvals}, {"SADD", sadd}, {"SCARD", scard},
        {"SISMEMBER", sismember}, {"SMEMBERS", smembers}, {"SREM", srem},
        {"ZADD", zadd}, {"ZCARD", zcard}, {"ZRANGE", zrange}, {"ZREM", zrem},
        {"ZSCORE", zscore},
    };
}

RespValue dispatch(const RespValue& command, Database& db) {
    // Clients always send commands as an array of bulk strings.
    if (!std::holds_alternative<RespArray>(command.value)) {
        return RespValue{RespSimpleError{invalid_msg}};
    }
    const std::vector<RespValue>& elems = std::get<RespArray>(command.value).vals;
    if (elems.empty()) {
        return RespValue{RespSimpleError{invalid_msg}};
    }

    std::vector<std::string_view> args;
    args.reserve(elems.size());
    for (const RespValue& elem : elems) {
        if (!std::holds_alternative<RespBulkString>(elem.value)) {
            return RespValue{RespSimpleError{invalid_msg}};
        }
        args.push_back(std::get<RespBulkString>(elem.value).msg);
    }

    // Command names are case-insensitive; the map is keyed by uppercase.
    std::string verb;
    verb.reserve(args[0].size());
    for (char c : args[0]) {
        verb.push_back(static_cast<char>(std::toupper(static_cast<unsigned char>(c))));
    }

    auto it = handler_map.find(verb);
    if (it == handler_map.end()) {
        return RespValue{RespSimpleError{"ERR unknown command '" + verb + "'"}};
    }
    return it->second(args, db);
}
