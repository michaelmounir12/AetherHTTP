cmake_minimum_required(VERSION 3.20)

project(multithreaded_http_server
  VERSION 0.1.0
  LANGUAGES CXX
)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

find_package(Threads REQUIRED)

add_executable(server
  src/main.cpp
  src/ThreadPool.cpp
  src/HttpRequestParser.cpp
  src/Logger.cpp
)

target_include_directories(server PRIVATE include)
target_link_libraries(server PRIVATE Threads::Threads)

if (CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
  target_compile_options(server PRIVATE -Wall -Wextra -Wpedantic)
endif()

