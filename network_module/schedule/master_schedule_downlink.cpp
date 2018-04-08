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

#include "../debug_settings.h"
#include "master_schedule_downlink.h"
#include <algorithm>

using namespace std;
using namespace miosix;

namespace mxnet {

void MasterScheduleDownlinkPhase::execute(long long slotStart) {
    auto* s = dynamic_cast<MasterScheduleContext*>(scheduleContext);
    auto data = s->getScheduleToDistribute(MediumAccessController::maxPktSize - 1);
    auto bitSize = numeric_limits<unsigned char>::digits;
    packet[0] = data.first.size();
    for (auto* d : data.first) {
        std::vector<unsigned char> v(packet.begin(), packet.end());
        d->serialize(v, bitSize);
        bitSize += d->getBitSize();
    }
    auto lsbShift = bitSize % numeric_limits<unsigned char>::digits;
    auto msbShift = numeric_limits<unsigned char>::digits - lsbShift;
    auto idx = bitSize / numeric_limits<unsigned char>::digits;
    if (lsbShift == 0)
        for(auto d : data.second) {
            packet[idx++] = d;
        }
    else {
        packet[idx] &= ~0 << msbShift;
        for(auto d : data.second) {
            packet[idx] |= d >> lsbShift;
            packet[++idx] = d << msbShift;
        }
    }
    transceiver.configure(ctx.getTransceiverConfig());
    transceiver.turnOn();
    //Thread::nanoSleepUntil(startTime);
    auto wakeUp = slotStart - MediumAccessController::sendingNodeWakeupAdvance;
    if(getTime() < wakeUp)
        pm.deepSleepUntil(wakeUp);
    try {
        transceiver.sendAt(packet.data(), (bitSize - 1) / numeric_limits<unsigned char>::digits + 1, slotStart);
    } catch(exception& e) {
        if (ENABLE_RADIO_EXCEPTION_DBG)
            print_dbg("%s\n", e.what());
    }
    if (ENABLE_TIMESYNC_DL_INFO_DBG)
        print_dbg("[SC] ST=%lld\n", slotStart);
    transceiver.turnOff();
}

}
