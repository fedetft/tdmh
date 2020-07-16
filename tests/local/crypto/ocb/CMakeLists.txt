

cmake_minimum_required(VERSION 3.1)

set (CMAKE_CXX_STANDARD 11)
set (CMAKE_CXX_FLAGS "-g")

add_definitions(-DUNITTEST)

include_directories(../../../../simulator/WandstemMac/src)
include_directories(../../../../simulator/WandstemMac/src/network_module)

set(SRCS
aes_ocb_test.cpp
../../../../simulator/WandstemMac/src/network_module/crypto/aes_ocb.cpp
../../../../simulator/WandstemMac/src/network_module/crypto/aes_ocb.h
../../../../simulator/WandstemMac/src/network_module/crypto/initialization_vector.cpp
../../../../simulator/WandstemMac/src/network_module/crypto/aes.cpp
../../../../simulator/WandstemMac/src/network_module/util/aes_accelerator.cpp
../../../../simulator/WandstemMac/src/network_module/util/tiny_aes_c.cpp

)

add_executable(aes_ocb_test ${SRCS})

