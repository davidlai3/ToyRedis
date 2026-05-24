#include "../include/RedisServer.h"

int main() {
    RedisServer server{};
    server.start_server();
}
