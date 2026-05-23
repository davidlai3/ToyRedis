#ifndef RESPPARSER_H
#define RESPPARSER_H

#include <optional>
#include <vector>

#include "RespValue.hpp"

enum class RespParserCode {
    COMPLETE,
    INCOMPLETE,
    ERROR
};

RespParserCode parse_one(std::vector<char>& buff, RespValue& ret);

#endif

