#ifndef REDISSERVER_H
#define REDISSERVER_H

class RedisServer {
public:
    static void start_server();
private:
    static const short PORT = 6379;
    static void receive_message(int connfd);
};

#endif
