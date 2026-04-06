#ifndef CLIENTCONNECTION_H
#define CLIENTCONNECTION_H

#include <vector>

class ClientConnection {
public:
    ClientConnection(int _fd) :
        fd(_fd), buff{} {}

    int fd;
    std::vector<char> buff;
};

#endif
