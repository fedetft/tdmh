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

#pragma once

#include "timesync_downlink.h"
#include "controller/flopsync2.h"
#include "controller/synchronizer.h"
#include "interfaces-impl/virtual_clock.h"
#include "kernel/timeconversion.h"
#include "../../util/packet.h"
#include <limits>

namespace mxnet {
class DynamicTimesyncDownlink : public TimesyncDownlink {
public:
    DynamicTimesyncDownlink() = delete;
    explicit DynamicTimesyncDownlink(MACContext& ctx) :
            TimesyncDownlink(ctx, DESYNCHRONIZED, std::numeric_limits<unsigned>::max()),
            tc(new miosix::TimeConversion(EFM32_HFXO_FREQ)),
            vt(miosix::VirtualClock::instance()),
            synchronizer(new Flopsync2()),
            measuredFrameStart(0),
            computedFrameStart(0),
            theoreticalFrameStart(0),
            clockCorrection(0),
            missedPackets(0) {
                vt.setSyncPeriod(networkConfig.getClockSyncPeriod());
        };
    DynamicTimesyncDownlink(const DynamicTimesyncDownlink& orig) = delete;
    virtual ~DynamicTimesyncDownlink() {
        delete tc;
        delete synchronizer;
    }
    inline void execute(long long slotStart) override;
    
    virtual long long getSlotframeStart() const override { return measuredFrameStart - (ctx.getHop() - 1) * rebroadcastInterval; }
protected:
    void rebroadcast(const Packet& pkt, long long arrivalTs);
    
    bool isSyncPacket(const Packet& packet, miosix::RecvResult r, bool synchronized) {
        auto panId = networkConfig.getPanId();
        unsigned int packetSize = syncPacketSize;
#ifdef CRYPTO
        /**
         * HACK: this is ugly, but needed because TimesyncDownlink::syncPacketSize
         * must be known at compile time
         * If crypto is on, but we are not authenticating timesync packets, decrease 
         * expected packet size by the size of the authentication tag.
         */
        if (!ctx.getNetworkConfig().getAuthenticateControlMessages()) packetSize -= 16;
#endif
        // Since when resyncing we only have one chance to select our hop
        // be less conservative than the rest of the MAC in the RSSI threshold.
        // The value has been selected as three sigma of the CC2520 RSSI standard
        // deviation in an experimental setup
        const int rssidelta = 5;
        // Ignore low RSSI packets if not syncronyzed
        if(synchronized == false && r.rssi<networkConfig.getMinNeighborRSSI()-rssidelta)
            return false;
        if((r.timestampValid && packet.size() == packetSize
            && packet[0] == 0x46 && packet[1] == 0x08
            && packet[3] == static_cast<unsigned char>(panId >> 8)
            && packet[4] == static_cast<unsigned char>(panId & 0xff)
            && packet[5] == 0xff && packet[6] == 0xff) == false) return false;
        if(synchronized) {
            // If synchronized, the hop can't change
            if(ctx.getHop() != packet[2] + 1) return false;
        } else {
            if(networkConfig.getStaticHop()>0)
            {
                // If not synchronized and forced hop selected, ignore other hops
                if(packet[2] + 1 != networkConfig.getStaticHop()) return false;
            } else {
                // If not synchronized and no forced hop, ignore hops>maxNumHop
                // NOTE: this is important because other phases send packets
                // with the same header which can be disambiguated by the hop field
                if(packet[2] + 1 > networkConfig.getMaxHops()) return false;
            }
        }
        return true;
    }

    /**
     * Since the node is synchronized, it performs the step in the FLOPSYNC-2 controller
     */
    void periodicSync();

    /**
     * Since the node is not synchronizes, it listens to the channel for an undefined time
     * to reinitialize the FLOPSYNC-2 controller
     */
    void resyncTime();

    /**
     * Resets the data calculated by and useful for the controller
     */
    void reset(long long hookPktTime);
    void next() override;
    long long correct(long long int uncorrected) override;

    /**
     * Marks the packet as missed, altering the internal state of the synchronization automaton
     */
    unsigned char missedPacket();

    /**
     * Updates the VirtualClock data to perform corrections based on the freshly calculated FLOPSYNC-2 data
     */
    void updateVt() {
        vt.update(
                tc->ns2tick(theoreticalFrameStart),
                tc->ns2tick(computedFrameStart), clockCorrection
        );
    }

    /**
     * To be called after setHop, every time the MAC resynchronizes
     * calls resync methods in every mac_phase containing some status
     * to avoid working with old data
     * NOTE: used only on dynamic mac_phases since the master node
     * can never be desynchronized since it's the one giving the time.
     */
    void resyncMAC();

    /**
     * To be called every time the MAC desynchronizes
     * calls desync methods in every mac_phase containing some status
     * NOTE: used only on dynamic mac_phases since the master node
     * can never be desynchronized since it's the one giving the time.
     */
    void desyncMAC();

    /**
     * This function needs to be implemented because we are inheriting from
     * MacPhase but is useless since the timesync is the one caling the resync
     * and desync methods, not the other way around.
     */
    void resync() override {}

    /**
     * This function needs to be implemented because we are inheriting from
     * MacPhase but is useless since the timesync is the one caling the resync
     * and desync methods, not the other way around.
     */
    void desync() override {}

    miosix::TimeConversion* const tc;
    miosix::VirtualClock& vt;
    Synchronizer* const synchronizer;

    /**
     * Contains the corrected timestamp of arrival of the synchronization packet.
     * If no timesync packet is received, it is derived from computedFrameStart in missedPacket().
     */
    long long measuredFrameStart;

    /**
     * Contains the calculated timestamp of arrival of the synchronization packet
     * including the clock correction. (uncorrected)
     */
    long long computedFrameStart;

    /**
     * Contains the calculated timestamp of arrival of the synchronization packet,
     * by taking the first timesync as zero and incrementing it every sync period.
     * (uncorrected)
     */
    long long theoreticalFrameStart;

    int clockCorrection;
    unsigned char missedPackets;
};
}

