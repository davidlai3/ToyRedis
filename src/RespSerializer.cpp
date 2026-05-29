#include "../include/RespSerializer.h"

namespace {
    template <class... Ts> struct overloaded : Ts... { using Ts::operator()...; };
    template <class... Ts> overloaded(Ts...) -> overloaded<Ts...>;
}

std::vector<char> serialize(const RespValue& resp) {

    std::vector<char> ret;
    constexpr std::string_view CRLF = "\r\n";

    auto append = [&](std::string_view sv) {
        ret.insert(ret.end(), sv.begin(), sv.end());
    };

    std::visit(overloaded {
        [&](const RespNullBulkString&) {
            append("$-1");
            append(CRLF);
        },
        [&](const RespSimpleString& str) {
            append("+");
            append(str.msg);
            append(CRLF);
        },
        [&](const RespSimpleError& err) {
            append("-");
            append(err.msg);
            append(CRLF);
        },
        [&](const RespInteger& num) {
            append(":");
            append(std::to_string(num.val));
            append(CRLF);
        },
        [&](const RespBulkString& str) {
            std::string_view msg = str.msg;
            std::string len = std::to_string(msg.size());
            append("$");
            append(len);
            append(CRLF);
            append(msg);
            append(CRLF);
        },
        [&](const RespArray& arr) { // RespArray
            const std::vector<RespValue>& vals = arr.vals;
            std::string len = std::to_string(vals.size());
            append("*");
            append(len);
            append(CRLF);

            for (const RespValue& val : vals) {
                std::vector<char> ret_val = serialize(val);
                ret.insert(ret.end(), ret_val.begin(), ret_val.end());
            }
        }
    }, resp.value);

    return ret;
}
