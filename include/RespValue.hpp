#ifndef RESPVALUE_H
#define RESPVALUE_H

#include <string>
#include <variant>
#include <vector>

// Forward declare because of RespArray
struct RespValue;

using RespNullBulkString = std::monostate;

struct RespSimpleString {
    std::string msg;
};
struct RespSimpleError {
    std::string msg;
};
struct RespInteger {
    long long val;
};
struct RespBulkString {
    std::string msg;
};
struct RespArray {
    std::vector<RespValue> vals;
};

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
