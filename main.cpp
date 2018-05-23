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
#include "network_module/network_configuration.h"
#include "network_module/dynamic_medium_access_controller.h"
#include "network_module/master_medium_access_controller.h"
#include "network_module/timesync/networktime.h"
#include "interfaces-impl/gpio_timer_corr.h"

using namespace std;
using namespace mxnet;
using namespace miosix;

void masterNode(void*)
{
    printf("Master node\n");
    const NetworkConfiguration config(
        6,             //maxHops
        32,            //maxNodes
        0,             //networkId
        false,         //staticHop
        6,             //panId
        5,             //txPower
        2460,          //baseFrequency
        10000000000,   //clockSyncPeriod
        10,            //maxForwardedTopologies
        100000000,     //tileDuration
        150000,        //maxAdmittedRcvWindow
        2,             //maxRoundsUnavailableBecomesDead
        -80,           //minNeighborRSSI
        3              //maxMissedTimesyncs
    );
    MasterMediumAccessController controller(Transceiver::instance(), config);
    controller.run();
}

void dynamicNode(void* arg)
{
    auto node = reinterpret_cast<int>(arg);
    printf("Dynamic node %d\n",node);
    const NetworkConfiguration config(
        6,             //maxHops
        32,            //maxNodes
        node,          //networkId
        false,         //staticHop
        6,             //panId
        5,             //txPower
        2460,          //baseFrequency
        10000000000,   //clockSyncPeriod
        10,            //maxForwardedTopologies
        100000000,     //tileDuration
        150000,        //maxAdmittedRcvWindow
        2,             //maxRoundsUnavailableBecomesDead
        -80,           //minNeighborRSSI
        3              //maxMissedTimesyncs
    );
    DynamicMediumAccessController controller(Transceiver::instance(), config);
    controller.run();
}

int main()
{
    auto t1 = Thread::create(masterNode, 2048, PRIORITY_MAX-1, nullptr, Thread::JOINABLE);
//     auto t1 = Thread::create(dynamicNode, 2048, PRIORITY_MAX-1, reinterpret_cast<void*>(1), Thread::JOINABLE);
//     auto t1 = Thread::create(dynamicNode, 2048, PRIORITY_MAX-1, reinterpret_cast<void*>(2), Thread::JOINABLE);
//     auto t1 = Thread::create(dynamicNode, 2048, PRIORITY_MAX-1, reinterpret_cast<void*>(3), Thread::JOINABLE);
//     auto t1 = Thread::create(dynamicNode, 2048, PRIORITY_MAX-1, reinterpret_cast<void*>(4), Thread::JOINABLE);
//     auto t1 = Thread::create(dynamicNode, 2048, PRIORITY_MAX-1, reinterpret_cast<void*>(5), Thread::JOINABLE);
//     auto t1 = Thread::create(dynamicNode, 2048, PRIORITY_MAX-1, reinterpret_cast<void*>(6), Thread::JOINABLE);
//     auto t1 = Thread::create(dynamicNode, 2048, PRIORITY_MAX-1, reinterpret_cast<void*>(7), Thread::JOINABLE);
//     auto t1 = Thread::create(dynamicNode, 2048, PRIORITY_MAX-1, reinterpret_cast<void*>(8), Thread::JOINABLE);
    
    t1->join();
    
//     sleep(25);
//     auto& timestamp=GPIOtimerCorr::instance();
//     auto now=NetworkTime::fromNetworkTime(((NetworkTime::now().get()/10000000000)+1)*10000000000+1000000000);
//     auto period=NetworkTime::fromNetworkTime(10000000000);
//     printf("--- %lld %lld %lld\n",now.get(),now.toLocalTime(),period.get());
//     for(;;now+=period)
//     {
//         timestamp.absoluteWaitTrigger(timestamp.ns2tick(now.toLocalTime()));
//         printf("[TIMESTAMP] %lld %lld\n",now.get(),now.toLocalTime());
//     }
//     
//     return 0;
}
