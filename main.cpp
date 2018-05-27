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

class Arg
{
public:
    Arg(unsigned char id, unsigned char hop=false) : id(id), hop(hop) {}
    const unsigned char id, hop;
};

void masterNode(void*)
{
    try {
        printf("Master node\n");
        const NetworkConfiguration config(
            6,             //maxHops
            32,            //maxNodes
            0,             //networkId
            false,         //staticHop
            6,             //panId
            5,             //txPower
            2450,          //baseFrequency
            10000000000,   //clockSyncPeriod
            10,            //maxForwardedTopologies
            100000000,     //tileDuration
            150000,        //maxAdmittedRcvWindow
            3,             //maxRoundsUnavailableBecomesDead
            -75,           //minNeighborRSSI
            3              //maxMissedTimesyncs
        );
        MasterMediumAccessController controller(Transceiver::instance(), config);
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
    try {
        auto arg = reinterpret_cast<Arg*>(argv);
        printf("Dynamic node %d",arg->id);
        if(arg->hop) printf(" forced hop %d",arg->hop);
        printf("\n");
        const NetworkConfiguration config(
            6,             //maxHops
            32,            //maxNodes
            arg->id,       //networkId
            arg->hop,      //staticHop
            6,             //panId
            5,             //txPower
            2450,          //baseFrequency
            10000000000,   //clockSyncPeriod
            10,            //maxForwardedTopologies
            100000000,     //tileDuration
            150000,        //maxAdmittedRcvWindow
            3,             //maxRoundsUnavailableBecomesDead
            -75,           //minNeighborRSSI
            3              //maxMissedTimesyncs
        );
        DynamicMediumAccessController controller(Transceiver::instance(), config);
        controller.run();
    } catch(exception& e) {
        for(;;)
        {
            printf("exception %s\n",e.what());
            Thread::sleep(1000);
        }
    }
}

void blinkThread(void *)
{
    for(;;)
    {
        redLed::high();
        Thread::sleep(10);
        redLed::low();
        Thread::sleep(990);
    }
}

int main()
{
    auto t1 = Thread::create(masterNode, 2048, PRIORITY_MAX-1, nullptr, Thread::JOINABLE);
//     auto t1 = Thread::create(dynamicNode, 2048, PRIORITY_MAX-1, new Arg(1,1), Thread::JOINABLE);
//     auto t1 = Thread::create(dynamicNode, 2048, PRIORITY_MAX-1, new Arg(2,3), Thread::JOINABLE);
//     auto t1 = Thread::create(dynamicNode, 2048, PRIORITY_MAX-1, new Arg(3,1), Thread::JOINABLE);
//     auto t1 = Thread::create(dynamicNode, 2048, PRIORITY_MAX-1, new Arg(4,2), Thread::JOINABLE);
//     auto t1 = Thread::create(dynamicNode, 2048, PRIORITY_MAX-1, new Arg(5,1), Thread::JOINABLE);
//     auto t1 = Thread::create(dynamicNode, 2048, PRIORITY_MAX-1, new Arg(6,3), Thread::JOINABLE);
//     auto t1 = Thread::create(dynamicNode, 2048, PRIORITY_MAX-1, new Arg(7,1), Thread::JOINABLE);
//     auto t1 = Thread::create(dynamicNode, 2048, PRIORITY_MAX-1, new Arg(8,2), Thread::JOINABLE);
    
    Thread::create(blinkThread,STACK_MIN,PRIORITY_MAX-1);
//     t1->join();
    
    auto& timestamp=GPIOtimerCorr::instance();
    for(;;)
    {
        auto period=NetworkTime::fromNetworkTime(10000000000);
        auto now=NetworkTime::fromNetworkTime(((NetworkTime::now().get()/period.get())+2)*period.get());
        auto offset=NetworkTime::fromLocalTime(0).get();
        printf("--- %lld %lld %lld\n",now.get(),now.toLocalTime(),period.get());
        for(;;now+=period)
        {
            timestamp.absoluteWaitTrigger(timestamp.ns2tick(now.toLocalTime()));
            printf("[TIMESTAMP] %lld %lld\n",now.get(),now.toLocalTime());
            MemoryProfiling::print();
            if(offset!=NetworkTime::fromLocalTime(0).get()) break; //Offset changed
        }
    }
    
    return 0;
}
