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
#include "network_module/stream.h"
#include <miosix.h>
#include <iostream>
#include <stdexcept>
#include <chrono>

Define_Module(Node);

using namespace std;
using namespace miosix;
using namespace mxnet;

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
    
    try {
        thread t(&Node::application, this);
        controller.run();
        quit.store(true);
        t.join();
    } catch(DisconnectException&) {
        print_dbg("===> Stopping @ %lld (disconnecTime %lld)\n",getTime(),disconnectTime);
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
    while(!ctx->isReady()) {
        this_thread::sleep_for(chrono::seconds(1));
    }
    /* Delay the Stream opening so it gets opened after the StreamServer */
    this_thread::sleep_for(chrono::seconds(1));
    /* Open Stream from node 1 */
    if(address == 1) {
        while(true){
            try{
                /* Open a Stream to another node */
                mxnet::Stream s(*tdmh,            // Pointer to MediumAccessController
                                0,                 // Destination node
                                0,                 // Destination port
                                Period::P20,        // Period
                                1,                 // Payload size
                                Direction::TX,     // Direction
                                Redundancy::NONE); // Redundancy
                vector<char> data={1,2,3,4};
                s.send(data.data(), data.size());
                printf("[A] Sent data 1234\n");
                break;
            } catch(exception& e) {
                //Note: omnet++ seems to terminate coroutines with an exception
                //of type cStackCleanupException. Squelch these
                if(string(typeid(e).name()).find("cStackCleanupException")==string::npos)
                    cerr<<"\nException thrown: "<<e.what()<<endl;
            }
        }
    }
}
