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
    const NetworkConfiguration config(
            hops,            //maxHops
            nodes,           //maxNodes
            address,       //networkId
            false,         //staticHop
            6,             //panId
            5,             //txPower
            2460,          //baseFrequency
            10000000000,   //clockSyncPeriod
            maxForwardedTopologiesFromMaxNumNodes(nodes), //maxForwardedTopologies
            1,             //numUplinkPackets
            100000000,     //tileDuration
            150000,        //maxAdmittedRcvWindow
            3,             //maxRoundsUnavailableBecomesDead
            -75,           //minNeighborRSSI
            3              //maxMissedTimesyncs
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
    /* Open a StreamServer to listen for incoming streams */
    mxnet::StreamServer server(*tdmh,      // Pointer to MediumAccessController
                 0,                 // Destination port
                 Period::P1,        // Period
                 1,                 // Payload size
                 Direction::TX,     // Direction
                 Redundancy::TRIPLE_SPATIAL); // Redundancy
    while(!server.isClosed()) {
        std::list<shared_ptr<Stream>> streamList;
        server.accept(streamList);
        for(auto& stream : streamList){
            thread t1(&RootNode::streamThread, this, stream);
            t1.detach();
        }
    }
}

void RootNode::streamThread(shared_ptr<Stream> s) {
    try{
        printf("[A] Accept returned! \n");
        while(!s->isClosed()){
            Data data;
            int len = s->recv(&data, sizeof(data));
            if(len != sizeof(data))
                printf("[E] Received wrong size data\n");
            else
                printf("[A] Received ID=%d Counter=%u\n",
                       data.id, data.counter);
        }
    }catch(...){
        printf("Exception thrown in streamThread\n");
    }
}
