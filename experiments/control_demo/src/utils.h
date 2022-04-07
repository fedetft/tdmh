
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

const unsigned char actuatorNodeID   = 9;  // controller node destination
const unsigned char controllerNodeID = 0;  // sensor node destination
const unsigned char sensorNodeID     = 12;

const unsigned char PORT1 = 1;

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

struct Data
{
    Data() {}

    // For sensor data
    Data(int id, unsigned int counter, int value) :
                    id(id), time(miosix::getTime()), netTime(NetworkTime::now().get()),
                    sampleTime(NetworkTime::now().get()), counter(counter), latency(0), value(value) {}

    // For control action and sample time forwarding
    Data(int id, unsigned int counter, long long sampleTime, int value) :
                    id(id), time(miosix::getTime()), netTime(NetworkTime::now().get()),
                    sampleTime(sampleTime), counter(counter), latency(0), value(value) {}

    // To send back to the master the latency among sensor and actuator
    Data(int id, unsigned int counter, long long sampleTime, long long latency) :
                    id(id), time(miosix::getTime()), netTime(NetworkTime::now().get()),
                    sampleTime(sampleTime), counter(counter), latency(latency), value(0) {}
                   
    unsigned char getId()          const { return id; }
    long long     getTime()        const { return time; }
    long long     getNetworkTime() const { return netTime; }
    long long     getSampleTime()  const { return sampleTime; }
    unsigned int  getCounter()     const { return counter; }
    long long     getLatency()     const { return latency; }
    int           getValue()       const { return value; }
    
    unsigned char id;
    long long time;
    long long netTime;
    long long sampleTime;
    unsigned int counter;
    long long latency;
    int value; // either sensor data or control action
}__attribute__((packed));

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