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

#include "schedule_distribution.h"
#include "../scheduler/schedule_computation.h"
#include "../stream/stream_collection.h"

namespace mxnet {

class MasterScheduleDownlinkPhase : public ScheduleDownlinkPhase {
public:
    MasterScheduleDownlinkPhase(MACContext& ctx, ScheduleComputation& sch);
    MasterScheduleDownlinkPhase() = delete;
    MasterScheduleDownlinkPhase(const MasterScheduleDownlinkPhase& orig) = delete;
    virtual ~MasterScheduleDownlinkPhase() {};
    void execute(long long slotStart) override;
    /**
     * Master node do not need resync since it never loses synchronization
     */
    void resync() override {}

    /**
     * Master node do not need desync since it never loses synchronization
     */
    void desync() override {}
    void getCurrentSchedule(long long slotStart);
    void sendSchedulePkt(long long slotstart);
    void sendInfoPkt(long long slotstart);
    void printHeader(ScheduleHeader& header);

private:
    unsigned int getActivationTile(unsigned int currentTile, unsigned int numPackets);
    // Last schedule element sent
    unsigned position = 0;
    unsigned downlinkSlots = 0;
    unsigned packetCapacity = 0;
    bool distributing = false;

    // Reference to ScheduleComputation class to get current schedule
    ScheduleComputation& schedule_comp;
    // Pointer to StreamCollection, used to get info elements to distribute
    StreamCollection* const streamColl;
};

}

