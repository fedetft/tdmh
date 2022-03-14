/***************************************************************************
 *   Copyright (C) 2017-2022 by Polidori Paolo, Terraneo Federico,         *
 *   Federico Amedeo Izzo, Luca Conterio                                   *
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
#include <chrono>
#include "network_module/network_configuration.h"
#include "network_module/util/stackrange.h"
#include "network_module/util/stats.h"
#include "interfaces-impl/gpio_timer_corr.h"

#include "experiments/control_demo/src/controller_node.h"
#include "experiments/control_demo/src/sensor_node.h"
#include "experiments/control_demo/src/actuator_node.h"
#include "experiments/control_demo/src/temp_sensor.h"
#include "experiments/control_demo/src/utils.h"

using namespace std;
using namespace mxnet;
using namespace miosix;
using namespace std::placeholders;

const int maxNodes = 16;
const int maxHops = 6;
const int txPower = 5; //dBm

FastMutex m;
MediumAccessController *tdmh = nullptr;

inline int guaranteedTopologies(int maxNumNodes, bool useWeakTopologies)
{
    /*
     * Dynamic Uplink allocation implemented
     * the parameter topologySMERatio configures the fraction of uplink packet
     * reserved for topology messages
     * UplinkMessagePkt   { hop, assignee, numTopology, numSME } : 4
     * NeighborTable      { bitmask }                            : bitmaskSize
     * TopologyElement(s) { nodeId, bitmask } * n                : (1+bitmaskSize) * n
     */
    int bitmaskSize = useWeakTopologies ? maxNumNodes/4 : maxNumNodes/8;
    const float topologySMERatio = 0.5;
    int packetCapacity = (125 - 4 - bitmaskSize) / (1 + bitmaskSize);
    return std::min<int>(packetCapacity * topologySMERatio, maxNumNodes - 2);
}

