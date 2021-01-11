cmake_minimum_required(VERSION 3.17)
project(cp)

set(CMAKE_CXX_STANDARD 20)
set(LOCAL_INCLUDE_DIR /usr/local/include)
set(SQLITE_INCLUDE_DIR /usr/local/Cellar/sqlite/3.34.0/include)
set(SQLITE_PATH /usr/local/Cellar/sqlite/3.34.0/lib)

find_library(SODIUM     NAMES libsodium.a)
find_library(ZMQ        NAMES libzmq.a)
find_library(ZMQPP      NAMES libzmqpp.a)
find_library(SQLITE     NAMES libsqlite3.a PATHS ${SQLITE_PATH})

add_library(database    STATIC lib/database.hpp lib/src/database.cpp lib/auth.hpp)
add_library(networking  STATIC lib/networking.hpp lib/src/networking.cpp)
add_library(messaging   STATIC lib/messaging.hpp lib/src/messaging.cpp)

add_executable(server server.cpp lib/auth.hpp)
add_executable(client client.cpp lib/auth.hpp)

target_include_directories(database     PUBLIC ${LOCAL_INCLUDE_DIR} ${SQLITE_INCLUDE_DIR})
target_include_directories(messaging    PUBLIC ${LOCAL_INCLUDE_DIR})
target_include_directories(server       PUBLIC ${LOCAL_INCLUDE_DIR})
target_include_directories(client       PUBLIC ${LOCAL_INCLUDE_DIR})

target_link_libraries(database  PUBLIC ${SQLITE})
target_link_libraries(server    PUBLIC pthread networking messaging database ${SODIUM} ${ZMQ} ${ZMQPP})
target_link_libraries(client    PUBLIC pthread networking messaging ${SODIUM} ${ZMQ} ${ZMQPP})
