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

using RespNullBulkString = std::monostate;

struct RespValue {
    std::variant<
        RespNullBulkString,
        RespSimpleString,
        RespSimpleError,
        RespInteger,
        RespBulkString,
        RespArray
    > value;
};

#endif
