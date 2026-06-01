#include <cassert>
#include <iostream>
#include <string>
#include <variant>
#include <vector>

#include "../include/Database.h"

#define CHECK(cond, msg) \
    do { \
        if (!(cond)) { \
            std::cerr << "CHECK failed at " << __FILE__ << ":" << __LINE__ \
                << ": " << msg << "\n"; \
            std::abort(); \
        } \
    } while (0)

// Shortcuts for the DBError variants -- keeps the assertions readable.
using Err = Database::DBError;

// ---- Helpers ----------------------------------------------------------------

template <typename T, typename R>
static T unwrap(R&& res, const std::string& tag) {
    CHECK(std::holds_alternative<T>(res),
            tag << ": expected success value, got DBError");
    return std::get<T>(res);
}

template <typename R>
static Err unwrap_err(R&& res, const std::string& tag) {
    CHECK(std::holds_alternative<Err>(res),
            tag << ": expected DBError, got success value");
    return std::get<Err>(res);
}

// ---- Generic ----------------------------------------------------------------

void test_set_get_del_exists() {
    Database db;

    // empty database
    CHECK(unwrap<bool>(db.exists("k"), "exists missing") == false, "exists missing");
    CHECK(unwrap<std::optional<std::string>>(db.get("k"), "get missing").has_value() == false,
            "get missing");

    // set then get
    db.set("k", std::string{"hello"});
    CHECK(unwrap<bool>(db.exists("k"), "exists present") == true, "exists present");
    auto got = unwrap<std::optional<std::string>>(db.get("k"), "get present");
    CHECK(got.has_value() && got.value() == "hello", "get returns stored value");

    // SET overwrites regardless of type
    db.set("k", std::string{"world"});
    auto got2 = unwrap<std::optional<std::string>>(db.get("k"), "get after overwrite");
    CHECK(got2.value() == "world", "set overwrites");

    // del existing -> true, then missing -> false
    CHECK(unwrap<bool>(db.del("k"), "del present") == true, "del present");
    CHECK(unwrap<bool>(db.del("k"), "del missing") == false, "del missing");
    CHECK(unwrap<bool>(db.exists("k"), "exists after del") == false, "exists after del");
}

void test_type_and_size() {
    Database db;
    CHECK(unwrap<size_t>(db.size(), "empty size") == 0, "empty size");

    auto type_missing = unwrap<std::optional<std::string>>(db.type("k"), "type missing");
    CHECK(!type_missing.has_value(), "type missing returns nullopt");

    db.set("s", std::string{"x"});
    db.lpush("l", {"a"});
    db.hset("h", {{"f", "v"}});
    db.sadd("st", {"m"});
    db.zadd("z", 1.0, "zm");

    CHECK(unwrap<size_t>(db.size(), "size after 5 keys") == 5, "size after 5 keys");

    CHECK(unwrap<std::optional<std::string>>(db.type("s"), "type str").value() == "String",
            "type string");
    CHECK(unwrap<std::optional<std::string>>(db.type("l"), "type list").value() == "List",
            "type list");
    CHECK(unwrap<std::optional<std::string>>(db.type("h"), "type hash").value() == "Hash",
            "type hash");
    CHECK(unwrap<std::optional<std::string>>(db.type("st"), "type set").value() == "Set",
            "type set");
    CHECK(unwrap<std::optional<std::string>>(db.type("z"), "type zset").value() == "Sorted Set",
            "type sorted set");
}

void test_keys() {
    Database db;
    CHECK(unwrap<std::vector<std::string>>(db.keys(), "keys empty").empty(), "keys empty");

    db.set("a", std::string{"1"});
    db.set("b", std::string{"2"});
    db.set("c", std::string{"3"});

    auto ks = unwrap<std::vector<std::string>>(db.keys(), "keys 3");
    CHECK(ks.size() == 3, "keys returns 3");
    // Order is unspecified for unordered_map; just verify the contents.
    std::sort(ks.begin(), ks.end());
    CHECK(ks[0] == "a" && ks[1] == "b" && ks[2] == "c", "keys content");
}

// ---- Strings ----------------------------------------------------------------

