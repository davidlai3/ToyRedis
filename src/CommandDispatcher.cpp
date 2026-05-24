#include "../include/CommandDispatcher.h"

#include <bit>
#include <functional>
#include <unordered_map>

namespace {
    // Health Commands
    RespValue echo(const std::vector<std::string_view>& args, Database& db) {
        RespValue ret;
        if (args.size() <= 1) {
            ret.value = RespSimpleString{};
        }
        else {
            ret.value = RespSimpleString{std::string{args[1]}};
        }

        return ret;
    }
    RespValue ping(const std::vector<std::string_view>& args, Database& db) {
        RespValue ret;
        if (args.size() <= 1) {
            ret.value = RespSimpleString{};
        }
        else {
            ret.value = RespSimpleString{"PONG"};
        }

        return ret;
    }

    // Generic Commands
    RespValue exists(const std::vector<std::string_view>& args, Database& db);
    RespValue dbsize(const std::vector<std::string_view>& args, Database& db);
    RespValue del(const std::vector<std::string_view>& args, Database& db);
    RespValue keys(const std::vector<std::string_view>& args, Database& db);
    RespValue type(const std::vector<std::string_view>& args, Database& db);

    // String Commands
    RespValue append(const std::vector<std::string_view>& args, Database& db);
    RespValue decr(const std::vector<std::string_view>& args, Database& db);
    RespValue decrby(const std::vector<std::string_view>& args, Database& db);
    RespValue incr(const std::vector<std::string_view>& args, Database& db);
    RespValue incrby(const std::vector<std::string_view>& args, Database& db);
    RespValue get(const std::vector<std::string_view>& args, Database& db);
    RespValue getdel(const std::vector<std::string_view>& args, Database& db);
    RespValue set(const std::vector<std::string_view>& args, Database& db);
    RespValue setnx(const std::vector<std::string_view>& args, Database& db);
    RespValue strlen(const std::vector<std::string_view>& args, Database& db);

    // List Commands
    RespValue lindex(const std::vector<std::string_view>& args, Database& db);
    RespValue llen(const std::vector<std::string_view>& args, Database& db);
    RespValue lpop(const std::vector<std::string_view>& args, Database& db);
    RespValue lpush(const std::vector<std::string_view>& args, Database& db);
    RespValue lrange(const std::vector<std::string_view>& args, Database& db);
    RespValue rpop(const std::vector<std::string_view>& args, Database& db);
    RespValue rpush(const std::vector<std::string_view>& args, Database& db);

    // Hash Commands
    RespValue hdel(const std::vector<std::string_view>& args, Database& db);
    RespValue hexists(const std::vector<std::string_view>& args, Database& db);
    RespValue hget(const std::vector<std::string_view>& args, Database& db);
    RespValue hgetall(const std::vector<std::string_view>& args, Database& db);
    RespValue hkeys(const std::vector<std::string_view>& args, Database& db);
    RespValue hlen(const std::vector<std::string_view>& args, Database& db);
    RespValue hset(const std::vector<std::string_view>& args, Database& db);
    RespValue hvals(const std::vector<std::string_view>& args, Database& db);

    // Set Commands
    RespValue sadd(const std::vector<std::string_view>& args, Database& db);
    RespValue scard(const std::vector<std::string_view>& args, Database& db);
    RespValue sismember(const std::vector<std::string_view>& args, Database& db);
    RespValue smembers(const std::vector<std::string_view>& args, Database& db);
    RespValue srem(const std::vector<std::string_view>& args, Database& db);

    // Sorted Set Commands
    RespValue zadd(const std::vector<std::string_view>& args, Database& db);
    RespValue zcard(const std::vector<std::string_view>& args, Database& db);
    RespValue zrange(const std::vector<std::string_view>& args, Database& db);
    RespValue zrem(const std::vector<std::string_view>& args, Database& db);
    RespValue zscore(const std::vector<std::string_view>& args, Database& db);
    
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
