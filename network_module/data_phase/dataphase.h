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

#pragma once

#include "../mac_phase.h"
#include "../mac_context.h"
#include "../downlink_phase/schedule_distribution.h"
#include "../downlink_phase/timesync/networktime.h"
#include "../stream/stream_manager.h"
#include "../util/align.h"

namespace mxnet {
/**
 * Represents the data phase, which transfers data from the streams among the nodes
 * in a TDMA way, by following the schedule that has been distributed.
 */

class DataPhase : public MACPhase {
public:
    DataPhase(MACContext& ctx, StreamManager& str) : MACPhase(ctx),
                                                     panId(ctx.getNetworkConfig().getPanId()),
                                                     myId(ctx.getNetworkId()),
                                                     stream(str) {};
    
    virtual ~DataPhase() {}

    /* Called during data slots, it does the schedule playback,
       in fact, it executes the action written in the explicit schedule slot
       corresponding to the current tile slot */
    virtual void execute(long long slotStart) override;
    /* Called instead of DataPhase::execute() when the node
       is not synchronized, to keep track with the current tile slot */
    void advance(long long slotStart) override {
        incrementSlot();
    }
    /* Called after Downlink and Uplink phases to update the tileSlot counter */
    void advanceBy(unsigned int slots) {
        incrementSlot(slots);
    }
    static unsigned long long getDuration() {
        long long processingTime=1500000; //TODO: benchmark
        return align(MACContext::radioTime(MediumAccessController::maxDataPktSize)+processingTime,1000000LL);
    }
    /**
     * Reset the internal status of the DataPhase after resynchronization
     * to avoid playback of an old schedule
     */
    void resync() override {
        slotIndex = 0;
        scheduleID = 0;
        scheduleTiles = 0;
        scheduleSlots = 0;
        currentSchedule.clear();
    };
    /**
     * Called after desynchronization
     */
    void desync() override {}

    /* Five possible actions, as described by the explicit schedule */
    void sleep(long long slotStart);
    void sendFromStream(long long slotStart, StreamId id);
    void receiveToStream(long long slotStart, StreamId id);
    void sendFromBuffer(long long slotStart, std::shared_ptr<Packet> buffer);
    void receiveToBuffer(long long slotStart, std::shared_ptr<Packet> buffer);
    /* Called from ScheduleDownlinkPhase class on the first downlink slot
     * of the new schedule, to replace the currentSchedule,
     * taking effect in the next dataphase */
    void applySchedule(const std::vector<ExplicitScheduleElement>&& newSchedule,
                       unsigned long newId, unsigned int newScheduleTiles,
                       unsigned long newActivationTile, unsigned int currentTile) {
        currentSchedule = std::move(newSchedule);
        setScheduleID(newId);
        setScheduleTiles(newScheduleTiles);
        slotIndex = 0;
        if(newActivationTile == currentTile) {
            print_dbg("[D] Schedule ID:%lu, StartTile:%lu activated at tile:%2u\n",
                      newId, newActivationTile, currentTile);
        }
        else {
            // Calculate current tileslot for a late schedule
            // NOTE: tileSlot MUST be at the beginning of a tile
            // (0 + scheduleTile * slotsInTile), with scheduleTile < scheduleSize
            unsigned int scheduleTile = currentTile - newActivationTile;
            unsigned int slotsInTile = ctx.getSlotsInTileCount();
            incrementSlot(scheduleTile * slotsInTile);
            print_dbg("[D] Schedule ID:%lu, StartTile:%lu activated late at tile:%2u\n",
                      newId, newActivationTile, currentTile);
        }
    }
    /* Called from ScheduleDownlinkPhase class to check if the schedule is up to date */
    unsigned long getScheduleID() {
        return scheduleID;
    }

private:
    void incrementSlot(unsigned int n = 1) {
        // Make sure that tileSlot is always in range {0;scheduleSlots}
        if(scheduleSlots != 0)
            slotIndex = (slotIndex + n) % scheduleSlots;
        else
            slotIndex = 0;
    }
    // Check streamId inside packet without extracting it
    bool checkStreamId(Packet pkt, StreamId streamId);

    /* Sets the schedule lenght or DataSuperframeSize */
    void setScheduleTiles(unsigned int newScheduleTiles) {
        scheduleTiles = newScheduleTiles;
        auto slotsInTile = ctx.getSlotsInTileCount();
        scheduleSlots = scheduleTiles * slotsInTile;
    }
    /* Sets the new scheduleID */
    void setScheduleID(unsigned long newID) {
        scheduleID = newID;
    }


    /* Constant value from NetworkConfiguration */
    const unsigned short panId;
    /* NetworkId of this node */
    unsigned char myId;

    // Reference to Stream class, to get packets from stream buffers
    StreamManager& stream;
    unsigned short slotIndex = 0;
    unsigned long scheduleID = 0;
    unsigned long scheduleTiles = 0;
    unsigned long scheduleSlots = 0;
    std::vector<ExplicitScheduleElement> currentSchedule;
};

}

