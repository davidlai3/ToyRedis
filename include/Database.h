#ifndef DATABASE_H
#define DATABASE_H

#include <unordered_set>
#include <unordered_map>
#include <variant>
#include <vector>
#include <set>
#include <string>

#include "RespValue.hpp"

struct ListValue { std::vector<std::string> data; };
struct HashValue { std::unordered_map<std::string, std::string> data; };
struct SetValue {std::unordered_set<std::string> data; };
struct SortedSetValue {std::set<std::string> data; };

using Value = std::variant<
    std::string,
    ListValue,
    HashValue,
    SetValue,
    SortedSetValue
>;

class Database {
public:
    std::unordered_map<std::string, Value> db;
};

#endif
