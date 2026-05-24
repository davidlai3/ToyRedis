#ifndef COMMANDDISPATCHER_H
#define COMMANDDISPATCHER_H

#include "Database.h"
#include "RespValue.hpp"

RespValue dispatch(const RespValue& cmd, Database& db);

#endif
