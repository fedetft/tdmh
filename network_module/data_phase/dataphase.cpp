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
#ifdef CRYPTO
#include "../crypto/initialization_vector.h"
#include "../crypto/aes_gcm.h"
#endif

using namespace std;
using namespace miosix;

namespace mxnet {

void DataPhase::execute(long long slotStart) {
    // Empty schedule
    if(scheduleTiles == 0) {
        this->sleep(slotStart);
        return;
    }
    // NOTE FIXME: this sleep is currently needed only in the simulator, to avoid
    // the mac thread taking 100% of the CPU and delaying the application thread
    // NOTE: This usleep makes the simulation a lot slower but avoids errors
#ifndef _MIOSIX
    usleep(1000);
#endif
    try {
        // Schedule playback
        switch(currentSchedule.at(slotIndex).getAction()){
        case Action::SLEEP:
            this->sleep(slotStart);
            break;
        case Action::SENDSTREAM:
            sendFromStream(slotStart, currentSchedule.at(slotIndex).getStreamId());
            break;
        case Action::RECVSTREAM:
            receiveToStream(slotStart, currentSchedule.at(slotIndex).getStreamId());
            break;
        case Action::SENDBUFFER:
            sendFromBuffer(slotStart, currentSchedule.at(slotIndex).getBuffer(),
                                    currentSchedule.at(slotIndex).getStreamId());
            break;
        case Action::RECVBUFFER:
            receiveToBuffer(slotStart, currentSchedule.at(slotIndex).getBuffer(),
                                    currentSchedule.at(slotIndex).getStreamId());
            break;
        }
        incrementSlot();
    } catch(...) {
        incrementSlot(); //Do not forget to increment the slot
        throw;
    }
}

void DataPhase::advance(long long slotStart) {
    // Empty schedule
    if(scheduleTiles == 0) {
        this->sleep(slotStart);
        return;
    }
    try {
        StreamId id = currentSchedule.at(slotIndex).getStreamId();
        Packet pkt;
        switch(currentSchedule.at(slotIndex).getAction()){
        case Action::SLEEP:
            this->sleep(slotStart);
            break;
        case Action::SENDSTREAM:
            stream.sendPacket(id, pkt);
            break;
        case Action::RECVSTREAM:
            stream.missPacket(id);
            break;
        default:
            this->sleep(slotStart);
            break;
        }
        incrementSlot();
    } catch(...) {
        incrementSlot(); //Do not forget to increment the slot
        throw;
    }
}

void DataPhase::sleep(long long slotStart) {
    ctx.sleepUntil(slotStart);
}

void DataPhase::sendFromStream(long long slotStart, StreamId id) {
    Packet pkt;
    bool pktReady;

#ifdef CRYPTO
    if (config.getAuthenticateDataMessages()) {
        unsigned long long seqNo = stream.getSequenceNumber(id);
        /**
         * NOTE: sendPacket must be called after getSequenceNumber, because
         * sendPacket advances the sequence numbers too.
         */
        pktReady = stream.sendPacket(id, pkt);
        if (pktReady) {
            AesGcm& gcm = stream.getStreamGCM(id);
            unsigned int masterIndex = ctx.getKeyManager()->getMasterIndex();

            if (ENABLE_CRYPTO_DATA_DBG)
                print_dbg("[D] sendFromStream: FrameNumber = %u, seqNo = %llu, mIndex = %u\n",
                          dataSuperframeNumber, seqNo, masterIndex);
            gcm.setIV(dataSuperframeNumber, seqNo, masterIndex);

            if (config.getEncryptDataMessages()) pkt.encryptAndPutTag(gcm);
            else pkt.putTag(gcm);
        }
    } else {
        pktReady = stream.sendPacket(id, pkt);
    }
#else
    pktReady = stream.sendPacket(id, pkt);
#endif


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
        this->sleep(slotStart);
}

void DataPhase::receiveToStream(long long slotStart, StreamId id) {
    Packet pkt;
    ctx.configureTransceiver(ctx.getTransceiverConfig());
    auto rcvResult = pkt.recv(ctx, slotStart);
    ctx.transceiverIdle();

    bool periodEnd = false;
    bool valid = true;
    if(rcvResult.error == RecvResult::ErrorCode::OK && pkt.checkPanHeader(panId) == true) {
#ifdef CRYPTO
        if (config.getAuthenticateDataMessages()) {
            AesGcm& gcm = stream.getStreamGCM(id);
            unsigned int masterIndex = ctx.getKeyManager()->getMasterIndex();
            if (ENABLE_CRYPTO_DATA_DBG)
                print_dbg("[D] receiveToStream: FrameNumber = %u, seqNo = %llu, mIndex = %u\n",
                          dataSuperframeNumber, stream.getSequenceNumber(id),
                          masterIndex);
            gcm.setIV(dataSuperframeNumber, stream.getSequenceNumber(id),
                      masterIndex);
            if (config.getEncryptDataMessages()) valid &= pkt.verifyAndDecrypt(gcm);
            else valid &= pkt.verify(gcm);

            if (ENABLE_CRYPTO_DATA_DBG)
                if (!valid) print_dbg("[D] receiveToStream: verify failed\n");
        }
#endif
        valid &= checkStreamId(pkt, id);
    } else {
        valid = false;
    }

    if (valid) {
        periodEnd = stream.receivePacket(id, pkt);
        if(ENABLE_DATA_INFO_DBG) {
            auto nt = NetworkTime::fromLocalTime(slotStart);
            if(COMPRESSED_DBG==false)
                print_dbg("[D] Node %d: Received packet for stream (%d,%d) NT=%lld\n", myId, id.src, id.dst, nt.get());
            else
                print_dbg("[D] r (%d,%d) NT=%lld\n", id.src, id.dst, nt.get());
        }
    } else {
        // Avoid overwriting valid data
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
void DataPhase::sendFromBuffer(long long slotStart,
                               std::shared_ptr<Packet> buffer, StreamId id) {
    if(!buffer)
    {
        print_dbg("Error: DataPhase::sendFromBuffer no buffer\n");
        return;
    }
    if(buffer->empty()==false) {
        ctx.configureTransceiver(ctx.getTransceiverConfig());
        buffer->send(ctx, slotStart);
        ctx.transceiverIdle();

        incrementBufCtr(id);
        if(lastTransmission(id)) {
            buffer->clear();
            resetBufCtr(id);
        }
    } else {
        /* If buffer is empty we still need to decrement the counter */
        incrementBufCtr(id);
        if(lastTransmission(id)) {
            resetBufCtr(id);
        }
        this->sleep(slotStart);
    }
}
void DataPhase::receiveToBuffer(long long slotStart,
                                std::shared_ptr<Packet> buffer, StreamId id) {
    if(!buffer)
    {
        print_dbg("Error: DataPhase::receiveToBuffer no buffer\n");
        return;
    }
    ctx.configureTransceiver(ctx.getTransceiverConfig());
    auto rcvResult = buffer->recv(ctx, slotStart);
    ctx.transceiverIdle();
    if(rcvResult.error != RecvResult::ErrorCode::OK || buffer->checkPanHeader(panId) == false) {
        // Delete received packet if pan header doesn't match with our network
        buffer->clear();
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

void DataPhase::incrementBufCtr(StreamId id){
    auto it = bufCtr.find(id);
    if(it != bufCtr.end()) {
        bufCtr[id].first++;
    } else {
        print_dbg("BUG: buffer counters incorrectly initialized\n");
    }
}

bool DataPhase::lastTransmission(StreamId id){
    auto it = bufCtr.find(id);
    if(it != bufCtr.end()) {
        return (bufCtr[id].first >= bufCtr[id].second);
    } else {
        print_dbg("BUG: buffer counters incorrectly initialized\n");
        return true;
    }
}

void DataPhase::resetBufCtr(StreamId id){
    auto it = bufCtr.find(id);
    if(it != bufCtr.end()) {
        bufCtr[id].first = 0;
    } else {
        print_dbg("BUG: buffer counters incorrectly initialized\n");
    }
}

}
