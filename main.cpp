/***************************************************************************
 *   Copyright (C) 2017 by Polidori Paolo, Terraneo Federico               *
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 *   This program is distributed in the hope that it will be useful,       *
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of        *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
 *   GNU General Public License for more details.                          *
 *                                                                         *
 *   As a special exception, if other files instantiate templates or use   *
 *   macros or inline functions from this file, or you compile this file   *
 *   and link it with other works to produce a work based on this file,    *
 *   this file does not by itself cause the resulting work to be covered   *
 *   by the GNU General Public License. However the source code for this   *
 *   file must still be made available in accordance with the GNU General  *
 *   Public License. This exception does not invalidate any other reasons  *
 *   why a work based on this file might be covered by the GNU General     *
 *   Public License.                                                       *
 *                                                                         *
 *   You should have received a copy of the GNU General Public License     *
 *   along with this program; if not, see <http://www.gnu.org/licenses/>   *
 ***************************************************************************/

#include <cstdio>
#include <unistd.h>
#include <miosix.h>
#include <chrono>
#include "network_module/network_configuration.h"
#include "network_module/dynamic_tdmh.h"
#include "network_module/master_tdmh.h"
#include "network_module/downlink_phase/timesync/networktime.h"
#include "interfaces-impl/gpio_timer_corr.h"

using namespace std;
using namespace mxnet;
using namespace miosix;

const int maxNodes = 8;
const int maxHops = 6;

FastMutex m;
MediumAccessController *tdmh = nullptr;

/* Arguments passed to the TDMH thread */
class Arg
{
public:
    Arg(unsigned char id, unsigned char hop=false) : id(id), hop(hop) {}
    const unsigned char id, hop;
};

class StreamThreadPar
{
public:
    StreamThreadPar(int stream) : stream(stream) {};
    int stream;
};

inline int maxForwardedTopologiesFromMaxNumNodes(int maxNumNodes)
{
    /*
     * Dynamic Uplink allocation implemented
     * the parameter topologySMERatio configures the fraction of uplink packet
     * reserved for topology messages
     * UplinkMessagePkt                 { hop, assignee }   2
     * NeighborTable                    { bitmask }         maxNumNodes/8
     * number of forwarded topologies (NeighborMessage::serialize) 1
     * vector<ForwardedNeighborMessage> { nodeId, bitmask } maxForwardedTopologies*(1+maxNumNodes/8)
     */
    const float topologySMERatio = 0.5;
    int packetCapacity = (125 - 2 - maxNumNodes/8 - 1) / (1 + maxNumNodes/8);
    return std::min<int>(packetCapacity * topologySMERatio, maxNumNodes - 2);
}

void masterNode(void*)
{
    try {
        printf("Master node\n");
        const NetworkConfiguration config(
            maxHops,       //maxHops
            maxNodes,      //maxNodes
            0,             //networkId
            false,         //staticHop
            6,             //panId
            5,             //txPower
            2450,          //baseFrequency
            10000000000,   //clockSyncPeriod
            maxForwardedTopologiesFromMaxNumNodes(maxNodes), //maxForwardedTopologies
            1,             //numUplinkPackets
            100000000,     //tileDuration
            150000,        //maxAdmittedRcvWindow
            3,             //maxRoundsUnavailableBecomesDead
            -75,           //minNeighborRSSI
            3              //maxMissedTimesyncs
        );
        printf("Starting TDMH with maxForwardedTopologies=%d\n", maxForwardedTopologiesFromMaxNumNodes(maxNodes));
        MasterMediumAccessController controller(Transceiver::instance(), config);
        tdmh = &controller;
        controller.run();
    } catch(exception& e) {
        for(;;)
        {
            printf("exception %s\n",e.what());
            Thread::sleep(1000);
        }
    }
}

