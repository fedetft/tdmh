/***************************************************************************
 *   Copyright (C)  2017 by Terraneo Federico, Polidori Paolo              *
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

#include "dynamic_timesync_downlink.h"
#include "networktime.h"
#include "../../uplink_phase/dynamic_uplink_phase.h"
#include "../../data_phase/dataphase.h"
#include "../../util/debug_settings.h"
#include <cassert>
#ifdef CRYPTO
#include "../../crypto/key_management/key_manager.h"
#include "../../crypto/aes_ocb.h"
#endif

using namespace miosix;

namespace mxnet {

void DynamicTimesyncDownlink::periodicSync() {
    greenLed::high();
    //slotStart = slotStart + (ctx.getHop() - 1) * rebroadcastInterval;
    auto correctedStart = correct(computedFrameStart);
    Packet pkt;
    using namespace std::placeholders;
    auto pred = std::bind(&DynamicTimesyncDownlink::isSyncPacket, this, _1, _2, true);
    auto rcvResult = pkt.recv(ctx, correctedStart, pred, Transceiver::Correct::UNCORR);
    if(rcvResult.error != RecvResult::ErrorCode::OK) {
        // Turn off the radio because we don't need it anymore this round
        ctx.transceiverIdle();
        auto n = missedPacket();
        if (ENABLE_TIMESYNC_DL_INFO_DBG) {
            auto nt = NetworkTime::fromLocalTime(getSlotframeStart());
            print_dbg("[T] miss NT=%lld u=%d w=%d\n", nt.get(), clockCorrection, receiverWindow);
            if (n >= networkConfig.getMaxMissedTimesyncs())
                print_dbg("[T] lost sync\n");
        }
    }
    else {
        //corrected time in NS, to pass to transceiver
        pkt[2]++;
        //Rebroadcast the sync packet
        measuredFrameStart = correct(rcvResult.timestamp);
        rebroadcast(pkt, measuredFrameStart);
        ctx.transceiverIdle();

#ifdef CRYPTO
        unsigned currentMI = ctx.getKeyManager()->getMasterIndex();
        unsigned int mI = *reinterpret_cast<unsigned int*>(&pkt[11]);

        bool indexValid;
        if (mI < currentMI) {
            indexValid = false;
        } else if (mI > currentMI+1) {
            // An advancement of more than 1 in masterIndex is not accepted
            indexValid = false;
        } else if (mI == currentMI+1) {
            KeyManagerStatus s = ctx.getKeyManager()->getStatus();
            if (s == KeyManagerStatus::CONNECTED)
                ctx.getKeyManager()->attemptAdvance();
            else if (s == KeyManagerStatus::MASTER_UNTRUSTED) {
                ctx.getKeyManager()->advanceResync();
            }
            indexValid = true;
        } else {
            // currentMI == mI
            indexValid = true;
        }

        bool verified = true;
        if(networkConfig.getAuthenticateControlMessages()) {
            /**
             * in order to authenticate the timesync packet, we set the
             * hop value to zero, as the master only sends and authenticates
             * packets with hop = 0.
             * the correct hop value is saved and restored later for the only
             * purpose of printing debug information.
             */
            unsigned char hop = pkt[2];
            pkt[2] = 0;

            AesOcb& ocb = ctx.getKeyManager()->getTimesyncOCB();
            /**
             * Note: the first argument of setNonce is supposed to be set to the
             * current tileNumber, which can be computed directly from the
             * slotframeStart in timesync.
             * Sequence number is always set to 1 in timesync messages.
             * Here we getMasterIndex again because it is possible that is was
             * changed by attempAdvance() or advanceResync().
             */
            unsigned int tile = ctx.getCurrentTile(getSlotframeStart());
            unsigned long long seqNo = 1;
            unsigned int mI = ctx.getKeyManager()->getMasterIndex();
            if (ENABLE_CRYPTO_TIMESYNC_DBG)
                print_dbg("[T] Verifying timesync: tile=%u, seqNo=%llu, mI=%u\n",
                          tile, seqNo, mI);
            ocb.setNonce(tile, seqNo, mI);
            verified = pkt.verify(ocb);
            if(ENABLE_CRYPTO_TIMESYNC_DBG)
                if(!verified) print_dbg("[T] periodicSync: verify failed!\n");
            pkt[2] = hop;
        }

        KeyManagerStatus s = ctx.getKeyManager()->getStatus();
        if(indexValid && verified) {
            if (s == KeyManagerStatus::ADVANCING) ctx.getKeyManager()->commitAdvance();
            doPeriodicSync(correctedStart, rcvResult, pkt);
        } else {
            if (s == KeyManagerStatus::ADVANCING) ctx.getKeyManager()->rollbackAdvance();
            missedPacket();
        }
