cmake_minimum_required(VERSION 2.8.4)
project(sergate)

set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wall -O2")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11 -Wall -O2")
set(Boost_USE_MULTITHREADED ON)
set(THREADS_PREFER_PTHREAD_FLAG ON)

find_package(Threads REQUIRED)
find_package(OpenSSL REQUIRED)
find_package(Boost COMPONENTS system REQUIRED)
find_package(CURL)
include_directories(/usr/local/include include ${OPENSSL_INCLUDE_DIR} ${Boost_INCLUDE_DIR})

if (CURL_FOUND)
    include_directories(${CURL_INCLUDE_DIRS})
    add_definitions(-DHAVE_CURL)
endif()

set(SRC_LIST
    src/main.cpp
    src/argparse.c
    src/json.c
    src/message.cpp
    src/netban.cpp
    src/network.cpp
    src/network_client.cpp
    src/server.cpp
    src/system.c)

add_subdirectory(tgbot-cpp)
add_executable(sergate ${SRC_LIST})

target_link_libraries(sergate TgBot ${CMAKE_THREAD_LIBS_INIT} ${OPENSSL_LIBRARIES} ${Boost_LIBRARIES} ${CURL_LIBRARIES} m)
