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
    using namespace miosix;
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
        t = new thread(&Node::application, this);
        controller.run();
        t->join();
    } catch(DisconnectException&) {
        print_dbg("===> Stopping @ %lld (disconnectTime %lld)\n",getTime(),disconnectTime);
        t->join();
        // Here we wait forever because terminating the node would cause errors on sendAt
        // targeted to the terminated node
        while(true) delete receive();
    }
    
} catch(exception& e) {
    //Note: omnet++ seems to terminate coroutines with an exception
    //of type cStackCleanupException. Squelch these
    if(string(typeid(e).name()).find("cStackCleanupException")==string::npos)
        cerr<<"\nException thrown: "<<e.what()<<endl;
    throw;
}

void Node::application() {
    /* Wait for TDMH to become ready */
    MACContext* ctx = tdmh->getMACContext();
    while(!ctx->isReady()) ;
    /* Open Stream from node 1 */
    if(address == 1) {
        sendData(ctx, Period::P1, Redundancy::NONE);
    }
    /* Open Stream from node 2 */
    if(address == 2) {
        sendData(ctx, Period::P1, Redundancy::NONE);
    }
    /* Open Stream from node 3 */
    if(address == 3) {
        sendData(ctx, Period::P1, Redundancy::NONE);
    }
}

void Node::sendData(MACContext* ctx, Period period, Redundancy redundancy) {
    try{
        auto params = StreamParameters(redundancy, period, 1, Direction::TX);
        unsigned char dest = 0;

        /* Open a Stream to another node */
        printf("[A] Opening stream to node %d\n", dest);
        int stream = connect(dest,          // Destination node
                             1,             // Destination port
                             params);       // Stream parameters
        if(stream < 0) {                
            printf("[A] Stream opening failed! error=%d\n", stream);
            return;
        }
        printf("[A] Stream opened \n");
        unsigned int counter = 0;
        while(getInfo(stream).getStatus() == StreamStatus::ESTABLISHED) {
            Data data(ctx->getNetworkId(), counter);
            write(stream, &data, sizeof(data));
            printf("[A] Sent ID=%d Counter=%u\n", data.id, data.counter);
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
