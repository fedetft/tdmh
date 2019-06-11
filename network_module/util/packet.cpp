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
#include "../mac_context.h"
#include "../downlink_phase/timesync/timesync_downlink.h"

using namespace std;
using namespace miosix;

namespace mxnet {

void Packet::put(const void* data, unsigned int putSize) {
    if(putSize > available())
        throw range_error("Packet::put: Overflow!");
    memcpy(packet.data()+dataSize, data, putSize);
    dataSize += putSize;
}

void Packet::get(void* data, unsigned int getSize) {
    if(getSize > size())
        throw PacketUnderflowException("Packet::get: Underflow!");
    memcpy(data, packet.data()+dataStart, getSize);
    dataStart += getSize;
}

void Packet::discard(unsigned int discardSize) {
    if(discardSize > size())
        throw PacketUnderflowException("Packet::discard: Underflow!");
    dataStart += discardSize;
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
        result = ctx.recv(packet.data(), maxSize(), timeout, corr);
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

void Packet::putPanHeader(unsigned short panId) {
    unsigned char panHeader[] = {
                                 0x46, //frame type 0b110 (reserved), intra pan
                                 0x08, //no source addressing, short destination addressing
                                 0xff, //seq no is reused as packet type (0xff=uplink), or glossy hop count
                                 static_cast<unsigned char>(panId>>8),
                                 static_cast<unsigned char>(panId & 0xff), //destination pan ID
    };
    put(&panHeader, sizeof(panHeader));
}

bool Packet::checkPanHeader(unsigned short panId) {
    if(packet.size() < 5)
        return false;
    // Check panHeader inside packet without extracting it
    if(packet.at(0) == 0x46 &&
       packet.at(1) == 0x08 &&
       packet.at(2) == 0xff &&
       packet.at(3) == static_cast<unsigned char>(panId >> 8) &&
       packet.at(4) == static_cast<unsigned char>(panId & 0xff)) {
        return true;
    }else {
        return false;
    }
}

} /* namespace mxnet */
