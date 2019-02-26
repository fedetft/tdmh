/***************************************************************************
 *   Copyright (C)  2018 by Terraneo Federico, Federico Amedeo Izzo        *
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

#include "debug_settings.h"
#include "packet.h"
#include "mac_context.h"
#include "timesync/timesync_downlink.h"

using namespace std;
using namespace miosix;

namespace mxnet {

void Packet::put(const void* data, int size) {
    if((size + dataSize) > static_cast<int>(packet.size()))
        throw range_error("Packet::put: Overflow!");
    memcpy(packet.data()+dataSize, data, size);
    dataSize += size;
}

void Packet::get(void* data, int size) {
    if(size > (dataSize - dataStart))
        throw range_error("Packet::get: Underflow!");
    memcpy(data, packet.data()+dataStart, size);
    dataStart += size;
}

void Packet::send(MACContext& ctx, long long sendTime) const {
    auto wuTime = ctx.getTimesync()->getSenderWakeup(sendTime);
    auto now = getTime();
    if (now >= sendTime) {
        print_dbg("Packet::send: too late\n");
        return;
    }
    if (now < wuTime)
        ctx.sleepUntil(wuTime);
#ifdef _MIOSIX
    redLed::high();
#endif
    ctx.sendAt(packet.data(), dataSize, sendTime);
#ifdef _MIOSIX
    redLed::low();
#endif
}

RecvResult Packet::recv(MACContext& ctx, long long tExpected, function<bool (const Packet& p, RecvResult r)> pred, Transceiver::Correct corr) {
    RecvResult result;
    long long timeout = infiniteTimeout;
    if(tExpected != infiniteTimeout) {
        auto wakeUpTimeout = ctx.getTimesync()->getWakeupAndTimeout(tExpected);
        timeout = wakeUpTimeout.second;
        auto now = getTime();
        if (now + ctx.getTimesync()->getReceiverWindow() >= tExpected) {
            print_dbg("Packet::recv: too late\n");
            // Returning RecvResult object with default values
            return result;
        }
        if (now < wakeUpTimeout.first)
            ctx.sleepUntil(wakeUpTimeout.first);
    }
    for(;;) {
#ifdef _MIOSIX
        redLed::high();
#endif
        result = ctx.recv(packet.data(), packet.size(), timeout, corr);
#ifdef _MIOSIX
        redLed::low();
#endif
        if (ENABLE_PKT_INFO_DBG) {
            if(result.size) {
                print_dbg("Packet::recv: Received packet, error %d, size %d, timestampValid %d: ",
                          result.error, result.size, result.timestampValid);
                if (ENABLE_PKT_DUMP_DBG)
                    memDump(packet.data(), result.size);
            } else print_dbg("Packet::recv: No packet received, timeout reached\n");
        }
        if(result.error == RecvResult::ErrorCode::TIMEOUT)
            break;

        dataStart = 0;
        dataSize = result.size;
        if(result.error == RecvResult::ErrorCode::OK && pred(*this, result))
            break;
    }
    return result;
}

} /* namespace mxnet */
