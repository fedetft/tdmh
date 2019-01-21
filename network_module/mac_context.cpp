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
    delete scheduleDistribution;
    delete data;
    delete stream;
}
    
MACContext::MACContext(const MediumAccessController& mac, Transceiver& transceiver, const NetworkConfiguration& config) :
                mac(mac), transceiverConfig(config.getBaseFrequency(), config.getTxPower(), true, false),
                networkConfig(config), networkId(config.getStaticNetworkId()), transceiver(transceiver),
                pm(miosix::PowerManager::instance()), controlSuperframe(networkConfig.getControlSuperframeStructure()),
                sendTotal(0), sendErrors(0), rcvTotal(0), rcvErrors(0),
                running(false)
{
    calculateDurations();
}

TopologyContext* MACContext::getTopologyContext() const { return uplink->getTopologyContext(); }
StreamManagementContext* MACContext::getStreamManagementContext() const { return uplink->getStreamManagementContext(); }

void MACContext::calculateDurations() {
    dataSlotDuration = DataPhase::getDuration();
    uplinkSlotDuration = UplinkPhase::getDuration(networkConfig.getNumUplinkPackets());
    /* Align the Uplink slot duration is a multiple of the Data slot duration */
    if (uplinkSlotDuration % dataSlotDuration != 0)
        uplinkSlotDuration = (uplinkSlotDuration / dataSlotDuration + 1) * dataSlotDuration;
    printf("####DEBUG####\n- maxhops: %d\n- scheduledownlinkphase: %d\n- timesyncdownlinkphase: %d\n",
           networkConfig.getMaxHops(), ScheduleDownlinkPhase::getDuration(networkConfig.getMaxHops()),
           TimesyncDownlink::getDuration(networkConfig.getMaxHops()));
    auto downlinkMaxDuration = std::max(ScheduleDownlinkPhase::getDuration(networkConfig.getMaxHops()), TimesyncDownlink::getDuration(networkConfig.getMaxHops()));
    /* Align the Downlink slot duration is a multiple of the Data slot duration */
    if (downlinkMaxDuration % dataSlotDuration == 0)
        downlinkSlotDuration = downlinkMaxDuration;
    else
        downlinkSlotDuration = (downlinkMaxDuration / dataSlotDuration + 1) * dataSlotDuration;
}

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

void MACContext::warmUp() {

    auto tileDuration = networkConfig.getTileDuration();
    /* Tile size in number of data slots, needed for the scheduler */
    numSlotInTile = tileDuration / dataSlotDuration;
   if(tileDuration - downlinkSlotDuration < dataSlotDuration)
        throwLogicError("downlink slot (%lld) too large for tile (%lld)", downlinkSlotDuration, tileDuration);

    if(tileDuration - uplinkSlotDuration < dataSlotDuration)
        throwLogicError("uplink slot (%lld) too large for tile (%lld)", uplinkSlotDuration, tileDuration);

    numDataSlotInDownlinkTile  = (tileDuration - downlinkSlotDuration) / dataSlotDuration;
    numDataSlotInUplinkTile    = (tileDuration - uplinkSlotDuration)   / dataSlotDuration;
    
    assert(downlinkSlotDuration + numDataSlotInDownlinkTile * dataSlotDuration ==
           uplinkSlotDuration   + numDataSlotInUplinkTile   * dataSlotDuration);
    tileSlackTime = tileDuration - (uplinkSlotDuration + numDataSlotInUplinkTile * dataSlotDuration);
}

void MACContext::run()
{
    warmUp();
    transceiver.turnOn();
    timesync->macStartHook();
    startScheduler();

    long long currentNextDeadline = 0;
    unsigned int controlSuperframeCounter = 0;
    int tileCounter = 0;
    for(running = true; running; )
    {
        unsigned dataSlots;
        if(controlSuperframe.isControlDownlink(tileCounter))
        {
            if(tileCounter==0 && controlSuperframeCounter==0)
            {
                timesync->execute(currentNextDeadline);
                currentNextDeadline = timesync->getSlotframeStart();
            } else {
                beginScheduling();
                scheduleDistribution->run(currentNextDeadline);
            }
            currentNextDeadline += downlinkSlotDuration;
            dataSlots = numDataSlotInDownlinkTile;
        } else {
            uplink->run(currentNextDeadline);
            currentNextDeadline += uplinkSlotDuration;
            dataSlots = numDataSlotInUplinkTile;
        }
            
        for(unsigned i = 0; i < dataSlots; i++)
        {
            data->run(currentNextDeadline);
            currentNextDeadline += dataSlotDuration;
        }
        
        currentNextDeadline += tileSlackTime;
        if(++tileCounter >= controlSuperframe.size())
        {
            tileCounter=0;
            if(++controlSuperframeCounter >= networkConfig.getNumSuperframesPerClockSync())
                controlSuperframeCounter=0;
        }
    }
    transceiver.turnOff();
}


void MACContext::stop() {
    running = false;
}

}