void test_setnx() {
    Database db;
    CHECK(unwrap<bool>(db.setnx("k", std::string{"first"}), "setnx new") == true,
            "setnx on new key returns true");

    auto got = unwrap<std::optional<std::string>>(db.get("k"), "get after setnx");
    CHECK(got.value() == "first", "setnx wrote value");

    CHECK(unwrap<bool>(db.setnx("k", std::string{"second"}), "setnx existing") == false,
            "setnx on existing key returns false");

    auto got2 = unwrap<std::optional<std::string>>(db.get("k"), "get unchanged");
    CHECK(got2.value() == "first", "setnx did not overwrite");
}

void test_append_and_strlen() {
    Database db;

    // append to missing key creates it
    CHECK(unwrap<size_t>(db.append("k", "abc"), "append create") == 3, "append create -> len 3");
    CHECK(unwrap<size_t>(db.strlen("k"), "strlen 3") == 3, "strlen 3");

    // append extends
    CHECK(unwrap<size_t>(db.append("k", "de"), "append extend") == 5, "append extend -> len 5");
    CHECK(unwrap<std::optional<std::string>>(db.get("k"), "get appended").value() == "abcde",
            "value is 'abcde' after append");

    // strlen on missing -> 0
    CHECK(unwrap<size_t>(db.strlen("missing"), "strlen missing") == 0, "strlen missing -> 0");
}

void test_incrby_basic() {
    Database db;

    // missing key initializes to delta
    CHECK(unwrap<long long>(db.incrby("c", 5), "incrby create") == 5, "incrby create");
    CHECK(unwrap<long long>(db.incrby("c", 3), "incrby +3") == 8, "incrby +3 -> 8");
    CHECK(unwrap<long long>(db.incrby("c", -10), "incrby -10") == -2, "incrby -10 -> -2");

    // existing non-integer value -> NOTINTEGER
    db.set("s", std::string{"hello"});
    CHECK(unwrap_err(db.incrby("s", 1), "incrby on non-int") == Err::NOTINTEGER,
            "incrby on non-integer string -> NOTINTEGER");
}

void test_wrongtype_string_ops() {
    Database db;
    db.lpush("list", {"a"});

    CHECK(unwrap_err(db.get("list"), "get list") == Err::WRONGTYPE, "GET on list -> WRONGTYPE");
    CHECK(unwrap_err(db.append("list", "x"), "append list") == Err::WRONGTYPE,
            "APPEND on list -> WRONGTYPE");
    CHECK(unwrap_err(db.strlen("list"), "strlen list") == Err::WRONGTYPE,
            "STRLEN on list -> WRONGTYPE");
    CHECK(unwrap_err(db.incrby("list", 1), "incrby list") == Err::WRONGTYPE,
            "INCRBY on list -> WRONGTYPE");
}

// ---- Lists ------------------------------------------------------------------

void test_list_push_pop() {
    Database db;

    CHECK(unwrap<size_t>(db.rpush("l", {"a", "b", "c"}), "rpush 3") == 3, "rpush 3 -> len 3");
    // List is now [a, b, c]
    CHECK(unwrap<size_t>(db.llen("l"), "llen 3") == 3, "llen 3");

    // LPUSH adds at the front, one at a time
    CHECK(unwrap<size_t>(db.lpush("l", {"x", "y"}), "lpush 2") == 5, "lpush 2 -> len 5");
    // After LPUSH x then y, list is [y, x, a, b, c]
    auto first = unwrap<std::optional<std::string>>(db.lindex("l", 0), "lindex 0");
    CHECK(first.value() == "y", "lindex 0 -> 'y'");
    auto last = unwrap<std::optional<std::string>>(db.lindex("l", -1), "lindex -1");
    CHECK(last.value() == "c", "lindex -1 -> 'c'");

    // lpop / rpop
    auto popped_front = unwrap<std::optional<std::string>>(db.lpop("l"), "lpop");
    CHECK(popped_front.value() == "y", "lpop -> 'y'");
    auto popped_back = unwrap<std::optional<std::string>>(db.rpop("l"), "rpop");
    CHECK(popped_back.value() == "c", "rpop -> 'c'");
    // List is now [x, a, b]
    CHECK(unwrap<size_t>(db.llen("l"), "llen post-pop") == 3, "llen 3 after pops");
}

void test_list_empty_deletes_key() {
    Database db;
    db.rpush("l", {"only"});
    CHECK(unwrap<bool>(db.exists("l"), "exists before pop") == true, "exists before pop");

    auto popped = unwrap<std::optional<std::string>>(db.lpop("l"), "final pop");
    CHECK(popped.value() == "only", "popped the only element");
    // After popping the last element, the key should disappear.
    CHECK(unwrap<bool>(db.exists("l"), "exists after pop") == false,
            "empty list deletes its key");
}

