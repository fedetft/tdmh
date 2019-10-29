
cmake_minimum_required(VERSION 3.1)

set (CMAKE_CXX_STANDARD 11)

add_definitions(-DUNITTEST)

include_directories(../../../simulator/WandstemMac/src)
include_directories(../../../simulator/WandstemMac/src/network_module)

set(SRCS
uplink_test.cpp
stubs.cpp
../../../simulator/WandstemMac/src/network_module/uplink_phase/topology/neighbor_table.cpp
../../../simulator/WandstemMac/src/network_module/uplink_phase/topology/network_graph.cpp
../../../simulator/WandstemMac/src/network_module/uplink_phase/topology/network_topology.cpp
../../../simulator/WandstemMac/src/network_module/uplink_phase/topology/topology_element.cpp
../../../simulator/WandstemMac/src/network_module/network_configuration.cpp
../../../simulator/WandstemMac/src/network_module/util/debug_settings.cpp
../../../simulator/WandstemMac/src/network_module/util/runtime_bitset.cpp
../../../simulator/WandstemMac/src/network_module/util/packet.cpp
)
add_executable(uplink_test ${SRCS})

# find_package(Threads REQUIRED)
# target_link_libraries(uplink_test ${CMAKE_THREAD_LIBS_INIT})
