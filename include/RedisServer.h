#ifndef REDISSERVER_H
#define REDISSERVER_H

#include <unordered_map>

#include "ClientConnection.hpp"
#include "Database.h"

class RedisServer {
public:
    void start_server();
private:
    static const short PORT = 6379;

    std::unordered_map<int, ClientConnection> connections;
    Database db;

    int set_nonblocking(int fd);
    void close_client(int fd, int epfd);
    void add_connection(int fd, int epfd);
    void receive_command(int fd, int epfd, char* buf);
};

#endif