void masterNode(void*)
{
    if (ENABLE_STACK_STATS_DBG) {
        printStackRange("MAC (master)");
    }

    try {
        printf("Master node\n");
        bool useWeakTopologies=true;
        const NetworkConfiguration config(
            maxHops,       //maxHops
            maxNodes,      //maxNodes
            0,             //networkId
            false,         //staticHop
            6,             //panId
            txPower,       //txPower
            2450,          //baseFrequency
            10000000000,   //clockSyncPeriod
            guaranteedTopologies(maxNodes,useWeakTopologies), //guaranteedTopologies
            1,             //numUplinkPackets
            100000000,     //tileDuration
            150000,        //maxAdmittedRcvWindow
            0,             //callbacksExecutionTime
            8,             //maxRoundsUnavailableBecomesDead
            128,           //maxRoundsWeakLinkBecomesDead
            -75,           //minNeighborRSSI
            -95,           //minWeakNeighborRSSI
            4,             //maxMissedTimesyncs
            true,          //channelSpatialReuse
            useWeakTopologies //useWeakTopologies
#ifdef CRYPTO
            ,
            true,          //authenticateControlMessages
            false,         //encryptControlMessages
            true,          //authenticateDataMessages
            true,          //encryptDataMessages
            true,          //doMasterChallengeAuthentication
            500, //3000,          //masterChallengeAuthenticationTimeout
            3000           //rekeyingPeriod
#endif
        );
        printf("Starting TDMH with guaranteedTopologies=%d\n", guaranteedTopologies(maxNodes,useWeakTopologies));
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
    if (ENABLE_STACK_STATS_DBG) {
        printStackRange("MAC (dynamic)");
    }

    try {
        auto arg = reinterpret_cast<Arg*>(argv);
        //printf("Dynamic node %d",arg->id);
        //if(arg->hop) printf(" forced hop %d",arg->hop);
        //printf("\n");
        bool useWeakTopologies=true;
        const NetworkConfiguration config(
            maxHops,       //maxHops
            maxNodes,      //maxNodes
            arg->id,       //networkId
            arg->hop,      //staticHop
            6,             //panId
            txPower,       //txPower
            2450,          //baseFrequency
            10000000000,   //clockSyncPeriod
            guaranteedTopologies(maxNodes,useWeakTopologies), //guaranteedTopologies
            1,             //numUplinkPackets
            100000000,     //tileDuration
            150000,        //maxAdmittedRcvWindow
            0,             //callbacksExecutionTime
            8,             //maxRoundsUnavailableBecomesDead
            128,           //maxRoundsWeakLinkBecomesDead
            -75,           //minNeighborRSSI
            -95,           //minWeakNeighborRSSI
            4,             //maxMissedTimesyncs
            true,          //channelSpatialReuse
            useWeakTopologies          //useWeakTopologies
#ifdef CRYPTO
            ,
            true,          //authenticateControlMessages
            false,         //encryptControlMessages
            true,          //authenticateDataMessages
            true,          //encryptDataMessages
            true,          //doMasterChallengeAuthentication
            500, //3000,          //masterChallengeAuthenticationTimeout
            3000           //rekeyingPeriod
#endif
        );
        //printf("Starting TDMH with guaranteedTopologies=%d\n", guaranteedTopologies(maxNodes,useWeakTopologies));
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
    if (ENABLE_STACK_STATS_DBG) {
        printStackRange("main");
    }
    
    TempSerialSensor sensor;
    sensor.run();
    
    unsigned long long nodeID = getUniqueID();
    //printf("### TDMH Test code ###\nNodeID=%llx\n", nodeID);
    const int macThreadStack = 4096;
    //Thread* t1;
    /* Start TDMH thread mapping node unique ID to network ID */
    switch(nodeID) {
    case 0x243537035155c338:
        /*t1 =*/ Thread::create(masterNode, macThreadStack, PRIORITY_MAX-1, nullptr, Thread::JOINABLE);
        break;
    case 0x243537005155c356:
        /*t1 =*/ Thread::create(masterNode, macThreadStack, PRIORITY_MAX-1, nullptr, Thread::JOINABLE);
        ///*t1 =*/ Thread::create(dynamicNode, macThreadStack, PRIORITY_MAX-1, new Arg(1), Thread::JOINABLE);
        break;
    case 0x243537005155c346:
        /*t1 =*/ Thread::create(dynamicNode, macThreadStack, PRIORITY_MAX-1, new Arg(2), Thread::JOINABLE);
        break;
    case 0x243537025155c346:
        /*t1 =*/ Thread::create(dynamicNode, macThreadStack, PRIORITY_MAX-1, new Arg(3), Thread::JOINABLE);
        break;
    case 0x243537035155c356:
        /*t1 =*/ Thread::create(dynamicNode, macThreadStack, PRIORITY_MAX-1, new Arg(4), Thread::JOINABLE);
        break;
    case 0x243537035155bdca:
        /*t1 =*/ Thread::create(dynamicNode, macThreadStack, PRIORITY_MAX-1, new Arg(5), Thread::JOINABLE);
        break;
    case 0x243537015155bdab:
        /*t1 =*/ Thread::create(dynamicNode, macThreadStack, PRIORITY_MAX-1, new Arg(6), Thread::JOINABLE);
        break;
    case 0x243537015155c9bf:
        /*t1 =*/ Thread::create(dynamicNode, macThreadStack, PRIORITY_MAX-1, new Arg(7), Thread::JOINABLE);
        break;
    case 0x2435370352c6aa9a:
        /*t1 =*/ Thread::create(dynamicNode, macThreadStack, PRIORITY_MAX-1, new Arg(8), Thread::JOINABLE);
        break;
    case 0x243537015155bdba:
        /*t1 =*/ Thread::create(dynamicNode, macThreadStack, PRIORITY_MAX-1, new Arg(9), Thread::JOINABLE);
        break;
    case 0x243537025155bdba:
        /*t1 =*/ Thread::create(dynamicNode, macThreadStack, PRIORITY_MAX-1, new Arg(10), Thread::JOINABLE);
        break;
    case 0x243537025155bdca:
        /*t1 =*/ Thread::create(dynamicNode, macThreadStack, PRIORITY_MAX-1, new Arg(11), Thread::JOINABLE);
        break;
    case 0x243537005155bdba:
        /*t1 =*/ Thread::create(dynamicNode, macThreadStack, PRIORITY_MAX-1, new Arg(12), Thread::JOINABLE);
        break;
    case 0x243537035155bdba:
        /*t1 =*/ Thread::create(dynamicNode, macThreadStack, PRIORITY_MAX-1, new Arg(13), Thread::JOINABLE);
        break;
    case 0x243537005155bdab:
        /*t1 =*/ Thread::create(dynamicNode, macThreadStack, PRIORITY_MAX-1, new Arg(14), Thread::JOINABLE);
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

    ControllerNode c(ctx);
    SensorNode s(ctx, &sensor);
    ActuatorNode a(ctx); // miss client params

    /* Perform actions depending on your network ID */
    switch(netID) {
    case controllerNodeID: // Controller node
        printf("[A] Running controller\n");
        c.run();
        break;
    case sensorNodeID: // Sensor node
        printf("[A] Running sensor\n");
        s.run();
        break;
    case actuatorNodeID: // Actuator node
        //printf("[A] Running actuator\n");
        a.run();
        break;
    default:
        idle();
    }
}
