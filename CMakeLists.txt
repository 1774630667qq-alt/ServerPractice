cmake_minimum_required(VERSION 3.10)
project(ServerPractice)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED True)

include_directories(include)

add_executable(main src/main.cpp)
add_executable(EchoSever EchoSever.cpp)
add_executable(EchoSeverByThreadPool EchoSeverByThreadPool.cpp)