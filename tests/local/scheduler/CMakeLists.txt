
cmake_minimum_required(VERSION 3.1)

set (CMAKE_CXX_STANDARD 11)

add_definitions(-DUNITTEST)

include_directories(../../../simulator/WandstemMac/src)
include_directories(../../../simulator/WandstemMac/src/network_module)

set(SRCS scheduler_test.cpp)
add_executable(scheduler_test ${SRCS})

find_package(Threads REQUIRED)
target_link_libraries(scheduler_test ${CMAKE_THREAD_LIBS_INIT})
