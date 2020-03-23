//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU Lesser General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
// 
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU Lesser General Public License for more details.
// 
// You should have received a copy of the GNU Lesser General Public License
// along with this program.  If not, see http://www.gnu.org/licenses/.
// 

#include "RootNode.h"

#include "network_module/master_tdmh.h"
#include "network_module/network_configuration.h"
#include <iostream>
#include <stdexcept>
#include <chrono>
#include <list>

Define_Module(RootNode);

using namespace std;
using namespace mxnet;

struct Data
{
    Data() {}
    Data(int id, unsigned int counter) : id(id), counter(counter){}
    unsigned char id;
    unsigned int counter;
}__attribute__((packed));

void RootNode::activity()
{
    using namespace miosix;
    print_dbg("Master node\n");
    bool useWeakTopologies=true;
    const NetworkConfiguration config(
            hops,            //maxHops
            nodes,           //maxNodes
            address,       //networkId
            false,         //staticHop
            6,             //panId
            5,             //txPower
            2450,          //baseFrequency
            10000000000,   //clockSyncPeriod
            guaranteedTopologies(nodes,useWeakTopologies), //guaranteedTopologies
            1,             //numUplinkPackets
            100000000,     //tileDuration
            150000,        //maxAdmittedRcvWindow
            3,             //maxRoundsUnavailableBecomesDead
            16,            //maxRoundsWeakLinkBecomesDead
            -75,           //minNeighborRSSI
            -90,           //minWeakNeighborRSSI
            3,             //maxMissedTimesyncs
            true,          //channelSpatialReuse
            useWeakTopologies //useWeakTopologies
    );
    MasterMediumAccessController controller(Transceiver::instance(), config);

    tdmh = &controller;
    auto *t = new thread(&RootNode::application, this);
    try {
        controller.run();
    } catch(exception& e) {
        //Note: omnet++ seems to terminate coroutines with an exception
        //of type cStackCleanupException. Squelch these
        if(string(typeid(e).name()).find("cStackCleanupException")==string::npos)
            cerr<<"\nException thrown: "<<e.what()<<endl;
        //TODO: perform clean shutdown when simulation stops
        exit(0);
        quit.store(true);
        t->join();
        throw;
    } catch(...) {
        quit.store(true);
        t->join();
        cerr<<"\nUnnamed exception thrown"<<endl;
        throw;
    }
    quit.store(true);
    t->join();
}

void RootNode::application() {
    /* Wait for TDMH to become ready */
    MACContext* ctx = tdmh->getMACContext();
    while(!ctx->isReady()) {
    }
    openServer(ctx, 1, Period::P1, Redundancy::TRIPLE_SPATIAL);
}

void RootNode::openServer(MACContext* ctx, unsigned char port, Period period, Redundancy redundancy) {
    try{
        // NOTE: we can't use Stream API functions in simulator
        // so we have to get a pointer to StreamManager
        StreamManager* mgr = ctx->getStreamManager();
        auto params = StreamParameters(redundancy,                 // Redundancy
                                       period,                     // Period
                                       1,                          // Payload size
                                       Direction::TX);             // Direction
        printf("[A] Opening server on port %d\n", port);
        /* Open a Server to listen for incoming streams */
        int server = mgr->listen(port,              // Destination port
                                 params);           // Server parameters
        if(server < 0) {
            printf("[A] Server opening failed! error=%d\n", server);
            return;
        }
        while(mgr->getInfo(server).getStatus() == StreamStatus::LISTEN) {
            int stream = mgr->accept(server);
            pair<int, StreamManager*> arg = make_pair(stream, mgr);
            thread t1(&RootNode::streamThread, this, arg);
            t1.detach();
        }
    } catch(exception& e) {
        //Note: omnet++ seems to terminate coroutines with an exception
        //of type cStackCleanupException. Squelch these
        if(string(typeid(e).name()).find("cStackCleanupException")==string::npos)
            cerr<<"\nException thrown: "<<e.what()<<endl;
    }
}

void RootNode::streamThread(pair<int, StreamManager*> arg) {
    try{
        int stream = arg.first;
        StreamManager* mgr = arg.second;
        StreamId id = mgr->getInfo(stream).getStreamId();
        printf("[A] Master node: Stream (%d,%d) accepted\n", id.src, id.dst);
        // Receive data until the stream is closed
        while(mgr->getInfo(stream).getStatus() == StreamStatus::ESTABLISHED) {
            Data data;
            int len = mgr->read(stream, &data, sizeof(data));
            if(len >= 0) {
                if(len == sizeof(data))
                    printf("[A] Received data from (%d,%d): ID=%d Time=0 MinHeap=0 Heap=0 Counter=%u\n",
                            id.src, id.dst, data.id, data.counter);
                else
                    printf("[E] Received wrong size data from Stream (%d,%d): %d\n",
                            id.src, id.dst, len);
            }
            else if(len == -1) {
                // No data received
                printf("[E] No data received from Stream (%d,%d)\n", id.src, id.dst);
            }
        }
        printf("[A] Stream (%d,%d) was closed\n", id.src, id.dst);
    }catch(...){
        printf("Exception thrown in streamThread\n");
    }
}
