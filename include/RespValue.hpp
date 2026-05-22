#ifndef RESPVALUE_H
#define RESPVALUE_H

#include <string>
#include <variant>
#include <vector>

// Forward declare because of RespArray
struct RespValue;

struct RespSimpleString {
    std::string msg;
};
struct RespBulkString {
    std::string msg;
};
struct RespInteger {
    long long val;
};
struct RespArray {
    std::vector<RespValue> vals;
};
struct RespSimpleError {
    std::string msg;
};
using RespNull = std::monostate;

struct RespValue {
    std::variant<
        RespNull,
        RespSimpleString,
        RespBulkString,
        RespInteger,
        RespArray,
        RespSimpleError
    > values;
};

#endif
