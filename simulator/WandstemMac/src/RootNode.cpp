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
#include "network_module/stream.h"
#include <iostream>
#include <stdexcept>
#include <chrono>

Define_Module(RootNode);

using namespace std;
using namespace mxnet;

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
    thread t(&RootNode::application, this);
    try {
        controller.run();
    } catch(exception& e) {
        //Note: omnet++ seems to terminate coroutines with an exception
        //of type cStackCleanupException. Squelch these
        if(string(typeid(e).name()).find("cStackCleanupException")==string::npos)
            cerr<<"\nException thrown: "<<e.what()<<endl;
        quit.store(true);
        t.join();
        throw;
    }
    quit.store(true);
    t.join();
}

void RootNode::application() {
    /* Wait for TDMH to become ready */
    MACContext* ctx = tdmh->getMACContext();
    while(!ctx->isReady()) {
        this_thread::sleep_for(chrono::seconds(1));
    }
    /* Open a StreamServer to listen for incoming streams */
    mxnet::StreamServer server(*tdmh,      // Pointer to MediumAccessController
                 0,                 // Destination port
                 Period::P50,        // Period
                 1,                 // Payload size
                 Direction::TX,     // Direction
                 Redundancy::NONE); // Redundancy
    Stream r(*tdmh);
    server.accept(r);
    vector<char> data;
    data.resize(125);
    data.resize(r.recv(data.data(), data.size()));
    printf("[A] Received data \n");
    miosix::memDump(data.data(), data.size());
}
