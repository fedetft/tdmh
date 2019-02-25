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
#include "network_module/timesync/networktime.h"
#include "interfaces-impl/gpio_timer_corr.h"

using namespace std;
using namespace mxnet;
using namespace miosix;

const int maxNodes = 32;

MediumAccessController *tdmh = nullptr;

class Arg
{
public:
    Arg(unsigned char id, unsigned char hop=false) : id(id), hop(hop) {}
    const unsigned char id, hop;
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
            6,             //maxHops
            maxNodes,            //maxNodes
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
            6,             //maxHops
            maxNodes,            //maxNodes
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

struct Data
{
    Data() {}
    Data(int id) : id(id), time(miosix::getTime()) {}
    unsigned char id;
    long long time;
}__attribute__((packed));

void masterApplication() {
    /* Wait for TDMH to become ready */
    MACContext* ctx = tdmh->getMACContext();
    while(!ctx->isReady()) {
        Thread::sleep(1000);
    }
    /* Open a StreamServer to listen for incoming streams */
    mxnet::StreamServer server(*tdmh,      // Pointer to MediumAccessController
                               0,                 // Destination port
                               Period::P1,        // Period
                               1,                 // Payload size
                               Direction::TX,     // Direction
                               Redundancy::NONE); // Redundancy
    Stream r(*tdmh);
    server.accept(r);
    printf("[A] Accept returned! \n");
    while(2.0){
        Data data;
        int len = r.recv(&data, sizeof(data));
        if(len != sizeof(data))
            printf("[E] Received wrong size data");
        else
            printf("[A] Received id=%d time=%lld\n", data.id, data.time);
    }
}

void dynamicApplication() {
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
            mxnet::Stream s(*tdmh,            // Pointer to MediumAccessController
                            0,                 // Destination node
                            0,                 // Destination port
                            Period::P1,        // Period
                            1,                 // Payload size
                            Direction::TX,     // Direction
                            Redundancy::NONE); // Redundancy
            printf("[A] Stream constructor returned \n");
            while(4.0) {
                Data data(ctx->getNetworkId());
                s.send(&data, sizeof(data));
                printf("[A] Sent id=%d time=%lld\n", data.id, data.time); 
            }
        } catch(exception& e) {
            cerr<<"\nException thrown: "<<e.what()<<endl;
        }
    }
}

int main()
{
    //     auto t1 = Thread::create(masterNode, 2048, PRIORITY_MAX-1, nullptr, Thread::JOINABLE);
       auto t1 = Thread::create(dynamicNode, 2048, PRIORITY_MAX-1, new Arg(1,1), Thread::JOINABLE);
//     auto t1 = Thread::create(dynamicNode, 2048, PRIORITY_MAX-1, new Arg(2,3), Thread::JOINABLE);
//     auto t1 = Thread::create(dynamicNode, 2048, PRIORITY_MAX-1, new Arg(3,1), Thread::JOINABLE);
//     auto t1 = Thread::create(dynamicNode, 2048, PRIORITY_MAX-1, new Arg(4,2), Thread::JOINABLE);
//     auto t1 = Thread::create(dynamicNode, 2048, PRIORITY_MAX-1, new Arg(5,1), Thread::JOINABLE);
//     auto t1 = Thread::create(dynamicNode, 2048, PRIORITY_MAX-1, new Arg(6,3), Thread::JOINABLE);
//     auto t1 = Thread::create(dynamicNode, 2048, PRIORITY_MAX-1, new Arg(7,1), Thread::JOINABLE);
//     auto t1 = Thread::create(dynamicNode, 2048, PRIORITY_MAX-1, new Arg(8,2), Thread::JOINABLE);
    
    Thread::create(blinkThread,STACK_MIN,PRIORITY_MAX-1);

    while(tdmh==nullptr) ;
    Thread::sleep(5000);
    MACContext* ctx = tdmh->getMACContext();
    // Master node code
    if(ctx->getNetworkId() == 0)
        masterApplication();
    else
        dynamicApplication();


//     t1->join();
    
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
