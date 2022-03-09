
#pragma once

#include <miosix.h>
#include <network_module/tdmh.h>
#include "network_module/dynamic_tdmh.h"
#include "network_module/master_tdmh.h"
#include "network_module/downlink_phase/timesync/networktime.h"
#include "network_module/stream/stream_parameters.h"

using namespace std;
using namespace mxnet;
using namespace miosix;

// NOTE: the server parameters represent the maximum values accepted
// by the server, the actual stream parameters are negotiated with
// the value required by the client.
// You don't need to change the server parameters, but just the
// ones of the client
StreamParameters serverParams(Redundancy::TRIPLE_SPATIAL,
                                Period::P10,
                                1,     // payload size
                                Direction::TX);
StreamParameters clientParams(Redundancy::TRIPLE_SPATIAL,
                            Period::P10,
                            1,     // payload size
                            Direction::TX);

const unsigned char PORT = 1;

class Arg
{
public:
    Arg(unsigned char id, unsigned char hop=false) : id(id), hop(hop) {}
    const unsigned char id, hop;
};

/**
 * Struct containing pointer to an object and stream.
 * Object pointer used to call a specific function on it,
 * e.g. after creating a thread (for active objects)
 */
class StreamThreadPar
{
public:
    StreamThreadPar(void* obj, int stream) : stream(stream), obj(obj) {};
    int stream;
    void* obj;
};

#ifndef SMALL_DATA

struct Data
{
    Data() {}
    Data(int id, unsigned int counter, int value) : id(id), time(miosix::getTime()), 
                   netTime(NetworkTime::now().get()),
                   minHeap(MemoryProfiling::getAbsoluteFreeHeap()),
                   heap(MemoryProfiling::getCurrentFreeHeap()),
                   counter(counter), value(value) {}
                   
    unsigned char getId()          const { return id; }
    long long     getTime()        const { return time; }
    long long     getNetworkTime() const { return netTime; }
    int           getMinHeap()     const { return minHeap; }
    int           getHeap()        const { return heap; }
    unsigned int  getCounter()     const { return counter; }
    int           getValue()       const { return value; }
    
    unsigned char id;
    long long time;
    long long netTime;
    int minHeap;
    int heap;
    unsigned int counter;
    int value;
}__attribute__((packed));

#else //SMALL_DATA

struct Data
{
    Data() {}
    Data(int id, unsigned int counter): id(id),
        minHeap(MemoryProfiling::getAbsoluteFreeHeap()), counter(counter) {}
    
    unsigned char getId()          const { return id; }
    long long     getTime()        const { return -1; }
    long long     getNetworkTime() const { return -1; }
    int           getMinHeap()     const { return minHeap; }
    int           getHeap()        const { return -1; }
    unsigned int  getCounter()     const { return counter; }
    
    unsigned char id;
    int minHeap;
    unsigned int counter;
}__attribute__((packed));

#endif //SMALL_DATA


void printStatus(StreamStatus status) {
    switch(status){
    case StreamStatus::CONNECTING:
        printf("CONNECTING");
        break;
    case StreamStatus::CONNECT_FAILED:
        printf("CONNECT_FAILED");
        break;
    case StreamStatus::ACCEPT_WAIT:
        printf("ACCEPT_WAIT");
        break;
    case StreamStatus::ESTABLISHED:
        printf("ESTABLISHED");
        break;
    case StreamStatus::LISTEN_WAIT:
        printf("LISTEN_WAIT");
        break;
    case StreamStatus::LISTEN_FAILED:
        printf("LISTEN_FAILED");
        break;
    case StreamStatus::LISTEN:
        printf("LISTEN");
        break;
    case StreamStatus::REMOTELY_CLOSED:
        printf("REMOTELY_CLOSED");
        break;
    case StreamStatus::REOPENED:
        printf("REOPENED");
        break;
    case StreamStatus::CLOSE_WAIT:
        printf("CLOSE_WAIT");
        break;
    case StreamStatus::UNINITIALIZED:
        printf("UNINITIALIZED");
        break;
    default:
        printf("INVALID!");
    }
    printf("\n");
}