void dynamicNode(void* argv)
{
    try {
        auto arg = reinterpret_cast<Arg*>(argv);
        printf("Dynamic node %d",arg->id);
        if(arg->hop) printf(" forced hop %d",arg->hop);
        printf("\n");
        const NetworkConfiguration config(
            maxHops,       //maxHops
            maxNodes,      //maxNodes
            arg->id,       //networkId
            arg->hop,      //staticHop
            6,             //panId
            5,             //txPower
            2450,          //baseFrequency
            10000000000,   //clockSyncPeriod
            maxForwardedTopologiesFromMaxNumNodes(maxNodes), //maxForwardedTopologies
            1,             //numUplinkPackets
            100000000,     //tileDuration
            150000,        //maxAdmittedRcvWindow
            3,             //maxRoundsUnavailableBecomesDead
            -75,           //minNeighborRSSI
            3              //maxMissedTimesyncs
        );
        printf("Starting TDMH with maxForwardedTopologies=%d\n", maxForwardedTopologiesFromMaxNumNodes(maxNodes));
        DynamicMediumAccessController controller(Transceiver::instance(), config);
        {
            Lock<FastMutex> l(m);
            tdmh = &controller;
        }
        controller.run();
    } catch(exception& e) {
        for(;;)
        {
            printf("exception %s\n",e.what());
            Thread::sleep(1000);
        }
    }
}

void blinkThread(void *)
{
    for(;;)
    {
        redLed::high();
        Thread::sleep(10);
        redLed::low();
        Thread::sleep(990);
   }
}

void statThread(void *)
{
    for(;;)
    {
        printf("[H] Time=%lld MinHeap=%u Heap=%u\n", miosix::getTime(),
               miosix::MemoryProfiling::getAbsoluteFreeHeap(),
               miosix::MemoryProfiling::getCurrentFreeHeap());
        Thread::sleep(2000);
    }
}

struct Data
{
    Data() {}
    Data(int id, unsigned int counter) : id(id), time(miosix::getTime()),
                   minHeap(miosix::MemoryProfiling::getAbsoluteFreeHeap()),
                   heap(miosix::MemoryProfiling::getCurrentFreeHeap()),
                   counter(counter){}
    unsigned char id;
    long long time;
    unsigned int minHeap;
    unsigned int heap;
    unsigned int counter;
}__attribute__((packed));

void streamThread(void *arg)
{
    auto *s = reinterpret_cast<StreamThreadPar*>(arg);
    int stream = s->stream;
    StreamInfo info = getInfo(stream);
    StreamId id = info.getStreamId();
    printf("[A] Master node: Stream (%d,%d) accepted\n", id.src, id.dst);
    while(getInfo(stream).getStatus() == StreamStatus::ESTABLISHED) {
        Data data;
        int len = read(stream, &data, sizeof(data));
        if(len > 0) {
            if(len == sizeof(data))
                printf("[A] Received data from (%d,%d): ID=%d Time=%lld MinHeap=%u Heap=%u Counter=%u\n",
                       id.src, id.dst, data.id, data.time, data.minHeap, data.heap, data.counter);
            else
                printf("[E] Received wrong size data from Stream (%d,%d): %d\n",
                       id.src, id.dst, len);
        }
        else if(len == -1) {
            printf("[E] No data received from Stream (%d,%d): %d\n",
                   id.src, id.dst, len);
        }
    }
    delete s;
}

void openServer(unsigned char port, StreamParameters params) {
    /* Wait for TDMH to become ready */
    MACContext* ctx = tdmh->getMACContext();
    while(!ctx->isReady()) {
        Thread::sleep(1000);
    }
    printf("[A] Opening server on port %d\n", port);
    /* Open a Server to listen for incoming streams */
    int server = listen(port,              // Destination port
                        params);           // Server parameters
    if(server < 0) {                
        printf("[A] Server opening failed! error=%d\n", server);
        return;
    }
    while(getInfo(server).getStatus() == StreamStatus::LISTEN) {
        int stream = accept(server);
        Thread::create(streamThread, 2048, MAIN_PRIORITY, new StreamThreadPar(stream));
    }
}

