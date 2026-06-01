#include <cassert>
#include <initializer_list>
#include <iostream>
#include <string>
#include <variant>
#include <vector>

#include "../include/CommandDispatcher.h"
#include "../include/Database.h"
#include "../include/RespSerializer.h"
#include "../include/RespValue.hpp"

#define CHECK(cond, msg) \
    do { \
        if (!(cond)) { \
            std::cerr << "CHECK failed at " << __FILE__ << ":" << __LINE__ \
                << ": " << msg << "\n"; \
            std::abort(); \
        } \
    } while (0)

// ---- Helpers ----------------------------------------------------------------

// Build a RESP command: an array of bulk strings, the shape every real client
// sends.
static RespValue cmd(std::initializer_list<std::string> args) {
    std::vector<RespValue> vals;
    vals.reserve(args.size());
    for (const std::string& s : args) {
        vals.push_back(RespValue{RespBulkString{s}});
    }
    return RespValue{RespArray{std::move(vals)}};
}

// Serialize a RespValue to bytes-as-string for compact wire-format assertions.
static std::string wire(const RespValue& v) {
    auto bytes = serialize(v);
    return std::string{bytes.begin(), bytes.end()};
}

// ---- Routing ----------------------------------------------------------------

void test_case_insensitive_dispatch() {
    Database db;
    // The verb should be uppercased before lookup, so all of these should work.
    for (const char* v : {"PING", "ping", "Ping", "PiNg"}) {
        auto resp = dispatch(cmd({v}), db);
        CHECK(wire(resp) == "+PONG\r\n",
                "case-insensitive dispatch failed for verb=" << v);
    }
}

void test_unknown_command() {
    Database db;
    auto resp = dispatch(cmd({"NOSUCHCOMMAND"}), db);
    CHECK(std::holds_alternative<RespSimpleError>(resp.value),
            "unknown command -> error");
}

// ---- Malformed command shapes -----------------------------------------------

void test_non_array_command() {
    Database db;
    // A raw simple string is not a valid command (clients always send arrays).
    auto resp = dispatch(RespValue{RespSimpleString{"PING"}}, db);
    CHECK(std::holds_alternative<RespSimpleError>(resp.value),
            "non-array command -> error");
}

void test_empty_array_command() {
    Database db;
    auto resp = dispatch(RespValue{RespArray{}}, db);
    CHECK(std::holds_alternative<RespSimpleError>(resp.value),
            "empty array command -> error");
}

void test_non_bulk_element_command() {
    Database db;
    // Array containing an integer rather than a bulk string is invalid.
    std::vector<RespValue> bad;
    bad.push_back(RespValue{RespBulkString{"PING"}});
    bad.push_back(RespValue{RespInteger{42}});
    auto resp = dispatch(RespValue{RespArray{std::move(bad)}}, db);
    CHECK(std::holds_alternative<RespSimpleError>(resp.value),
            "non-bulk element in command -> error");
}

// ---- Health -----------------------------------------------------------------

void test_ping() {
    Database db;
    CHECK(wire(dispatch(cmd({"PING"}), db)) == "+PONG\r\n", "PING -> +PONG");
    CHECK(wire(dispatch(cmd({"PING", "hello"}), db)) == "$5\r\nhello\r\n",
            "PING msg -> bulk echo");
}

void test_echo() {
    Database db;
    CHECK(wire(dispatch(cmd({"ECHO", "world"}), db)) == "$5\r\nworld\r\n",
            "ECHO -> bulk string");
    // Wrong arg count
    CHECK(std::holds_alternative<RespSimpleError>(dispatch(cmd({"ECHO"}), db).value),
            "ECHO with no arg -> error");
}

// ---- Generic ----------------------------------------------------------------

void test_set_get_via_dispatcher() {
    Database db;
    CHECK(wire(dispatch(cmd({"SET", "k", "hello"}), db)) == "+OK\r\n", "SET -> +OK");
    CHECK(wire(dispatch(cmd({"GET", "k"}), db)) == "$5\r\nhello\r\n", "GET -> bulk");
}

void test_get_missing_key_returns_null_bulk() {
    Database db;
    // GET on a missing key must return the nil bulk ($-1\r\n), NOT +none.
    CHECK(wire(dispatch(cmd({"GET", "missing"}), db)) == "$-1\r\n",
            "GET missing -> $-1");
}

void test_get_wrongtype_returns_error() {
    Database db;
    dispatch(cmd({"LPUSH", "list", "a"}), db);
    auto resp = dispatch(cmd({"GET", "list"}), db);
    CHECK(std::holds_alternative<RespSimpleError>(resp.value),
            "GET on a list -> WRONGTYPE error");
}

