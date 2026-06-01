#include <cassert>
#include <iostream>
#include <string>
#include <vector>

#include "../include/RespParser.h"
#include "../include/RespSerializer.h"

#define CHECK(cond, msg) \
    do { \
        if (!(cond)) { \
            std::cerr << "CHECK failed at " << __FILE__ << ":" << __LINE__ \
                << ": " << msg << "\n"; \
            std::abort(); \
        } \
    } while (0)

// Convenience: serialize and return the bytes as a std::string so we can
// compare against literal wire-format strings.
static std::string ser(const RespValue& v) {
    auto bytes = serialize(v);
    return std::string{bytes.begin(), bytes.end()};
}

// Round-trip: serialize -> parse -> serialize again, expect identical bytes.
// This catches drift in either direction without needing RespValue equality.
static void check_round_trip(const RespValue& v, const std::string& tag) {
    auto bytes1 = serialize(v);
    std::vector<char> buff{bytes1.begin(), bytes1.end()};

    RespValue parsed;
    RespParserCode code = parse_one(buff, parsed);

    CHECK(code == RespParserCode::COMPLETE,
            tag << ": parser did not return COMPLETE");
    CHECK(buff.empty(),
            tag << ": parser left " << buff.size() << " bytes unconsumed");

    auto bytes2 = serialize(parsed);
    CHECK(bytes1 == bytes2,
            tag << ": round-trip bytes mismatch");
}

// ---- Direct serialization tests ---------------------------------------------

void test_simple_string_serialize() {
    CHECK(ser(RespValue{RespSimpleString{"OK"}}) == "+OK\r\n",
            "+OK\\r\\n");
    CHECK(ser(RespValue{RespSimpleString{"PONG"}}) == "+PONG\r\n",
            "+PONG\\r\\n");
    CHECK(ser(RespValue{RespSimpleString{""}}) == "+\r\n",
            "empty simple string");
}

void test_simple_error_serialize() {
    CHECK(ser(RespValue{RespSimpleError{"ERR something bad"}})
            == "-ERR something bad\r\n",
            "error reply");
    CHECK(ser(RespValue{RespSimpleError{""}}) == "-\r\n",
            "empty error");
}

void test_integer_serialize() {
    CHECK(ser(RespValue{RespInteger{0}}) == ":0\r\n", ":0");
    CHECK(ser(RespValue{RespInteger{42}}) == ":42\r\n", ":42");
    CHECK(ser(RespValue{RespInteger{-1}}) == ":-1\r\n", ":-1");
    CHECK(ser(RespValue{RespInteger{1234567890LL}})
            == ":1234567890\r\n", ":1234567890");
    // LLONG_MAX
    CHECK(ser(RespValue{RespInteger{9223372036854775807LL}})
            == ":9223372036854775807\r\n", "LLONG_MAX");
    // LLONG_MIN, written as -MAX - 1 to dodge the literal-overflow warning
    CHECK(ser(RespValue{RespInteger{-9223372036854775807LL - 1}})
            == ":-9223372036854775808\r\n", "LLONG_MIN");
}

void test_bulk_string_serialize() {
    CHECK(ser(RespValue{RespBulkString{"hello"}}) == "$5\r\nhello\r\n",
            "hello");

    // Empty bulk -- distinct from null bulk; off-by-one canary on the byte
    // count.
    CHECK(ser(RespValue{RespBulkString{""}}) == "$0\r\n\r\n",
            "empty bulk");

    // Embedded CRLF inside a bulk string is legal because the length prefix
    // makes the payload binary-safe.
    CHECK(ser(RespValue{RespBulkString{"a\r\nb"}}) == "$4\r\na\r\nb\r\n",
            "bulk with embedded CRLF");

    // Arbitrary binary content including null bytes.
    std::string binary;
    binary.push_back('\0');
    binary.push_back('\x01');
    binary.push_back('\x02');
    binary.push_back('\x03');
    binary.push_back('\x04');

    std::string expected = "$5\r\n";
    expected += binary;
    expected += "\r\n";

    CHECK(ser(RespValue{RespBulkString{binary}}) == expected,
            "bulk with binary content");
}