#else //ifdef CRYPTO
        doPeriodicSync(correctedStart, rcvResult, pkt);
#endif //ifdef CRYPTO
    }
    greenLed::low();
}

void DynamicTimesyncDownlink::doPeriodicSync(long long correctedStart,
                                    miosix::RecvResult rcvResult, Packet pkt) {
    // TODO: make a struct containing the packetCounter
    packetCounter++;
    auto newPacketCounter = *reinterpret_cast<unsigned int*>(&pkt[7]);
    if(newPacketCounter != packetCounter)
        print_dbg("[T] Received wrong packetCounter=%d (should be %d)", newPacketCounter, packetCounter);

    error = rcvResult.timestamp - computedFrameStart;
    std::pair<int,int> clockCorrectionReceiverWindow = synchronizer->computeCorrection(error);
    missedPackets = 0;
    clockCorrection = clockCorrectionReceiverWindow.first;
    receiverWindow = clockCorrectionReceiverWindow.second;
    internalStatus = IN_SYNC;
    updateVt();
    if (ENABLE_TIMESYNC_DL_INFO_DBG) {
        auto nt = NetworkTime::fromLocalTime(getSlotframeStart());
        print_dbg("[T] hop=%u NT=%lld ets=%lld ats=%lld e=%lld u=%d w=%d rssi=%d\n",
                  pkt[2],
                  nt.get(),
                  correctedStart,
                  rcvResult.timestamp,
                  error,
                  clockCorrection,
                  receiverWindow,
                  rcvResult.rssi);
    }
}

