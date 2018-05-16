/***************************************************************************
 *   Copyright (C)  2017 by Polidori Paolo                                 *
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

#include "mac_context.h"
#include "debug_settings.h"
#include "timesync/timesync_downlink.h"
#include "uplink/uplink_phase.h"
#include "data/dataphase.h"
#include "interfaces-impl/transceiver.h"
#include <type_traits>
#include <stdexcept>

using namespace std;
using namespace miosix;

namespace mxnet {

MACContext::~MACContext() {
    delete timesync;
    delete uplink;
    delete topologyContext;
    delete streamManagement;
    delete schedule;
    delete data;
}
    
MACContext::MACContext(const MediumAccessController& mac, Transceiver& transceiver, const NetworkConfiguration& config) :
                mac(mac), transceiverConfig(config.getBaseFrequency(), config.getTxPower(), true, false),
                networkConfig(config), networkId(config.getStaticNetworkId()), transceiver(transceiver),
                pm(miosix::PowerManager::instance()), sendTotal(0), sendErrors(0), rcvTotal(0), rcvErrors(0),
                running(false) {}

TopologyContext* MACContext::getTopologyContext() const { return uplink->getTopologyContext(); }
StreamManagementContext* MACContext::getStreamManagementContext() const { return uplink->getStreamManagementContext(); }

void MACContext::sendAt(const void* pkt, int size, long long ns) {
    try {
        transceiver.sendAt(pkt, size, ns);
    } catch(std::exception& e) {
        sendErrors++;
        if (ENABLE_RADIO_EXCEPTION_DBG)
            print_dbg("%s\n", e.what());
    }
    if (++sendTotal & 1 << 31) {
        sendTotal >>= 1;
        sendErrors >>= 1;
    }
}

void MACContext::sendAt(const void* pkt, int size, long long ns, std::function<void(std::exception&)> cbk) {
    try {
        transceiver.sendAt(pkt, size, ns);
    } catch(std::exception& e) {
        sendErrors++;
        if (ENABLE_RADIO_EXCEPTION_DBG)
            print_dbg("%s\n", e.what());
        cbk(e);
    }
    if (++sendTotal & 1 << 31) {
        sendTotal >>= 1;
        sendErrors >>= 1;
    }
}

RecvResult MACContext::recv(void* pkt, int size, long long timeout, Transceiver::Correct c) {
    RecvResult rcvResult;
    try {
        rcvResult = transceiver.recv(pkt, size, timeout, Transceiver::Unit::NS, c);
    } catch(std::exception& e) {
        rcvErrors++;
        if (ENABLE_RADIO_EXCEPTION_DBG)
            print_dbg("%s\n", e.what());
    }
    if (++rcvTotal & (1 << 31)) {
        rcvTotal >>= 1;
        rcvErrors >>= 1;
    }
    return rcvResult;
}

RecvResult MACContext::recv(void *pkt, int size, long long timeout, std::function<void(std::exception&)> cbk, Transceiver::Correct c) {
    RecvResult rcvResult;
    try {
        rcvResult = transceiver.recv(pkt, size, timeout, Transceiver::Unit::NS, c);
    } catch(std::exception& e) {
        rcvErrors++;
        if (ENABLE_RADIO_EXCEPTION_DBG)
            print_dbg("%s\n", e.what());
        cbk(e);
    }
    if (++rcvTotal & (1 << 31)) {
        rcvTotal >>= 1;
        rcvErrors >>= 1;
    }
    return rcvResult;
}

void MACContext::run()
{
    auto downlinkSlotDuration = max(timesync->getDuration(),schedule->getDuration());
    auto uplinkSlotDuration = uplink->getDuration();
    auto dataSlotDuration = data->getDuration();
    auto tileDuration = networkConfig.getTileDuration();
    
    auto clockSyncPeriod = networkConfig.getClockSyncPeriod();
    auto controlSuperframe = networkConfig.getControlSuperframeStructure();
    auto controlSuperframeDuration = tileDuration * controlSuperframe.size();
    
    if(tileDuration - downlinkSlotDuration < dataSlotDuration)
        throw logic_error("downlink slot too large for tile");
    
    if(tileDuration - uplinkSlotDuration < dataSlotDuration)
        throw logic_error("uplink slot too large for tile");
    
    if(clockSyncPeriod % controlSuperframeDuration != 0)
        throw logic_error("control superframe does not divide clock sync period");
    
    unsigned int numDataSlotInDownlinkTile  = (tileDuration - downlinkSlotDuration) / dataSlotDuration;
    unsigned int numDataSlotInUplinkTile    = (tileDuration - uplinkSlotDuration)   / dataSlotDuration;
    unsigned int numSuperframesPerClockSync = clockSyncPeriod / controlSuperframeDuration;
    
    transceiver.turnOn();
    long long currentNextDeadline = 0;
    unsigned int controlSuperframeCounter = 0;
    int tileCounter = 0;
    for(running = true; running; )
    {
        if(controlSuperframe.isControlDownlink(tileCounter))
        {
            if(tileCounter==0 && controlSuperframeCounter==0)
            {
                timesync->execute(currentNextDeadline);
                if(!timesync->macCanOperate()) continue;
            } else {
                schedule->execute(currentNextDeadline);
            }
            
            for(int i = 0; i < numDataSlotInDownlinkTile; i++)
            {
                data->execute(currentNextDeadline);
            }
        } else {
            uplink->execute(currentNextDeadline);
            
            for(int i = 0; i < numDataSlotInUplinkTile; i++)
            {
                data->execute(currentNextDeadline);
            }
        }
        
        if(++tileCounter>=controlSuperframe.size())
        {
            tileCounter=0;
            if(++controlSuperframeCounter >= numSuperframesPerClockSync)
                controlSuperframeCounter=0;
        }
    }
    transceiver.turnOff();

//     transceiver.turnOn();
//     long long currentNextDeadline = 0;
//     for (running = true; running; ) {
//         timesync->execute(currentNextDeadline);
//         if (!timesync->macCanOperate())
//             continue;
//         currentNextDeadline = timesync->getSlotframeStart() + timesync->getDuration();
//         for (int i = 0; i < networkConfig.getMaxNodes() - 1; i++) {
//             uplink->execute(currentNextDeadline);
//             currentNextDeadline += uplink->getDuration();
//         }
//         schedule->execute(currentNextDeadline);
//         currentNextDeadline += schedule->getDuration();
//         for (unsigned i = 0; i < data->getSlotsCount(); i++) {
//             data->execute(currentNextDeadline);
//             currentNextDeadline += data->getDuration();
//         }
//     }
//     transceiver.turnOff();
}


void MACContext::stop() {
    running = false;
}

}