void test_null_bulk_string_serialize() {
    CHECK(ser(RespValue{RespNullBulkString{}}) == "$-1\r\n",
            "null bulk string");
}

void test_array_serialize() {
    // Empty array.
    CHECK(ser(RespValue{RespArray{}}) == "*0\r\n", "empty array");

    // Heterogeneous: bulk string then integer.
    {
        std::vector<RespValue> elems;
        elems.push_back(RespValue{RespBulkString{"foo"}});
        elems.push_back(RespValue{RespInteger{42}});
        CHECK(ser(RespValue{RespArray{elems}})
                == "*2\r\n$3\r\nfoo\r\n:42\r\n",
                "two-element heterogeneous array");
    }

    // Array containing a null bulk string.
    {
        std::vector<RespValue> elems;
        elems.push_back(RespValue{RespNullBulkString{}});
        elems.push_back(RespValue{RespBulkString{"x"}});
        CHECK(ser(RespValue{RespArray{elems}})
                == "*2\r\n$-1\r\n$1\r\nx\r\n",
                "array with null bulk");
    }

    // Nested array: array containing an array containing a simple string.
    {
        std::vector<RespValue> inner;
        inner.push_back(RespValue{RespSimpleString{"OK"}});
        std::vector<RespValue> outer;
        outer.push_back(RespValue{RespArray{inner}});
        CHECK(ser(RespValue{RespArray{outer}})
                == "*1\r\n*1\r\n+OK\r\n",
                "nested array");
    }
}

// ---- Round-trip tests -------------------------------------------------------

void test_round_trip_basics() {
    check_round_trip(RespValue{RespSimpleString{"OK"}},        "simple string");
    check_round_trip(RespValue{RespSimpleString{""}},          "empty simple string");
    check_round_trip(RespValue{RespSimpleError{"ERR bad"}},    "error");
    check_round_trip(RespValue{RespInteger{0}},                "int 0");
    check_round_trip(RespValue{RespInteger{42}},               "int 42");
    check_round_trip(RespValue{RespInteger{-1}},               "int -1");
    check_round_trip(RespValue{RespInteger{9223372036854775807LL}},     "int LLONG_MAX");
    check_round_trip(RespValue{RespInteger{-9223372036854775807LL - 1}}, "int LLONG_MIN");
    check_round_trip(RespValue{RespBulkString{"hello"}},       "bulk");
    check_round_trip(RespValue{RespBulkString{""}},            "empty bulk");
    check_round_trip(RespValue{RespBulkString{"a\r\nb"}},      "bulk with CRLF");
    check_round_trip(RespValue{RespNullBulkString{}},          "null bulk");
}

void test_round_trip_binary_bulk() {
    std::string binary;
    binary.push_back('\0');
    binary.push_back('\xff');
    binary.push_back('\x7f');
    binary.push_back('\x80');
    check_round_trip(RespValue{RespBulkString{binary}}, "binary bulk content");
}

void test_round_trip_arrays() {
    // Empty array.
    check_round_trip(RespValue{RespArray{}}, "empty array");

    // Heterogeneous array including null bulk.
    {
        std::vector<RespValue> elems;
        elems.push_back(RespValue{RespBulkString{"foo"}});
        elems.push_back(RespValue{RespInteger{42}});
        elems.push_back(RespValue{RespNullBulkString{}});
        check_round_trip(RespValue{RespArray{elems}}, "heterogeneous array");
    }

    // Nested: array of array of simple string, plus a bulk sibling.
    {
        std::vector<RespValue> inner;
        inner.push_back(RespValue{RespSimpleString{"OK"}});
        std::vector<RespValue> outer;
        outer.push_back(RespValue{RespArray{inner}});
        outer.push_back(RespValue{RespBulkString{"end"}});
        check_round_trip(RespValue{RespArray{outer}}, "nested array");
    }
}

int main() {
    test_simple_string_serialize();
    test_simple_error_serialize();
    test_integer_serialize();
    test_bulk_string_serialize();
    test_null_bulk_string_serialize();
    test_array_serialize();

    test_round_trip_basics();
    test_round_trip_binary_bulk();
    test_round_trip_arrays();

    std::cout << "All tests passed\n";
    return 0;
}