void DynamicTimesyncDownlink::resyncTime() {
    //Even the Theoretic is started at this time, so the absolute time is dependent of the board
    if (ENABLE_TIMESYNC_DL_INFO_DBG)
        print_dbg("[T] Resync\n");

    greenLed::high();

    Packet pkt;
    using namespace std::placeholders;
    auto pred = std::bind(&DynamicTimesyncDownlink::isSyncPacket, this, _1, _2, false);
    auto rcvResult = pkt.recv(ctx, infiniteTimeout, pred, Transceiver::Correct::UNCORR);

    greenLed::low();
    ++pkt[2];
    
    // NOTE: by the way the virtual clock is currently implemented, a clock
    // jump is observed whenever updateVt is called after a reset, due to the
    // resetting of theoreticalFrameStart. This will have to be fixed in the
    // virtual clock. Until this is done, however, we put this clock jump in the
    // most convenient place, which is before the first virtual clock use
    // after a resync (which is the "correct(rcvResult.timestamp)" line).
    // By doing so we do not interfere with the duration of the first sync
    // period, which would mess up the deadbeat in FLOPSYNC2 (recoverable),
    // but most importantly we do not get an unrecoverable clock error in
    // NetworkTime::setLocalNodeToNetworkTimeOffset.
    reset(rcvResult.timestamp);
    updateVt();
    
    measuredFrameStart = correct(rcvResult.timestamp);
    rebroadcast(pkt, measuredFrameStart);
    ctx.transceiverIdle();
    ctx.setHop(pkt[2]);
#ifdef CRYPTO
    // get masterIndex from packet, attempt to resync. Commit resync if packet
    // is valid, otherwise rollback.
    unsigned int mI = *reinterpret_cast<unsigned int*>(&pkt[11]);
    bool indexValid = ctx.getKeyManager()->attemptResync(mI);
    bool verified = true;
    if(networkConfig.getAuthenticateControlMessages()) {
        /**
         * In order to authenticate the timesync packet, we set the
         * hop value to zero, as the master only sends and authenticates
         * packets with hop = 0.
         * The correct hop value is saved and restored later for the only
         * purpose of printing debug information.
         */
        unsigned char hop = pkt[2];
        pkt[2] = 0;

        /* NOTE: to get the current tile, we need to initialize the
         * NetworkTime. If packet verification fails, we will go back to being
         * desynchronized, so it will not be a problem to have an incorrect
         * value for NetworkTime. */
        auto slotframeStart = getSlotframeStart();
        packetCounter = *reinterpret_cast<unsigned int*>(&pkt[7]);
        NetworkTime::setLocalNodeToNetworkTimeOffset(packetCounter *
                         networkConfig.getClockSyncPeriod() - slotframeStart);

        AesOcb& ocb = ctx.getKeyManager()->getTimesyncOCB();
        /**
         * Note: the first argument of setNonce is supposed to be set to the
         * current tileNumber. Because we are in the process of performing
         * resync, the tileNumber should be computed from the timing
         * information just received.
         * The same logic applies to the value of masterIndex.
         * Sequence number is always set to 1 in timesync messages.
         */
        unsigned int tile = ctx.getCurrentTile(getSlotframeStart());
        unsigned long long seqNo = 1;
        if (ENABLE_CRYPTO_TIMESYNC_DBG)
            print_dbg("[T] Verifying timesync: tile=%u, seqNo=%llu, mI=%u\n",
                      tile, seqNo, mI);
        ocb.setNonce(tile, seqNo, mI);
        verified = pkt.verify(ocb);
        if(ENABLE_CRYPTO_TIMESYNC_DBG)
            if(!verified) print_dbg("[T] resyncTime: verify failed!\n");
        pkt[2] = hop;
    }
    if(indexValid && verified) {
        /**
         * If the packet is valid and the masterIndex is acceptable, we perform
         * usual timesync operations. If the node is not using challenges to
         * verify the master's identity, we also commit resync.
         */
        doResyncTime(rcvResult, pkt);
        if(!networkConfig.getDoMasterChallengeAuthentication()) {
            ctx.getKeyManager()->commitResync();
        }
    } else {
        /**
         * If packet verification fails, or if the received masterIndex is
         * lower than the last valid one, the timesync message is not valid
         * and resync operation is aborted.
         */
        missedPacket();
        ctx.getKeyManager()->rollbackResync();
    }

#else //ifdef CRYPTO
    // Accept all messages if cryptography is disabled
    doResyncTime(rcvResult, pkt);
#endif //ifdef CRYPTO
}

void DynamicTimesyncDownlink::doResyncTime(miosix::RecvResult rcvResult, Packet pkt) {
    auto slotframeStart = getSlotframeStart();

    // NOTE: call resetMAC to clear the status of all the MAC components after resync
    resyncMAC();

    packetCounter = *reinterpret_cast<unsigned int*>(&pkt[7]);
    NetworkTime::setLocalNodeToNetworkTimeOffset(packetCounter *
                                                 networkConfig.getClockSyncPeriod() - slotframeStart);
    auto ntNow = NetworkTime::fromLocalTime(slotframeStart);
    ctx.getUplink()->alignToNetworkTime(ntNow);

    if (ENABLE_TIMESYNC_DL_INFO_DBG)      
        print_dbg("[T] hop=%d NT=%lld ats=%lld w=%d rssi=%d\n",
                  pkt[2], ntNow.get(), rcvResult.timestamp, receiverWindow, rcvResult.rssi
        );
}

inline void DynamicTimesyncDownlink::execute(long long slotStart)
{

    //the argument is ignored, since this is the time source class.
    next();
    ctx.configureTransceiver(ctx.getTransceiverConfig());
    if (internalStatus == DESYNCHRONIZED) {
        resyncTime();
    } else {
        periodicSync();
    }
    
#ifdef _MIOSIX
    unsigned int stackSize = MemoryProfiling::getStackSize();
    unsigned int absFreeStack = MemoryProfiling::getAbsoluteFreeStack();
    print_dbg("[H] MAC stack %d/%d\n",stackSize-absFreeStack,stackSize);
#endif
}