void test_exists_and_del() {
    Database db;
    dispatch(cmd({"SET", "a", "1"}), db);
    dispatch(cmd({"SET", "b", "2"}), db);

    // EXISTS supports multiple keys -> count.
    CHECK(wire(dispatch(cmd({"EXISTS", "a", "b", "missing"}), db)) == ":2\r\n",
            "EXISTS counts existing keys");

    // DEL supports multiple keys -> count.
    CHECK(wire(dispatch(cmd({"DEL", "a", "b", "missing"}), db)) == ":2\r\n",
            "DEL returns count actually deleted");

    CHECK(wire(dispatch(cmd({"EXISTS", "a"}), db)) == ":0\r\n",
            "EXISTS after DEL -> 0");
}

void test_type() {
    Database db;
    // missing key -> +none (NOT an error, NOT nil)
    CHECK(wire(dispatch(cmd({"TYPE", "missing"}), db)) == "+none\r\n",
            "TYPE missing -> +none");

    dispatch(cmd({"SET", "s", "x"}), db);
    auto resp = dispatch(cmd({"TYPE", "s"}), db);
    CHECK(std::holds_alternative<RespSimpleString>(resp.value),
            "TYPE on string returns SimpleString");
}

void test_dbsize() {
    Database db;
    CHECK(wire(dispatch(cmd({"DBSIZE"}), db)) == ":0\r\n", "DBSIZE empty -> 0");
    dispatch(cmd({"SET", "a", "1"}), db);
    dispatch(cmd({"SET", "b", "2"}), db);
    CHECK(wire(dispatch(cmd({"DBSIZE"}), db)) == ":2\r\n", "DBSIZE after 2 sets -> 2");
}

// ---- Strings ----------------------------------------------------------------

void test_incr_family() {
    Database db;
    CHECK(wire(dispatch(cmd({"INCR", "c"}), db)) == ":1\r\n", "INCR new -> 1");
    CHECK(wire(dispatch(cmd({"INCR", "c"}), db)) == ":2\r\n", "INCR -> 2");
    CHECK(wire(dispatch(cmd({"INCRBY", "c", "10"}), db)) == ":12\r\n", "INCRBY 10 -> 12");
    CHECK(wire(dispatch(cmd({"DECR", "c"}), db)) == ":11\r\n", "DECR -> 11");
    CHECK(wire(dispatch(cmd({"DECRBY", "c", "5"}), db)) == ":6\r\n", "DECRBY 5 -> 6");
}

void test_incrby_invalid_argument() {
    Database db;
    // Non-numeric delta -> error (caught by the dispatcher's parse_ll).
    auto resp = dispatch(cmd({"INCRBY", "c", "abc"}), db);
    CHECK(std::holds_alternative<RespSimpleError>(resp.value),
            "INCRBY with non-integer delta -> error");
}

void test_incr_on_non_integer_value() {
    Database db;
    dispatch(cmd({"SET", "k", "hello"}), db);
    auto resp = dispatch(cmd({"INCR", "k"}), db);
    CHECK(std::holds_alternative<RespSimpleError>(resp.value),
            "INCR on non-integer string -> error");
}

void test_setnx_dispatch() {
    Database db;
    CHECK(wire(dispatch(cmd({"SETNX", "k", "first"}), db)) == ":1\r\n",
            "SETNX new -> 1");
    CHECK(wire(dispatch(cmd({"SETNX", "k", "second"}), db)) == ":0\r\n",
            "SETNX existing -> 0");
    CHECK(wire(dispatch(cmd({"GET", "k"}), db)) == "$5\r\nfirst\r\n",
            "SETNX did not overwrite");
}

void test_strlen_append_dispatch() {
    Database db;
    CHECK(wire(dispatch(cmd({"APPEND", "k", "abc"}), db)) == ":3\r\n", "APPEND -> 3");
    CHECK(wire(dispatch(cmd({"APPEND", "k", "de"}), db)) == ":5\r\n", "APPEND -> 5");
    CHECK(wire(dispatch(cmd({"STRLEN", "k"}), db)) == ":5\r\n", "STRLEN -> 5");
    CHECK(wire(dispatch(cmd({"GET", "k"}), db)) == "$5\r\nabcde\r\n",
            "APPEND persisted");
}

// ---- Lists ------------------------------------------------------------------

void test_list_dispatch() {
    Database db;
    CHECK(wire(dispatch(cmd({"RPUSH", "l", "a", "b", "c"}), db)) == ":3\r\n",
            "RPUSH 3 -> 3");
    CHECK(wire(dispatch(cmd({"LLEN", "l"}), db)) == ":3\r\n", "LLEN -> 3");

    // LRANGE returns an array of bulk strings.
    auto r = dispatch(cmd({"LRANGE", "l", "0", "-1"}), db);
    CHECK(wire(r) == "*3\r\n$1\r\na\r\n$1\r\nb\r\n$1\r\nc\r\n",
            "LRANGE 0 -1 -> [a,b,c]");

    CHECK(wire(dispatch(cmd({"LINDEX", "l", "1"}), db)) == "$1\r\nb\r\n",
            "LINDEX 1 -> b");
    CHECK(wire(dispatch(cmd({"LINDEX", "l", "99"}), db)) == "$-1\r\n",
            "LINDEX out of range -> nil");
}