void test_lpop_rpop_missing() {
    Database db;
    auto p = unwrap<std::optional<std::string>>(db.lpop("nope"), "lpop missing");
    CHECK(!p.has_value(), "lpop missing -> nullopt");
}

void test_lrange() {
    Database db;
    db.rpush("l", {"a", "b", "c", "d", "e"});

    auto whole = unwrap<std::vector<std::string>>(db.lrange("l", 0, -1), "lrange whole");
    CHECK(whole.size() == 5 && whole[0] == "a" && whole[4] == "e", "lrange 0 -1 -> all");

    auto mid = unwrap<std::vector<std::string>>(db.lrange("l", 1, 3), "lrange mid");
    CHECK(mid.size() == 3 && mid[0] == "b" && mid[2] == "d", "lrange 1 3 -> [b,c,d]");

    auto neg = unwrap<std::vector<std::string>>(db.lrange("l", -2, -1), "lrange neg");
    CHECK(neg.size() == 2 && neg[0] == "d" && neg[1] == "e", "lrange -2 -1 -> [d,e]");

    auto past = unwrap<std::vector<std::string>>(db.lrange("l", 10, 20), "lrange past");
    CHECK(past.empty(), "lrange past end -> empty");
}

void test_lindex_bounds() {
    Database db;
    db.rpush("l", {"a", "b"});

    CHECK(unwrap<std::optional<std::string>>(db.lindex("l", 0), "lindex 0").value() == "a",
            "lindex 0");
    CHECK(unwrap<std::optional<std::string>>(db.lindex("l", 1), "lindex 1").value() == "b",
            "lindex 1");
    CHECK(!unwrap<std::optional<std::string>>(db.lindex("l", 99), "lindex oob+").has_value(),
            "lindex out of range -> nullopt");
    CHECK(unwrap<std::optional<std::string>>(db.lindex("l", -1), "lindex -1").value() == "b",
            "lindex -1 -> last");
    CHECK(!unwrap<std::optional<std::string>>(db.lindex("l", -99), "lindex -oob").has_value(),
            "lindex -99 out of range -> nullopt");
}

void test_wrongtype_list_ops() {
    Database db;
    db.set("s", std::string{"x"});
    CHECK(unwrap_err(db.lpush("s", {"a"}), "lpush on str") == Err::WRONGTYPE, "LPUSH WRONGTYPE");
    CHECK(unwrap_err(db.llen("s"), "llen on str") == Err::WRONGTYPE, "LLEN WRONGTYPE");
    CHECK(unwrap_err(db.lindex("s", 0), "lindex on str") == Err::WRONGTYPE, "LINDEX WRONGTYPE");
    CHECK(unwrap_err(db.lrange("s", 0, -1), "lrange on str") == Err::WRONGTYPE, "LRANGE WRONGTYPE");
}

// ---- Hashes -----------------------------------------------------------------

void test_hset_hget() {
    Database db;

    // adding two new fields returns count = 2
    CHECK(unwrap<size_t>(db.hset("h", {{"a", "1"}, {"b", "2"}}), "hset new 2") == 2,
            "hset 2 new fields -> 2");

    // updating existing field returns 0 (not counted as added)
    CHECK(unwrap<size_t>(db.hset("h", {{"a", "10"}}), "hset update") == 0,
            "hset update existing -> 0");

    // mixed: one new + one update -> 1
    CHECK(unwrap<size_t>(db.hset("h", {{"c", "3"}, {"b", "20"}}), "hset mixed") == 1,
            "hset 1 new + 1 update -> 1");

    auto a = unwrap<std::optional<std::string>>(db.hget("h", "a"), "hget a");
    CHECK(a.value() == "10", "hget a -> '10' (updated value)");

    auto missing = unwrap<std::optional<std::string>>(db.hget("h", "z"), "hget missing field");
    CHECK(!missing.has_value(), "hget missing field -> nullopt");
}

