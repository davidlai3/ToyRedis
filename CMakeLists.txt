cmake_minimum_required(VERSION 3.16)

project(MyProject LANGUAGES CXX)

add_executable(ToyRedisServer
    src/RedisServer.cpp
    src/main_server.cpp
    src/RespParser.cpp
)

add_executable(ToyRedisClient
    src/main_client.cpp
)

add_executable(ToyRedisTests
    tests/test_resp_parser.cpp
    src/RespParser.cpp
)

add_executable(ToyRedisSerializerTests
    tests/test_resp_serializer.cpp
    src/RespSerializer.cpp
    src/RespParser.cpp
)

target_compile_features(ToyRedisServer PRIVATE cxx_std_20)
target_compile_features(ToyRedisClient PRIVATE cxx_std_20)
target_compile_features(ToyRedisTests PRIVATE cxx_std_20)
target_compile_features(ToyRedisSerializerTests PRIVATE cxx_std_20)
