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

}

void test_simple_integer() {

}

void test_bulk_string() {

}

void test_null_bulk_string() {

}

void test_array() {

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
