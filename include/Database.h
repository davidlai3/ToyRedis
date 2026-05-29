#ifndef DATABASE_H
#define DATABASE_H

#include <ext/pb_ds/assoc_container.hpp>
#include <ext/pb_ds/tree_policy.hpp>
#include <deque>
#include <map>
#include <optional>
#include <unordered_set>
#include <unordered_map>
#include <utility>
#include <variant>
#include <vector>
#include <set>
#include <string>

using namespace __gnu_pbds;

template <typename T>
using ordered_set = tree<T, null_type, std::less<T>, rb_tree_tag, tree_order_statistics_node_update>;

#include "RespValue.hpp"

struct ListValue { std::deque<std::string> elements; };
struct HashValue { std::unordered_map<std::string, std::string> fields; };
struct SetValue { std::unordered_set<std::string> members; };
struct SortedSetValue {
    std::map<std::string, double> members; // member -> score
    ordered_set<std::pair<double, std::string>> ranks; // (score, member)
};

using Value = std::variant<
    std::string,
    ListValue,
    HashValue,
    SetValue,
    SortedSetValue
>;

inline constexpr std::string_view value_names[] = {
    "String", "List", "Hash", "Set", "Sorted Set"
}; 

class Database {
public:

    enum class DBError {
        WRONGTYPE,
        NOTINTEGER,
        OUTOFRANGE
    };

    template <typename T>
    using DBResult = std::variant<T, DBError>;

    // Generic
    DBResult<bool> del(std::string key);
    DBResult<bool> exists(std::string key);
    DBResult<std::optional<std::string>> type(std::string key);
    DBResult<size_t> size();
    DBResult<std::vector<std::string>> keys();

    // Strings
    DBResult<std::optional<std::string>> get(std::string key);
    void set(std::string key, Value val);
    DBResult<bool> setnx(std::string key, Value val);
    DBResult<size_t> append(std::string key, std::string suffix);
    DBResult<size_t> strlen(std::string key);
    DBResult<long long> incrby(std::string key, long long delta);

    // Lists
    DBResult<size_t> lpush(std::string key, std::vector<std::string> vals);
    DBResult<size_t> rpush(std::string key, std::vector<std::string> vals);
    DBResult<std::optional<std::string>> lpop(std::string key);
    DBResult<std::optional<std::string>> rpop(std::string key);
    DBResult<std::vector<std::string>> lrange(std::string key, 
            long long start, long long stop);
    DBResult<size_t> llen(std::string key);
    DBResult<std::optional<std::string>> lindex(std::string key, long long idx);

    // Hashes
    DBResult<size_t> hset(std::string key, 
            std::vector<std::pair<std::string, std::string>> field_vals);
    DBResult<std::optional<std::string>> hget(std::string key, 
            std::string field);
    DBResult<size_t> hdel(std::string key,
            std::vector<std::string> fields);
    DBResult<bool> hexists(std::string key, std::string field);
    DBResult<std::vector<std::pair<std::string, std::string>>> hgetall(
            std::string key);
    DBResult<size_t> hlen(std::string key);
    DBResult<std::vector<std::string>> hkeys(std::string key);
    DBResult<std::vector<std::string>> hvals(std::string key);

    // Sets
    DBResult<size_t> sadd(std::string key, std::vector<std::string> members);
    DBResult<size_t> srem(std::string key, std::vector<std::string> members);
    DBResult<std::vector<std::string>> smembers(std::string key);
    DBResult<bool> sismember(std::string key, std::string member);
    DBResult<size_t> scard(std::string key);

    // Sorted Sets
    DBResult<size_t> zadd(std::string, double score, std::string member);
    DBResult<std::vector<std::pair<std::string, double>>> zrange(
            std::string key, long long start, long long stop);
    DBResult<std::optional<double>> zscore(std::string key, std::string member);
    DBResult<size_t> zrem(std::string key, std::vector<std::string> members);
    DBResult<size_t> zcard(std::string key);

private:
    std::unordered_map<std::string, Value> db;
};

#endif