void test_lpop_missing_returns_null_bulk() {
    Database db;
    CHECK(wire(dispatch(cmd({"LPOP", "nope"}), db)) == "$-1\r\n",
            "LPOP missing -> nil bulk");
}

void test_lrange_non_integer_index() {
    Database db;
    dispatch(cmd({"RPUSH", "l", "a"}), db);
    auto resp = dispatch(cmd({"LRANGE", "l", "x", "y"}), db);
    CHECK(std::holds_alternative<RespSimpleError>(resp.value),
            "LRANGE with non-integer indices -> error");
}

// ---- Hashes -----------------------------------------------------------------

void test_hash_dispatch() {
    Database db;
    CHECK(wire(dispatch(cmd({"HSET", "h", "a", "1", "b", "2"}), db)) == ":2\r\n",
            "HSET 2 new fields -> 2");
    CHECK(wire(dispatch(cmd({"HGET", "h", "a"}), db)) == "$1\r\n1\r\n", "HGET -> '1'");
    CHECK(wire(dispatch(cmd({"HGET", "h", "ghost"}), db)) == "$-1\r\n",
            "HGET missing field -> nil");
    CHECK(wire(dispatch(cmd({"HLEN", "h"}), db)) == ":2\r\n", "HLEN -> 2");
    CHECK(wire(dispatch(cmd({"HEXISTS", "h", "a"}), db)) == ":1\r\n", "HEXISTS yes -> 1");
    CHECK(wire(dispatch(cmd({"HEXISTS", "h", "z"}), db)) == ":0\r\n", "HEXISTS no -> 0");

    // HGETALL returns a FLAT array: [field1, val1, field2, val2, ...].
    auto resp = dispatch(cmd({"HGETALL", "h"}), db);
    CHECK(std::holds_alternative<RespArray>(resp.value), "HGETALL -> array");
    CHECK(std::get<RespArray>(resp.value).vals.size() == 4,
            "HGETALL flat array has 4 elements for 2 pairs");
}

void test_hset_odd_args_is_error() {
    Database db;
    // HSET requires complete field/value pairs after the key. "HSET k f" is
    // odd-numbered and must be rejected without writing anything.
    auto resp = dispatch(cmd({"HSET", "h", "f"}), db);
    CHECK(std::holds_alternative<RespSimpleError>(resp.value),
            "HSET with odd field/value args -> error");
    CHECK(wire(dispatch(cmd({"EXISTS", "h"}), db)) == ":0\r\n",
            "HSET error did not create the key");
}

// ---- Sets -------------------------------------------------------------------

void test_set_dispatch() {
    Database db;
    CHECK(wire(dispatch(cmd({"SADD", "s", "a", "b", "c"}), db)) == ":3\r\n",
            "SADD 3 new -> 3");
    CHECK(wire(dispatch(cmd({"SADD", "s", "a", "d"}), db)) == ":1\r\n",
            "SADD 1 dup + 1 new -> 1");
    CHECK(wire(dispatch(cmd({"SCARD", "s"}), db)) == ":4\r\n", "SCARD -> 4");
    CHECK(wire(dispatch(cmd({"SISMEMBER", "s", "a"}), db)) == ":1\r\n",
            "SISMEMBER yes -> 1");
    CHECK(wire(dispatch(cmd({"SISMEMBER", "s", "z"}), db)) == ":0\r\n",
            "SISMEMBER no -> 0");
    CHECK(wire(dispatch(cmd({"SREM", "s", "a", "z"}), db)) == ":1\r\n",
            "SREM existing+missing -> 1");

    auto resp = dispatch(cmd({"SMEMBERS", "s"}), db);
    CHECK(std::holds_alternative<RespArray>(resp.value), "SMEMBERS -> array");
    CHECK(std::get<RespArray>(resp.value).vals.size() == 3,
            "SMEMBERS has 3 elements after one SREM");
}

// ---- Sorted Sets ------------------------------------------------------------

