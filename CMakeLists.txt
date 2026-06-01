cmake_minimum_required(VERSION 3.16)

project(MyProject LANGUAGES CXX)

add_executable(ToyRedisServer
    src/CommandDispatcher.cpp
    src/Database.cpp
    src/RespParser.cpp
    src/RespSerializer.cpp
    src/RedisServer.cpp
    src/main_server.cpp
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

add_executable(ToyRedisDatabaseTests
    tests/test_database.cpp
    src/Database.cpp
)

add_executable(ToyRedisCommandDispatcherTests
    tests/test_command_dispatcher.cpp
    src/CommandDispatcher.cpp
    src/Database.cpp
    src/RespSerializer.cpp
)

target_compile_features(ToyRedisServer PRIVATE cxx_std_20)
target_compile_features(ToyRedisClient PRIVATE cxx_std_20)
target_compile_features(ToyRedisTests PRIVATE cxx_std_20)
target_compile_features(ToyRedisSerializerTests PRIVATE cxx_std_20)
target_compile_features(ToyRedisDatabaseTests PRIVATE cxx_std_20)
target_compile_features(ToyRedisCommandDispatcherTests PRIVATE cxx_std_20)
