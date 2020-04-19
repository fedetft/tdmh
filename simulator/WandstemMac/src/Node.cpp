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

#include "Node.h"
#include "DisconnectMessage.h"
#include "network_module/dynamic_tdmh.h"
#include "network_module/network_configuration.h"
#include <miosix.h>
#include <iostream>
#include <stdexcept>
#include <chrono>

Define_Module(Node);

using namespace std;
using namespace miosix;
using namespace mxnet;

struct Data
{
    Data() {}
    Data(int id, unsigned int counter) : id(id), counter(counter){}
    unsigned char id;
    unsigned int counter;
}__attribute__((packed));


void Node::initialize()
{
    NodeBase::initialize();
    connectTime = par("connect_time").intValue();
    disconnectTime = par("disconnect_time").intValue();
}

void Node::activity()
try {
    print_dbg("Dynamic node %d\n", address);
    using namespace miosix;
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
#ifdef CRYPTO
            ,
            true,          //authenticateControlMessages
            false,         //encryptControlMessages
            true,          //authenticateDataMessages
            true           //encryptDataMessages
#endif
    );
    DynamicMediumAccessController controller(Transceiver::instance(), config);
    tdmh = &controller;
    
    if(connectTime > 0)
    {
        Thread::nanoSleepUntil(connectTime);
        print_dbg("===> Starting @ %lld\n", connectTime);
    }
    
    if(disconnectTime < numeric_limits<long long>::max())
    {
        print_dbg("Note: setting disconnect event\n");
        scheduleAt(SimTime(disconnectTime, SIMTIME_NS), new DisconnectMessage);
    }
    
    thread *t = nullptr;
    try {
        if(openStream) t = new thread(&Node::application, this);
        controller.run();
        if(t) t->join();
    } catch(DisconnectException&) {
        print_dbg("===> Stopping @ %lld (disconnectTime %lld)\n",getTime(),disconnectTime);
        if(t) t->join();
        // Here we wait forever because terminating the node would cause errors on sendAt
        // targeted to the terminated node
        while(true) delete receive();
    }
    
} catch(exception& e) {
    //NOTE: omnet++ seems to terminate coroutines with an exception
    //of type cStackCleanupException. Squelch these
    if(string(typeid(e).name()).find("cStackCleanupException")==string::npos)
        cerr<<"\nException thrown: "<<e.what()<<endl;
    throw;
}

void Node::application() {
    /* Wait for TDMH to become ready */
    MACContext* ctx = tdmh->getMACContext();
    while(!ctx->isReady()) ;
    Period p = Period::P10;
    Redundancy r = Redundancy::TRIPLE_SPATIAL;
    sendData(ctx, 0, p, r);
}

void Node::sendData(MACContext* ctx, unsigned char dest, Period period, Redundancy redundancy) {
    try{
        // NOTE: we can't use Stream API functions in simulator
        // so we have to get a pointer to StreamManager
        StreamManager* mgr = ctx->getStreamManager();
        auto params = StreamParameters(redundancy, period, 1, Direction::TX);

        /* Open a Stream to another node */
        int stream;
        do{
            printf("[A] Node %d: Opening stream to node %d\n", address, dest);
            stream = mgr->connect(dest,          // Destination node
                             1,             // Destination port
                             params);       // Stream parameters
            if(stream < 0) {                
                printf("[A] Stream opening failed! error=%d\n", stream);
            }
        }while(stream < 0);
        StreamId id = mgr->getInfo(stream).getStreamId();
        printf("[A] Stream (%d,%d) opened \n", id.src, id.dst);
        unsigned int counter = 1;
        while(mgr->getInfo(stream).getStatus() == StreamStatus::ESTABLISHED) {
            Data data(ctx->getNetworkId(), counter);
            /*int len =*/ mgr->write(stream, &data, sizeof(data));
            //printf("[A] Sent ID=%d Counter=%u, result=%d \n", data.id, data.counter, len);
            counter++;
        }
        printf("[A] Stream was closed\n");
    } catch(exception& e) {
        //Note: omnet++ seems to terminate coroutines with an exception
        //of type cStackCleanupException. Squelch these
        if(string(typeid(e).name()).find("cStackCleanupException")==string::npos)
            cerr<<"\nException thrown: "<<e.what()<<endl;
    }
}

void Node::openServer(MACContext* ctx, unsigned char port, Period period, Redundancy redundancy) {
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
            thread t1(&Node::streamThread, this, arg);
            t1.detach();
        }
    } catch(exception& e) {
        //Note: omnet++ seems to terminate coroutines with an exception
        //of type cStackCleanupException. Squelch these
        if(string(typeid(e).name()).find("cStackCleanupException")==string::npos)
            cerr<<"\nException thrown: "<<e.what()<<endl;
    }
}

void Node::streamThread(pair<int, StreamManager*> arg) {
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