void test_zset_dispatch() {
    Database db;
    CHECK(wire(dispatch(cmd({"ZADD", "z", "1", "a"}), db)) == ":1\r\n",
            "ZADD new -> 1");
    CHECK(wire(dispatch(cmd({"ZADD", "z", "2", "b"}), db)) == ":1\r\n",
            "ZADD new b -> 1");
    CHECK(wire(dispatch(cmd({"ZADD", "z", "5", "a"}), db)) == ":0\r\n",
            "ZADD update score -> 0");
    CHECK(wire(dispatch(cmd({"ZSCORE", "z", "a"}), db)) == "$1\r\n5\r\n",
            "ZSCORE reflects updated score (whole number, no decimal)");
    CHECK(wire(dispatch(cmd({"ZSCORE", "z", "ghost"}), db)) == "$-1\r\n",
            "ZSCORE missing -> nil");
    CHECK(wire(dispatch(cmd({"ZCARD", "z"}), db)) == ":2\r\n", "ZCARD -> 2");

    // ZRANGE returns ascending-by-score: b(2), a(5)
    auto r = dispatch(cmd({"ZRANGE", "z", "0", "-1"}), db);
    CHECK(std::holds_alternative<RespArray>(r.value), "ZRANGE -> array");
    auto& arr = std::get<RespArray>(r.value).vals;
    CHECK(arr.size() == 2, "ZRANGE returns 2 members (no WITHSCORES)");
    CHECK(std::get<RespBulkString>(arr[0].value).msg == "b", "rank 0 is b");
    CHECK(std::get<RespBulkString>(arr[1].value).msg == "a", "rank 1 is a");
}

void test_zrange_withscores() {
    Database db;
    dispatch(cmd({"ZADD", "z", "1", "a"}), db);
    dispatch(cmd({"ZADD", "z", "2", "b"}), db);

    auto r = dispatch(cmd({"ZRANGE", "z", "0", "-1", "WITHSCORES"}), db);
    auto& arr = std::get<RespArray>(r.value).vals;
    CHECK(arr.size() == 4, "WITHSCORES doubles the array size");
    CHECK(std::get<RespBulkString>(arr[0].value).msg == "a", "elem 0: member a");
    CHECK(std::get<RespBulkString>(arr[1].value).msg == "1", "elem 1: score 1");
    CHECK(std::get<RespBulkString>(arr[2].value).msg == "b", "elem 2: member b");
    CHECK(std::get<RespBulkString>(arr[3].value).msg == "2", "elem 3: score 2");
}

void test_zrange_invalid_option() {
    Database db;
    dispatch(cmd({"ZADD", "z", "1", "a"}), db);
    // Extra arg that isn't WITHSCORES should be rejected.
    auto resp = dispatch(cmd({"ZRANGE", "z", "0", "-1", "BOGUS"}), db);
    CHECK(std::holds_alternative<RespSimpleError>(resp.value),
            "ZRANGE with unrecognized option -> error");
}

void test_zadd_invalid_score() {
    Database db;
    auto resp = dispatch(cmd({"ZADD", "z", "notanumber", "m"}), db);
    CHECK(std::holds_alternative<RespSimpleError>(resp.value),
            "ZADD with non-numeric score -> error");
}

// ---- Arg-count validation ---------------------------------------------------

void test_arg_count_errors() {
    Database db;
    // A few representative wrong-arity calls; the goal isn't to enumerate every
    // command, just to confirm that the arity checks fire and return errors.
    for (const auto& bad : std::initializer_list<std::initializer_list<std::string>>{
            {"GET"},                  // missing key
            {"GET", "k", "extra"},    // too many
            {"SET", "k"},             // missing value
            {"HSET", "k"},            // no field/value pairs
            {"LPUSH", "k"},           // no values
            {"ZADD", "k", "1"},       // missing member
            {"ZRANGE", "k", "0"},     // missing stop
            }) {
        auto resp = dispatch(cmd(bad), db);
        CHECK(std::holds_alternative<RespSimpleError>(resp.value),
                "expected arg-count error for the test case");
    }
}

// ---- main -------------------------------------------------------------------

int main() {
    test_case_insensitive_dispatch();
    test_unknown_command();

    test_non_array_command();
    test_empty_array_command();
    test_non_bulk_element_command();

    test_ping();
    test_echo();

    test_set_get_via_dispatcher();
    test_get_missing_key_returns_null_bulk();
    test_get_wrongtype_returns_error();
    test_exists_and_del();
    test_type();
    test_dbsize();

    test_incr_family();
    test_incrby_invalid_argument();
    test_incr_on_non_integer_value();
    test_setnx_dispatch();
    test_strlen_append_dispatch();

    test_list_dispatch();
    test_lpop_missing_returns_null_bulk();
    test_lrange_non_integer_index();

    test_hash_dispatch();
    test_hset_odd_args_is_error();

    test_set_dispatch();

    test_zset_dispatch();
    test_zrange_withscores();
    test_zrange_invalid_option();
    test_zadd_invalid_score();

    test_arg_count_errors();

    std::cout << "All tests passed\n";
    return 0;
}
