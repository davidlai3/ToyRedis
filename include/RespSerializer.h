#ifndef RESPSERIALIZER_H
#define RESPSERIALIZER_H

#include <vector>

#include "RespValue.hpp"

std::vector<char> serialize(const RespValue& resp);

#endif

