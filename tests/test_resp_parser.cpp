#include <cassert>
#include <string>
#include <vector>

#include "../include/RespParser.h"

void test_simple_string_basic() {
    std::string test_msg = "+hello\r\n";
    std::vector<char> buff(test_msg.begin(), test_msg.end());
    RespValue resp;

    RespParserCode ret = parse_one(buff, resp);

    assert(ret == RespParserCode::COMPLETE);
    assert(std::get<RespSimpleString>(resp.value).msg == test_msg);
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
    test_simple_error();
    test_simple_integer();
    test_bulk_string();
    test_null_bulk_string();
    test_array();

    return 0;
}
