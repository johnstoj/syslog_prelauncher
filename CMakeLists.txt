cmake_minimum_required(VERSION 3.5)
project(syslog_prelauncher)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11")

set(SOURCE_FILES main.cpp)
add_executable(syslog_prelauncher ${SOURCE_FILES})