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

#include "network_module/master_medium_access_controller.h"
#include "network_module/network_configuration.h"

Define_Module(RootNode);

using namespace mxnet;

void RootNode::activity()
{
    using namespace miosix;
    print_dbg("Master node\n");
    const NetworkConfiguration config(
            16,            //maxHops
            256,           //maxNodes
            address,       //networkId
            false,         //staticHop
            6,             //panId
            5,             //txPower
            2460,          //baseFrequency
            10000000000,   //clockSyncPeriod
            2,             //maxForwardedTopologies
            100000000,     //tileDuration
            150000,        //maxAdmittedRcvWindow
            2,             //maxRoundsUnavailableBecomesDead
            -90,           //minNeighborRSSI
            3              //maxMissedTimesyncs
    );
    MasterMediumAccessController controller(Transceiver::instance(), config);
    controller.run();
}
