
cmake_minimum_required(VERSION 3.1)

set (CMAKE_CXX_STANDARD 11)

add_definitions(-DUNITTEST)

include_directories(../../../simulator/WandstemMac/src)
include_directories(../../../simulator/WandstemMac/src/network_module)

set(SRCS
scheduler_test.cpp
stubs.cpp
../../../simulator/WandstemMac/src/network_module/network_configuration.cpp
../../../simulator/WandstemMac/src/network_module/scheduler/schedule_computation.cpp
../../../simulator/WandstemMac/src/network_module/scheduler/schedule_element.cpp
../../../simulator/WandstemMac/src/network_module/uplink_phase/topology/network_graph.cpp
../../../simulator/WandstemMac/src/network_module/uplink_phase/topology/network_topology.cpp
../../../simulator/WandstemMac/src/network_module/uplink_phase/topology/topology_element.cpp
../../../simulator/WandstemMac/src/network_module/stream/stream_collection.cpp
../../../simulator/WandstemMac/src/network_module/stream/stream_management_element.cpp
../../../simulator/WandstemMac/src/network_module/util/debug_settings.cpp
../../../simulator/WandstemMac/src/network_module/util/runtime_bitset.cpp
../../../simulator/WandstemMac/src/network_module/util/packet.cpp
)
add_executable(scheduler_test ${SRCS})

find_package(Threads REQUIRED)
target_link_libraries(scheduler_test ${CMAKE_THREAD_LIBS_INIT})