void test_hdel_empty_deletes_key() {
    Database db;
    db.hset("h", {{"a", "1"}, {"b", "2"}});

    CHECK(unwrap<size_t>(db.hdel("h", {"a", "missing"}), "hdel 1 of 2") == 1,
            "hdel removes one existing field");
    CHECK(unwrap<bool>(db.exists("h"), "exists after partial hdel") == true,
            "hash with remaining fields still exists");

    CHECK(unwrap<size_t>(db.hdel("h", {"b"}), "hdel last") == 1, "hdel last field");
    CHECK(unwrap<bool>(db.exists("h"), "exists after empty") == false,
            "empty hash deletes its key");
}

void test_hexists_hlen_hkeys_hvals_hgetall() {
    Database db;
    db.hset("h", {{"a", "1"}, {"b", "2"}});

    CHECK(unwrap<bool>(db.hexists("h", "a"), "hexists a") == true, "hexists existing");
    CHECK(unwrap<bool>(db.hexists("h", "z"), "hexists z") == false, "hexists missing");
    CHECK(unwrap<size_t>(db.hlen("h"), "hlen") == 2, "hlen 2");

    auto ks = unwrap<std::vector<std::string>>(db.hkeys("h"), "hkeys");
    std::sort(ks.begin(), ks.end());
    CHECK(ks.size() == 2 && ks[0] == "a" && ks[1] == "b", "hkeys");

    auto vs = unwrap<std::vector<std::string>>(db.hvals("h"), "hvals");
    std::sort(vs.begin(), vs.end());
    CHECK(vs.size() == 2 && vs[0] == "1" && vs[1] == "2", "hvals");

    auto all = unwrap<std::vector<std::pair<std::string, std::string>>>(
            db.hgetall("h"), "hgetall");
    CHECK(all.size() == 2, "hgetall size 2");
}

void test_wrongtype_hash_ops() {
    Database db;
    db.set("s", std::string{"x"});
    CHECK(unwrap_err(db.hset("s", {{"f", "v"}}), "hset on str") == Err::WRONGTYPE,
            "HSET WRONGTYPE");
    CHECK(unwrap_err(db.hget("s", "f"), "hget on str") == Err::WRONGTYPE, "HGET WRONGTYPE");
    CHECK(unwrap_err(db.hdel("s", {"f"}), "hdel on str") == Err::WRONGTYPE, "HDEL WRONGTYPE");
}

// ---- Sets -------------------------------------------------------------------

void test_sadd_srem_basics() {
    Database db;

    CHECK(unwrap<size_t>(db.sadd("s", {"a", "b", "c"}), "sadd 3 new") == 3, "sadd 3 new");
    // re-adding existing members counts as 0 added
    CHECK(unwrap<size_t>(db.sadd("s", {"a", "d"}), "sadd 1 new 1 dup") == 1,
            "sadd 1 new + 1 duplicate -> 1");

    CHECK(unwrap<size_t>(db.scard("s"), "scard 4") == 4, "scard 4");
    CHECK(unwrap<bool>(db.sismember("s", "a"), "sismember present") == true,
            "sismember present");
    CHECK(unwrap<bool>(db.sismember("s", "z"), "sismember missing") == false,
            "sismember missing");

    CHECK(unwrap<size_t>(db.srem("s", {"a", "z"}), "srem 1 of 2") == 1,
            "srem one existing + one missing -> 1");
}

void test_set_empty_deletes_key() {
    Database db;
    db.sadd("s", {"only"});
    CHECK(unwrap<size_t>(db.srem("s", {"only"}), "srem last") == 1, "srem last");
    CHECK(unwrap<bool>(db.exists("s"), "exists after srem all") == false,
            "empty set deletes its key");
}

void test_smembers() {
    Database db;
    db.sadd("s", {"a", "b", "c"});
    auto ms = unwrap<std::vector<std::string>>(db.smembers("s"), "smembers");
    std::sort(ms.begin(), ms.end());
    CHECK(ms.size() == 3 && ms[0] == "a" && ms[2] == "c", "smembers content");
}

void test_wrongtype_set_ops() {
    Database db;
    db.set("k", std::string{"x"});
    CHECK(unwrap_err(db.sadd("k", {"a"}), "sadd on str") == Err::WRONGTYPE, "SADD WRONGTYPE");
    CHECK(unwrap_err(db.smembers("k"), "smembers on str") == Err::WRONGTYPE, "SMEMBERS WRONGTYPE");
}

// ---- Sorted Sets ------------------------------------------------------------