void openStream(unsigned char dest, unsigned char port, StreamParameters params) {
    /* Wait for TDMH to become ready */
    MACContext* ctx = tdmh->getMACContext();
    while(!ctx->isReady()) {
        Thread::sleep(1000);
    }
    /* Delay the Stream opening so it gets opened after the StreamServer */
    Thread::sleep(1000);
    /* Open Stream from node */
    while(true){
        try{
            /* Open a Stream to another node */
            printf("[A] Opening stream to node %d\n", dest);
            int stream = connect(dest,          // Destination node
                                 port,             // Destination port
                                 params);       // Stream parameters
            if(stream < 0) {                
                printf("[A] Stream opening failed! error=%d\n", stream);
                continue;
            }
            printf("[A] Stream opened \n");
            unsigned int counter = 0;
            while(getInfo(stream).getStatus() == StreamStatus::ESTABLISHED) {
                Data data(ctx->getNetworkId(), counter);
                write(stream, &data, sizeof(data));
                printf("[A] Sent ID=%d Time=%lld MinHeap=%u Heap=%u Counter=%u\n",
                       data.id, data.time, data.minHeap, data.heap, data.counter);
                counter++;
            }
            printf("[A] Stream was closed, reopening it \n");
        } catch(exception& e) {
            cerr<<"\nException thrown: "<<e.what()<<endl;
        }
    }
}

unsigned long long getUniqueID() {
    return *reinterpret_cast<unsigned long long*>(0x0FE081F0);
}

void idle() {
    // Remove this sleep loop when enabling stream code
    for(;;) {            
        Thread::sleep(5000);
    }
}

int main()
{
    //Thread::create(blinkThread,STACK_MIN,MAIN_PRIORITY);

    unsigned long long nodeID = getUniqueID();
    printf("### TDMH Test code ###\nNodeID=%llx\n", nodeID);
    Thread* t1;
    /* Start TDMH thread mapping node unique ID to network ID */
    switch(nodeID) {
    case 0x243537035155c338:
        t1 = Thread::create(masterNode, 2048, PRIORITY_MAX-1, nullptr, Thread::JOINABLE);
        break;
    case 0x243537005155c356:
        t1 = Thread::create(dynamicNode, 2048, PRIORITY_MAX-1, new Arg(1), Thread::JOINABLE);
        break;
    case 0x243537005155c346:
        t1 = Thread::create(dynamicNode, 2048, PRIORITY_MAX-1, new Arg(2), Thread::JOINABLE);
        break;
    case 0x243537025155c346:
        t1 = Thread::create(dynamicNode, 2048, PRIORITY_MAX-1, new Arg(3), Thread::JOINABLE);
        //p.redundancy=Redundancy::TRIPLE_SPATIAL; auto t1 = Thread::create(dynamicNode, 2048, PRIORITY_MAX-1, new Arg(3), Thread::JOINABLE);
        break;
    default:        
        printf("ERROR: nodeID is not mapped to any node, halting!\n");
    }

    /* Wait for tdmh pointer to become valid */
    for(;;)
    {
        Lock<FastMutex> l(m);
        if(tdmh) break;
    }
    Thread::sleep(5000);

    MACContext* ctx = tdmh->getMACContext();
    unsigned char netID = ctx->getNetworkId();
    StreamParameters serverParams(Redundancy::TRIPLE_SPATIAL,
                                  Period::P1,
                                  1,     // payload size
                                  Direction::TX);
    StreamParameters clientParams(Redundancy::NONE,
                                  Period::P20,
                                  1,     // payload size
                                  Direction::TX);
    unsigned char port = 1;
    /* Perform actions depending on your network ID */
    switch(netID) {
    case 0:
        // Master node code
        Thread::create(statThread, 2048, MAIN_PRIORITY);
        openServer(port, serverParams);
        break;
    case 1:
        idle();
        //openStream(0, port, clientParams);
        break;
    case 2:
        idle();
        //openStream(0, port, clientParams);
        break;
    case 3:
        openStream(0, port, clientParams);
        break;
    default:
        idle();
    }


/*    auto& timestamp=GPIOtimerCorr::instance();
    for(;;)
    {
        auto period=NetworkTime::fromNetworkTime(10000000000);
        auto now=NetworkTime::fromNetworkTime(((NetworkTime::now().get()/period.get())+2)*period.get());
        printf("--- %lld %lld %lld\n",now.get(),now.toLocalTime(),period.get());
        for(auto offset=NetworkTime::fromLocalTime(0).get(); offset == NetworkTime::fromLocalTime(0).get(); now += period) {
            timestamp.absoluteWaitTrigger(timestamp.ns2tick(now.toLocalTime()));
            printf("[TIMESTAMP] %lld %lld\n",now.get(),now.toLocalTime());
            // MemoryProfiling::print();
        }
    }
*/  
}
