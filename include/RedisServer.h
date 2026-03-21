#ifndef REDISSERVER_H
#define REDISSERVER_H

class RedisServer {
public:
    static void start_server();
private:
    static const short PORT = 6379;

    static int set_nonblocking(int fd);
    static void close_client(int fd, int epfd);
    static void add_connection(int fd, int epfd);
    static void receive_command(int fd, int epfd, char* buf);
};

#endif
