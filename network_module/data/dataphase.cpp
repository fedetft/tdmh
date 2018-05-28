/***************************************************************************
 *   Copyright (C)  2018 by Polidori Paolo                                 *
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

#include "dataphase.h"
#include "../debug_settings.h"
#include "../timesync/timesync_downlink.h"
#include <algorithm>

using namespace std;
using namespace miosix;

namespace mxnet {

void DataPhase::execute(long long slotStart) {
    nextSlot();
    if (scheduleDownlink->isScheduleEnd(curSched) || (*curSched)->getDataslot() != dataSlot) return;
    ctx.configureTransceiver(ctx.getTransceiverConfig());
    auto sched = *curSched++;
    switch (sched->getRole()) {
    //TODO move to the polymorphic methods of DynamicScheduleElement::run
    case DynamicScheduleElement::SENDER: {
        char a[8];
        snprintf(a, 8, "hello %hhu", ctx.getNetworkId());
        strcpy((char *)packet.data(), a);
        auto wu = timesync->getSenderWakeup(slotStart);
        auto now = getTime();
        if (now >= slotStart) {
            if (ENABLE_DATA_ERROR_DBG)
                print_dbg("[D] start late\n");
            break;
        }
        if (now < wu)
            ctx.sleepUntil(wu);
        ctx.sendAt(packet.data(), 8, slotStart);
        if (ENABLE_DATA_INFO_DBG)
            print_dbg("[D]<-#%hu @%llu[%hu]\n", sched->getId(), slotStart, dataSlot);
        break;
    }
    case DynamicScheduleElement::RECEIVER: {
        auto wakeUpTimeout = timesync->getWakeupAndTimeout(slotStart);
        auto now = getTime();
        if (now + timesync->getReceiverWindow() >= slotStart) {
            if (ENABLE_DATA_ERROR_DBG)
                print_dbg("[D] start late\n");
            break;
        }
        if (now < wakeUpTimeout.first)
            ctx.sleepUntil(wakeUpTimeout.first);
        do {
            rcvResult = ctx.recv(packet.data(), packet.size(), wakeUpTimeout.second);
            if (ENABLE_PKT_INFO_DBG) {
                if(rcvResult.size) {
                    print_dbg("Received packet, error %d, size %d, timestampValid %d: ", rcvResult.error, rcvResult.size, rcvResult.timestampValid);
                    if (ENABLE_PKT_DUMP_DBG)
                        memDump(packet.data(), rcvResult.size);
                } else print_dbg("No packet received, timeout reached\n");
            }
        } while (rcvResult.error != miosix::RecvResult::TIMEOUT && rcvResult.error != miosix::RecvResult::OK);
        if (rcvResult.error == RecvResult::OK) {
            char a[8];
            strcpy(a, (char *)packet.data());
            if (ENABLE_DATA_INFO_DBG)
#ifndef _MIOSIX
                print_dbg("[D]#%hu @%llu[%hu] %s\n", sched->getId(), rcvResult.timestamp, dataSlot, a);
#else
                print_dbg("[D]#%hu %c\n", sched->getId(), a[6]);
#endif
        } else if (ENABLE_DATA_INFO_DBG)
#ifndef _MIOSIX
            print_dbg("[D]#%hu @%llu[%hu] ERR\n", sched->getId(), slotStart, dataSlot);
#else
            print_dbg("[D]#%hu X\n", sched->getId());
#endif
        break;
    }
    case DynamicScheduleElement::FORWARDEE: {
        auto wakeUpTimeout = timesync->getWakeupAndTimeout(slotStart);
        auto now = getTime();
        if (now + timesync->getReceiverWindow() >= slotStart) {
            if (ENABLE_DATA_ERROR_DBG)
                print_dbg("[D] start late\n");
            break;
        }
        if (now < wakeUpTimeout.first)
            ctx.sleepUntil(wakeUpTimeout.first);
        do {
            rcvResult = ctx.recv(packet.data(), packet.size(), wakeUpTimeout.second);
            if (ENABLE_PKT_INFO_DBG) {
                if(rcvResult.size) {
                    print_dbg("Received packet, error %d, size %d, timestampValid %d: ", rcvResult.error, rcvResult.size, rcvResult.timestampValid);
                    if (ENABLE_PKT_DUMP_DBG)
                        memDump(packet.data(), rcvResult.size);
                } else print_dbg("No packet received, timeout reached\n");
            }
        } while (rcvResult.error != miosix::RecvResult::TIMEOUT && rcvResult.error != miosix::RecvResult::OK);
        if (rcvResult.error == RecvResult::OK) {
            queues[sched->getId()] = vector<unsigned char>(packet.begin(), packet.begin() + rcvResult.size);
            if (ENABLE_DATA_INFO_DBG)
                print_dbg("[D]->#%hu{%hhu} @%llu[%hu]\n", sched->getId(), rcvResult.size, rcvResult.timestamp, dataSlot);
        } else if (ENABLE_DATA_INFO_DBG)
            print_dbg("[D]->#%hu[%hu] ERR\n", sched->getId(), dataSlot);
        break;
    }
    case DynamicScheduleElement::FORWARDER: {
        if (queues[sched->getId()].empty()) {
            print_dbg("[D] #%hhu @%llu[%hu] empty Q\n", sched->getId(), slotStart, dataSlot);
            break;
        }
        std::copy_n(queues[sched->getId()].begin(), queues[sched->getId()].size(), packet.begin());
        queues[sched->getId()].resize(0);
        auto wu = timesync->getSenderWakeup(slotStart);
        auto now = getTime();
        if (now >= slotStart) {
            if (ENABLE_DATA_ERROR_DBG)
                print_dbg("[D] start late\n");
            break;
        }
        if (now < wu)
            ctx.sleepUntil(wu);
        ctx.sendAt(packet.data(), 8, slotStart);
        if (ENABLE_DATA_INFO_DBG)
            print_dbg("[D]<-#%hu @%llu[%hu]\n", sched->getId(), slotStart, dataSlot);
        break;
    }
    }
    ctx.transceiverIdle();
}

void DataPhase::advance(long long int slotStart) {
    nextSlot();
}

void DataPhase::alignToNetworkTime(NetworkTime nt) {
    //TODO provide a version with only the timesync index (k)
    auto networkConfig = ctx.getNetworkConfig();
    auto controlSuperframe = networkConfig.getControlSuperframeStructure();
    auto tileDuration = networkConfig.getTileDuration();

    auto timeWithinTile = nt.get() % tileDuration;
    auto tilesPassedTotal = nt.get() / tileDuration;

    //during each control superframe, the number of tiles
    unsigned tilesPassed = tilesPassedTotal % controlSuperframe.size();

    //contains the number of data phases already executed
    //the initial value is 0, since we start counting from the control
    //superframe start
    unsigned long long phase = 0;

    //adds the data slots for each passed tile
    for(unsigned i = 0; i < tilesPassed; i++)
    {
        phase += controlSuperframe.isControlDownlink(i)?
                ctx.getDataSlotsInDownlinkTileCount():
                ctx.getDataSlotsInUplinkTileCount();
    }

    //removes the control time
    timeWithinTile -= controlSuperframe.isControlDownlink(tilesPassed)?
            ctx.getDownlinkSlotDuration():
            ctx.getUplinkSlotDuration();

    if (timeWithinTile > 0)
        phase += timeWithinTile / ctx.getDataSlotDuration();
    dataSlot = phase;
    curSched = scheduleDownlink->getScheduleForOrBeforeSlot(phase);
    // TODO remove porchettona
    dataSlot = 0;
    curSched = scheduleDownlink->getFirstSchedule();
}

}

