#include <string>
#include <string_view>

#include "../include/RespParser.h"

constexpr size_t TYPE_TAG_LEN = 1;
constexpr std::string_view CRLF = "\r\n";
RespParserCode parse_one_inner(std::vector<char>& buff, RespValue& ret, size_t& cursor);

RespParserCode parse_simple_string(std::vector<char>& buff, RespValue& ret, size_t& cursor) {
    std::string_view sv{buff.data()+cursor+1, buff.size()-cursor-1};

    size_t pos = sv.find(CRLF);

    if (pos == std::string::npos) {
        return RespParserCode::INCOMPLETE;
    }

    std::string msg{sv.substr(0, pos)};
    if (msg.find("\r") != std::string::npos) {
        return RespParserCode::ERROR;
    }
    else if (msg.find("\n") != std::string::npos) {
        return RespParserCode::ERROR;
    }

    ret.value = RespSimpleString{msg};
    cursor += TYPE_TAG_LEN + pos + CRLF.size();

    return RespParserCode::COMPLETE;
}

RespParserCode parse_null_bulk_string(std::vector<char>& buff, RespValue& ret, size_t pos, size_t& cursor) {
    ret.value = RespNullBulkString{};
    cursor += TYPE_TAG_LEN + pos + CRLF.size();
    return RespParserCode::COMPLETE;
}

RespParserCode parse_bulk_string(std::vector<char>& buff, RespValue& ret, size_t& cursor) {
    std::string_view sv{buff.data()+cursor+1, buff.size()-cursor-1};

    size_t pos = sv.find(CRLF);
    if (pos == std::string::npos) {
        return RespParserCode::INCOMPLETE;
    }
    if (pos == 0) {
        return RespParserCode::ERROR;
    }

    long long msg_len_raw;
    try {
        msg_len_raw = std::stoll(std::string{sv.substr(0, pos)});
    }
    catch (...) {
        return RespParserCode::ERROR;
    }

    if (msg_len_raw == -1) {
        return parse_null_bulk_string(buff, ret, pos, cursor);
    }
    else if (msg_len_raw < 0) {
        return RespParserCode::ERROR;
    }

    size_t msg_len = msg_len_raw;
    std::string_view msg_sv = sv.substr(pos+2, msg_len+2);

    if (msg_sv.size() < msg_len+2) {
        return RespParserCode::INCOMPLETE;
    }
    if (msg_sv.substr(msg_len, 2) != CRLF) {
        return RespParserCode::ERROR;
    }

    std::string msg{msg_sv.substr(0, msg_len)};
    ret.value = RespBulkString{msg};
    cursor += TYPE_TAG_LEN + pos + CRLF.size() + msg_len + CRLF.size();

    return RespParserCode::COMPLETE;
}

RespParserCode parse_integer(std::vector<char>& buff, RespValue& ret, size_t& cursor) {
    std::string_view sv{buff.data()+cursor+1, buff.size()-cursor-1};

    size_t pos = sv.find(CRLF);
    if (pos == std::string::npos) {
        return RespParserCode::INCOMPLETE;
    }
    if (pos == 0) {
        return RespParserCode::ERROR;
    }

    long long val;
    try {
        val = std::stoll(std::string{sv.substr(0, pos)});
    }
    catch (...) {
        return RespParserCode::ERROR;
    }

    ret.value = RespInteger{val};
    cursor += TYPE_TAG_LEN + pos + CRLF.size();

    return RespParserCode::COMPLETE;
}

RespParserCode parse_array(std::vector<char>& buff, RespValue& ret, size_t& cursor) {
    std::string_view sv{buff.data()+cursor+1, buff.size()-cursor-1};

    size_t pos = sv.find(CRLF);
    if (pos == std::string::npos) {
        return RespParserCode::INCOMPLETE;
    }
    if (pos == 0) {
        return RespParserCode::ERROR;
    }

    long long arr_len_raw;
    try {
        arr_len_raw = stoll(std::string{sv.substr(0, pos)});
    }
    catch (...) {
        return RespParserCode::ERROR;
    }

    if (arr_len_raw < 0) {
        return RespParserCode::ERROR;
    }
    size_t arr_len = arr_len_raw;

    cursor += TYPE_TAG_LEN + pos + CRLF.size();

    std::vector<RespValue> elems;
    elems.reserve(arr_len);

    for (size_t i = 0; i < arr_len; i++) {
        RespValue elem;
        RespParserCode code = parse_one_inner(buff, elem, cursor);

        if (code != RespParserCode::COMPLETE) {
            return code;
        }

        elems.push_back(std::move(elem));
    }

    ret.value = RespArray{std::move(elems)};

    return RespParserCode::COMPLETE;
}

RespParserCode parse_simple_error(std::vector<char>& buff, RespValue& ret, size_t& cursor) {
    std::string_view sv{buff.data()+cursor+1, buff.size()-cursor-1};

    size_t pos = sv.find(CRLF);
    if (pos == std::string::npos) {
        return RespParserCode::INCOMPLETE;
    }
    if (pos == 0) {
        return RespParserCode::ERROR;
    }

    std::string msg{sv.substr(0, pos)};
    if (msg.find("\r") != std::string::npos) {
        return RespParserCode::ERROR;
    }
    else if (msg.find("\n") != std::string::npos) {
        return RespParserCode::ERROR;
    }

    ret.value = RespSimpleError{msg};
    cursor += TYPE_TAG_LEN + pos + CRLF.size();

    return RespParserCode::COMPLETE;
}

RespParserCode parse_one_inner(std::vector<char>& buff, RespValue& ret, size_t& cursor) {
    if (cursor >= buff.size()) {
        return RespParserCode::INCOMPLETE;
    }

    switch (buff[cursor]) {
        case '+':
            return parse_simple_string(buff, ret, cursor);
        case '$':
            return parse_bulk_string(buff, ret, cursor);
        case ':':
            return parse_integer(buff, ret, cursor);
        case '*':
            return parse_array(buff, ret, cursor);
        case '-':
            return parse_simple_error(buff, ret, cursor);
        default:
            return RespParserCode::ERROR;
    }
}

RespParserCode parse_one(std::vector<char>& buff, RespValue& ret) {

    if (buff.empty()) {
        return RespParserCode::INCOMPLETE;
    }

    size_t cursor = 0; // invariant that cursor sits on start of command

    RespParserCode code = parse_one_inner(buff, ret, cursor);

    if (code == RespParserCode::COMPLETE) {
        buff.erase(buff.begin(), buff.begin()+cursor);
    }
    return code;
}
