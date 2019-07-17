/***************************************************************************
 *   Copyright (C) 2018-2019 by Federico Amedeo Izzo                       *
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
#include "../util/debug_settings.h"
#include <unistd.h>

using namespace std;
using namespace miosix;

namespace mxnet {

void DataPhase::execute(long long slotStart) {
    // Schedule not received yet
    if(tileSlot >= currentSchedule.size()) {
        sleep(slotStart);
        return;
    }
    // NOTE FIXME: this sleep is currently needed only in the simulator, to avoid
    // the mac thread taking 100% of the CPU and delaying the application thread
    // NOTE: This usleep makes the simulation a lot slower but avoids errors
#ifndef _MIOSIX
    usleep(1000);
#endif
    // Schedule playback
    switch(currentSchedule[tileSlot].getAction()){
    case Action::SLEEP:
        sleep(slotStart);
        break;
    case Action::SENDSTREAM:
        sendFromStream(slotStart, currentSchedule[tileSlot].getStreamId());
        break;
    case Action::RECVSTREAM:
        receiveToStream(slotStart, currentSchedule[tileSlot].getStreamId());
        break;
    case Action::SENDBUFFER:
        sendFromBuffer(slotStart);
        break;
    case Action::RECVBUFFER:
        receiveToBuffer(slotStart);
        break;
    }
    incrementSlot();
}
void DataPhase::sleep(long long slotStart) {
    ctx.sleepUntil(slotStart);
}
void DataPhase::sendFromStream(long long slotStart, StreamId id) {
    Packet pkt;
    bool pktReady = stream.sendPacket(id, pkt);
    if(pktReady) {
        ctx.configureTransceiver(ctx.getTransceiverConfig());
        pkt.send(ctx, slotStart);
        ctx.transceiverIdle();
        if(ENABLE_DATA_INFO_DBG) {
            auto nt = NetworkTime::fromLocalTime(slotStart);
            if(COMPRESSED_DBG==false)
                print_dbg("[D] Node %d: Sent packet for stream (%d,%d) NT=%lld\n", myId, id.src, id.dst, nt.get());
            else
                print_dbg("[D] s (%d,%d) NT=%lld\n", id.src, id.dst, nt.get());
        }
    }
    else
        sleep(slotStart);
}
void DataPhase::receiveToStream(long long slotStart, StreamId id) {
    Packet pkt;
    ctx.configureTransceiver(ctx.getTransceiverConfig());
    auto rcvResult = pkt.recv(ctx, slotStart);
    ctx.transceiverIdle();
    bool periodEnd = false;
    if(rcvResult.error == RecvResult::ErrorCode::OK &&
       pkt.checkPanHeader(panId) == true &&
       checkStreamId(pkt, id) == true) {
        periodEnd = stream.receivePacket(id, pkt);
        if(ENABLE_DATA_INFO_DBG) {
            auto nt = NetworkTime::fromLocalTime(slotStart);
            if(COMPRESSED_DBG==false)
                print_dbg("[D] Node %d: Received packet for stream (%d,%d) NT=%lld\n", myId, id.src, id.dst, nt.get());
            else
                print_dbg("[D] r (%d,%d) NT=%lld\n", id.src, id.dst, nt.get());
        }
    }
    // Avoid overwriting valid data
    else {
        periodEnd = stream.missPacket(id);
        if(ENABLE_DATA_ERROR_DBG) {
            auto nt = NetworkTime::fromLocalTime(slotStart);
            if(COMPRESSED_DBG==false)
                print_dbg("[D] Node %d: Missed packet for stream (%d,%d) NT=%lld\n", myId, id.src, id.dst, nt.get());
            else
                print_dbg("[D] m (%d,%d) NT=%lld\n", id.src, id.dst, nt.get());
        }
    }
    if(ENABLE_DATA_INFO_DBG || ENABLE_DATA_ERROR_DBG) {
        if(periodEnd)
        {
            if(COMPRESSED_DBG==false)
                print_dbg("[D] Node %d: (%d,%d) --- \n", myId, id.src, id.dst);
            else
                print_dbg("[D] - (%d,%d)\n", id.src, id.dst);
        }
    }
}
void DataPhase::sendFromBuffer(long long slotStart) {
    if(bufferValid) {
        ctx.configureTransceiver(ctx.getTransceiverConfig());
        buffer.send(ctx, slotStart);
        ctx.transceiverIdle();
        buffer.clear();
    }
    else
        sleep(slotStart);
}
void DataPhase::receiveToBuffer(long long slotStart) {
    ctx.configureTransceiver(ctx.getTransceiverConfig());
    auto rcvResult = buffer.recv(ctx, slotStart);
    ctx.transceiverIdle();
    if(rcvResult.error == RecvResult::ErrorCode::OK && buffer.checkPanHeader(panId) == true) { 
        bufferValid = true;
    }
    // Delete received packet if pan header doesn't match with our network 
    else {
        bufferValid = false;
        buffer.clear();
    }
}
bool DataPhase::checkStreamId(Packet pkt, StreamId streamId) {
    if(pkt.size() < 8)
        return false;
    // Check streamId inside packet without extracting it
    static_assert(sizeof(StreamId) == 3, "");
    // Read streamId bytes inside the packet
    StreamId packetId = StreamId::fromBytes(&pkt[5]);
    return packetId == streamId;
}
}
