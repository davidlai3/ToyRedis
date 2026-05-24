#include <cassert>
#include <iostream>
#include <string>
#include <vector>

#include "../include/RespParser.h"

#define CHECK(cond, msg) \
    do { \
        if (!(cond)) { \
            std::cerr << "CHECK failed at " << __FILE__ << ":" << __LINE__ \
            << ": " << msg << "\n"; \
            std::abort(); \
        } \
    } while (0)


void test_simple_string_basic() {
    std::string test_msg = "+hello\r\n";
    std::vector<char> buff(test_msg.begin(), test_msg.end());

    RespValue resp;
    RespParserCode ret = parse_one(buff, resp);

    CHECK(ret == RespParserCode::COMPLETE, "Parser return code not complete!");

    std::string exp = "hello";
    std::string act = std::get<RespSimpleString>(resp.value).msg;

    CHECK(exp == act, "Expected: " << exp << ", Actual: " << act);
    CHECK(buff.empty(), "Buffer not cleared!");
}

void test_simple_string_streamed() {
    std::string test_msg = "+abcdefg\r\n";
    size_t n = test_msg.size();
    std::vector<char> buff;
    buff.reserve(n);

    RespValue resp;
    for (size_t i = 0; i < n; i++) {
        RespParserCode ret = parse_one(buff, resp);
        CHECK(ret == RespParserCode::INCOMPLETE, 
                "Parser return code not incomplete on char " << i);
        CHECK(buff.size() == i, "Buffer size mismatch on char " << i);
        buff.push_back(test_msg[i]);
    }
    RespParserCode ret = parse_one(buff, resp);

    CHECK(ret == RespParserCode::COMPLETE, "Parser return code not complete!");

    std::string exp = "abcdefg";
    std::string act = std::get<RespSimpleString>(resp.value).msg;

    CHECK(exp == act, "Expected: " << exp << ", Actual: " << act);
    CHECK(buff.empty(), "Buffer not cleared!");
}

void test_simple_error() {
    std::string test_msg = "-ERR something bad\r\n";
    std::vector<char> buff(test_msg.begin(), test_msg.end());

    RespValue resp;
    RespParserCode ret = parse_one(buff, resp);

    CHECK(ret == RespParserCode::COMPLETE, "Parser return code not complete!");

    std::string exp = "ERR something bad";
    std::string act = std::get<RespSimpleError>(resp.value).msg;

    CHECK(exp == act, "Expected: " << exp << ", Actual: " << act);
    CHECK(buff.empty(), "Buffer not cleared!");
}

void test_simple_integer() {
    struct Case {
        std::string wire;
        long long expected;
    };

    std::vector<Case> cases = {
        {":0\r\n", 0},
        {":42\r\n", 42},
        {":-1\r\n", -1},
        {":+5\r\n", 5},
        {":1234567890\r\n", 1234567890LL},
        {":-9223372036854775807\r\n", -9223372036854775807LL},
    };

    for (const auto& tc : cases) {
        std::vector<char> buff(tc.wire.begin(), tc.wire.end());
        RespValue resp;
        RespParserCode ret = parse_one(buff, resp);

        CHECK(ret == RespParserCode::COMPLETE,
                "Return code not complete for: " << tc.wire);

        long long act = std::get<RespInteger>(resp.value).val;
        CHECK(act == tc.expected,
                "For " << tc.wire << " -- Expected: " << tc.expected
                << ", Actual: " << act);
        CHECK(buff.empty(), "Buffer not cleared for: " << tc.wire);
    }
}

