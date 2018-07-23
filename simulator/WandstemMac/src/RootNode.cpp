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

Define_Module(RootNode);

using namespace std;
using namespace mxnet;

void RootNode::activity()
try {
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
            100000000,     //tileDuration
            150000,        //maxAdmittedRcvWindow
            3,             //maxRoundsUnavailableBecomesDead
            -75,           //minNeighborRSSI
            3              //maxMissedTimesyncs
    );
    MasterMediumAccessController controller(Transceiver::instance(), config);
    controller.run();
} catch(exception& e) {
	cerr<<"\nException thrown: "<<e.what()<<endl;
	throw;
}