void test_zadd_zscore_zcard() {
    Database db;

    CHECK(unwrap<size_t>(db.zadd("z", 1.0, "a"), "zadd new a") == 1, "zadd new a -> 1");
    CHECK(unwrap<size_t>(db.zadd("z", 2.0, "b"), "zadd new b") == 1, "zadd new b -> 1");
    CHECK(unwrap<size_t>(db.zcard("z"), "zcard 2") == 2, "zcard 2");

    // Score update on existing member returns 0 added.
    CHECK(unwrap<size_t>(db.zadd("z", 10.0, "a"), "zadd update a") == 0,
            "zadd score update -> 0");

    auto sa = unwrap<std::optional<double>>(db.zscore("z", "a"), "zscore a");
    CHECK(sa.has_value() && sa.value() == 10.0, "zscore reflects update");

    auto sb = unwrap<std::optional<double>>(db.zscore("z", "b"), "zscore b");
    CHECK(sb.value() == 2.0, "zscore unchanged member");

    auto missing = unwrap<std::optional<double>>(db.zscore("z", "ghost"), "zscore missing");
    CHECK(!missing.has_value(), "zscore missing member -> nullopt");
}

void test_zrange_ordering_with_score_update() {
    Database db;
    // Insert out of order, expect zrange to return ascending-by-score.
    db.zadd("z", 3.0, "c");
    db.zadd("z", 1.0, "a");
    db.zadd("z", 2.0, "b");

    auto r = unwrap<std::vector<std::pair<std::string, double>>>(
            db.zrange("z", 0, -1), "zrange ordered");
    CHECK(r.size() == 3, "zrange 3 elems");
    CHECK(r[0].first == "a" && r[0].second == 1.0, "rank 0 is a");
    CHECK(r[1].first == "b" && r[1].second == 2.0, "rank 1 is b");
    CHECK(r[2].first == "c" && r[2].second == 3.0, "rank 2 is c");

    // Update b's score so it overtakes c -- this is the dual-structure-sync
    // case where forgetting to remove the old (score, member) leaves a stale
    // rank entry.
    db.zadd("z", 99.0, "b");
    auto r2 = unwrap<std::vector<std::pair<std::string, double>>>(
            db.zrange("z", 0, -1), "zrange after update");
    CHECK(r2.size() == 3, "still 3 elements (not 4) after score update");
    // After the update, ascending order is a(1) c(3) b(99)
    CHECK(r2[0].first == "a", "rank 0 still a");
    CHECK(r2[1].first == "c", "rank 1 now c (was b)");
    CHECK(r2[2].first == "b" && r2[2].second == 99.0, "rank 2 now b at score 99");
}

void test_zrem_empty_deletes_key() {
    Database db;
    db.zadd("z", 1.0, "only");
    CHECK(unwrap<size_t>(db.zrem("z", {"only"}), "zrem last") == 1, "zrem last");
    CHECK(unwrap<bool>(db.exists("z"), "exists after zrem all") == false,
            "empty sorted set deletes its key");
}

void test_wrongtype_zset_ops() {
    Database db;
    db.set("k", std::string{"x"});
    CHECK(unwrap_err(db.zadd("k", 1.0, "m"), "zadd on str") == Err::WRONGTYPE,
            "ZADD WRONGTYPE");
    CHECK(unwrap_err(db.zscore("k", "m"), "zscore on str") == Err::WRONGTYPE,
            "ZSCORE WRONGTYPE");
    CHECK(unwrap_err(db.zrange("k", 0, -1), "zrange on str") == Err::WRONGTYPE,
            "ZRANGE WRONGTYPE");
}

// ---- main -------------------------------------------------------------------

int main() {
    test_set_get_del_exists();
    test_type_and_size();
    test_keys();

    test_setnx();
    test_append_and_strlen();
    test_incrby_basic();
    test_wrongtype_string_ops();

    test_list_push_pop();
    test_list_empty_deletes_key();
    test_lpop_rpop_missing();
    test_lrange();
    test_lindex_bounds();
    test_wrongtype_list_ops();

    test_hset_hget();
    test_hdel_empty_deletes_key();
    test_hexists_hlen_hkeys_hvals_hgetall();
    test_wrongtype_hash_ops();

    test_sadd_srem_basics();
    test_set_empty_deletes_key();
    test_smembers();
    test_wrongtype_set_ops();

    test_zadd_zscore_zcard();
    test_zrange_ordering_with_score_update();
    test_zrem_empty_deletes_key();
    test_wrongtype_zset_ops();

    std::cout << "All tests passed\n";
    return 0;
}