void test_bulk_string() {
    // Normal bulk string
    {
        std::string test_msg = "$5\r\nhello\r\n";
        std::vector<char> buff(test_msg.begin(), test_msg.end());
        RespValue resp;
        RespParserCode ret = parse_one(buff, resp);

        CHECK(ret == RespParserCode::COMPLETE, "basic: not COMPLETE");
        std::string exp = "hello";
        std::string act = std::get<RespBulkString>(resp.value).msg;
        CHECK(exp == act, "basic: Expected: " << exp << ", Actual: " << act);
        CHECK(buff.empty(), "basic: buffer not cleared");
    }

    // Empty bulk string -- the off-by-one canary
    {
        std::string test_msg = "$0\r\n\r\n";
        std::vector<char> buff(test_msg.begin(), test_msg.end());
        RespValue resp;
        RespParserCode ret = parse_one(buff, resp);

        CHECK(ret == RespParserCode::COMPLETE, "empty: not COMPLETE");
        std::string act = std::get<RespBulkString>(resp.value).msg;
        CHECK(act.empty(), "empty: expected empty string, got: " << act);
        CHECK(buff.empty(), "empty: buffer not cleared");
    }

    // Bulk string containing embedded CRLF -- length-prefixed parsing must
    // not stop at the first \r\n inside the content.
    {
        std::string test_msg = "$12\r\nhello\r\nworld\r\n";
        std::vector<char> buff(test_msg.begin(), test_msg.end());
        RespValue resp;
        RespParserCode ret = parse_one(buff, resp);

        CHECK(ret == RespParserCode::COMPLETE, "embedded CRLF: not COMPLETE");
        std::string exp = "hello\r\nworld";
        std::string act = std::get<RespBulkString>(resp.value).msg;
        CHECK(exp == act, "embedded CRLF: Expected: " << exp
                << ", Actual: " << act);
        CHECK(buff.empty(), "embedded CRLF: buffer not cleared");
    }

    // Bulk string with arbitrary binary content (including null bytes)
    {
        std::string test_msg;
        test_msg += "$5\r\n";
        test_msg.push_back('\0');
        test_msg.push_back('\x01');
        test_msg.push_back('\x02');
        test_msg.push_back('\x03');
        test_msg.push_back('\x04');
        test_msg += "\r\n";

        std::vector<char> buff(test_msg.begin(), test_msg.end());
        RespValue resp;
        RespParserCode ret = parse_one(buff, resp);

        CHECK(ret == RespParserCode::COMPLETE, "binary: not COMPLETE");
        const std::string& act = std::get<RespBulkString>(resp.value).msg;
        CHECK(act.size() == 5, "binary: expected size 5, got " << act.size());
        CHECK(act[0] == '\0' && act[1] == '\x01' && act[2] == '\x02'
                && act[3] == '\x03' && act[4] == '\x04',
                "binary: content bytes do not match");
        CHECK(buff.empty(), "binary: buffer not cleared");
    }
}

void test_null_bulk_string() {
    std::string test_msg = "$-1\r\n";
    std::vector<char> buff(test_msg.begin(), test_msg.end());

    RespValue resp;
    RespParserCode ret = parse_one(buff, resp);

    CHECK(ret == RespParserCode::COMPLETE, "Parser return code not complete!");
    CHECK(std::holds_alternative<RespNullBulkString>(resp.value),
            "Variant does not hold RespNullBulkString");
    CHECK(buff.empty(), "Buffer not cleared!");
}

void test_array() {
    // Empty array
    {
        std::string test_msg = "*0\r\n";
        std::vector<char> buff(test_msg.begin(), test_msg.end());
        RespValue resp;
        RespParserCode ret = parse_one(buff, resp);

        CHECK(ret == RespParserCode::COMPLETE, "empty: not COMPLETE");
        const auto& arr = std::get<RespArray>(resp.value);
        CHECK(arr.vals.empty(),
                "empty: expected 0 elements, got " << arr.vals.size());
        CHECK(buff.empty(), "empty: buffer not cleared");
    }

    // Heterogeneous array: bulk string + integer
    {
        std::string test_msg = "*2\r\n$3\r\nfoo\r\n:42\r\n";
        std::vector<char> buff(test_msg.begin(), test_msg.end());
        RespValue resp;
        RespParserCode ret = parse_one(buff, resp);

        CHECK(ret == RespParserCode::COMPLETE, "mixed: not COMPLETE");
        const auto& arr = std::get<RespArray>(resp.value);
        CHECK(arr.vals.size() == 2,
                "mixed: expected 2 elements, got " << arr.vals.size());

        const auto& first = std::get<RespBulkString>(arr.vals[0].value);
        CHECK(first.msg == "foo",
                "mixed: first elem expected foo, got " << first.msg);

        long long second = std::get<RespInteger>(arr.vals[1].value).val;
        CHECK(second == 42,
                "mixed: second elem expected 42, got " << second);

        CHECK(buff.empty(), "mixed: buffer not cleared");
    }

    // Nested array: array containing an array containing a simple string
    {
        std::string test_msg = "*1\r\n*1\r\n+OK\r\n";
        std::vector<char> buff(test_msg.begin(), test_msg.end());
        RespValue resp;
        RespParserCode ret = parse_one(buff, resp);

        CHECK(ret == RespParserCode::COMPLETE, "nested: not COMPLETE");
        const auto& outer = std::get<RespArray>(resp.value);
        CHECK(outer.vals.size() == 1,
                "nested: outer size expected 1, got " << outer.vals.size());

        const auto& inner = std::get<RespArray>(outer.vals[0].value);
        CHECK(inner.vals.size() == 1,
                "nested: inner size expected 1, got " << inner.vals.size());

        const auto& leaf = std::get<RespSimpleString>(inner.vals[0].value);
        CHECK(leaf.msg == "OK",
                "nested: leaf expected OK, got " << leaf.msg);

        CHECK(buff.empty(), "nested: buffer not cleared");
    }
}


int main() {
    test_simple_string_basic();
    test_simple_string_streamed();
    test_simple_error();
    test_simple_integer();
    test_bulk_string();
    test_null_bulk_string();
    test_array();

    std::cout << "All tests passed\n";

    return 0;
}
