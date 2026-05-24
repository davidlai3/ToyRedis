#ifndef DEBUG_H
#define DEBUG_H

#include <iostream>

#ifdef DEBUG
#define debug(x) do { std::cerr << x << std::endl; } while(0)
#else
#define debug(x) do {} while(0)
#endif

#endif