void DynamicTimesyncDownlink::rebroadcast(const Packet& pkt, long long arrivalTs){
    if(pkt[2] == networkConfig.getMaxHops()) return;
    pkt.send(ctx, arrivalTs + rebroadcastInterval);
}

void DynamicTimesyncDownlink::reset(long long hookPktTime) {
    //All the timestamps start from here, since i take this points as a ground reference
    //so no need to correct anything.
    synchronizer->reset();
    computedFrameStart = theoreticalFrameStart = hookPktTime;
    receiverWindow = synchronizer->getReceiverWindow();
    clockCorrection = 0;
    missedPackets = 0;
    error = 0;
    internalStatus = SYNCING;
}

void DynamicTimesyncDownlink::next() {
    //This an uncorrected clock! Good for Rtc, that doesn't correct by itself
    //needed because we ALWAYS need to consider the reference to be the first hook time,
    //otherwise we would build a second integrator without actually managing it.
    theoreticalFrameStart += networkConfig.getClockSyncPeriod();
    //This is the estimate of the next packet in our clock
    //using the FLOPSYNC-2 clock correction
    computedFrameStart += networkConfig.getClockSyncPeriod() + clockCorrection;
}

long long DynamicTimesyncDownlink::correct(long long int uncorrected) {
    //TODO FIXME this works by converting, applying the correction and converting back.
    //This leads to a non-negligible error.
    return tc->tick2ns(vt.uncorrected2corrected(tc->ns2tick(uncorrected)));
}

unsigned char DynamicTimesyncDownlink::missedPacket() {
    // We have not received the sync packet but we need to increment it
    // to keep the NetworkTime up to date
    packetCounter++;
    // NOTE: It is important that measuredFrameStart is a CORRECTED
    // time, because it is used as is in the NetworkTime
    measuredFrameStart = correct(computedFrameStart);

    if(++missedPackets >= networkConfig.getMaxMissedTimesyncs()) {
        internalStatus = DESYNCHRONIZED;
        synchronizer->reset();
        desyncMAC();
        // NOTE: when we desynchronize, we set the correction to zero.
        // This is important, as we may spend an unbounded time desynchronized.
        // Why is this a problem? The virtual clock has separate conversion
        // factors for the forward and backward correction (see factor and
        // inverseFactor in virtual_clock.h). As the time from the last
        // correction passes, the round trip (between a corrected to
        // an uncorrected time and back) conversion error grows, and if its
        // sign is negative, the scheduler timer will no longer recognize the
        // wakeup it has set and won't wake up threads, effectively deadlocking.
        // The same issue has been observed in the nanosecond to tick conversion
        // in timeconversion.cpp. However, while in the tick/ns the issue is
        // unavoidable and the code has been hardened to handle this case,
        // it has been decided that it's too computationally costly to do the
        // same in this case. For this reason, when we desynchronize, we reset
        // clockCorrection to zero and call updateVt(). This resets the
        // conversion coefficents to 1.0 both ways, which is perfectly
        // symmetrical, making the round trip work even for unbounded desyncs.
        clockCorrection = 0;
    } else {
        std::pair<int,int> clockCorrectionReceiverWindow = synchronizer->lostPacket();
        clockCorrection = clockCorrectionReceiverWindow.first;
        receiverWindow = clockCorrectionReceiverWindow.second;
    }
    updateVt();
    return missedPackets;
}

void DynamicTimesyncDownlink::resyncMAC() {
    ctx.getUplink()->resync();
    ctx.getScheduleDistribution()->resync();
    ctx.getDataPhase()->resync();
    ctx.getStreamManager()->resync();
}

void DynamicTimesyncDownlink::desyncMAC() {
#ifdef CRYPTO
    ctx.getKeyManager()->desync();
#endif
    ctx.getUplink()->desync();
    ctx.getScheduleDistribution()->desync();
    ctx.getDataPhase()->desync();
    ctx.getStreamManager()->desync();
}

}
