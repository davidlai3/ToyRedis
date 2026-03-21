cmake_minimum_required(VERSION 3.16)

project(MyProject LANGUAGES CXX)

add_executable(ToyRedisServer
    src/RedisServer.cpp
    src/main_server.cpp
)

add_executable(ToyRedisClient
    src/main_client.cpp
)

target_compile_features(ToyRedisServer PRIVATE cxx_std_20)
target_compile_features(ToyRedisClient PRIVATE cxx_std_20)
