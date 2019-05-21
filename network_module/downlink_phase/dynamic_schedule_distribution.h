/***************************************************************************
 *   Copyright (C)  2017 by Federico Amedeo Izzo                           *
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

namespace mxnet {

class DynamicScheduleDownlinkPhase : public ScheduleDownlinkPhase {
public:
    DynamicScheduleDownlinkPhase(MACContext& ctx) : ScheduleDownlinkPhase(ctx),
                                                    streamMgr(ctx.getStreamManager()) {};
    DynamicScheduleDownlinkPhase() = delete;
    DynamicScheduleDownlinkPhase(const DynamicScheduleDownlinkPhase& orig) = delete;
    void execute(long long slotStart) override;
    /**
     * Reset the internal status of the ScheduleDownlinkPhase after resynchronization
     */
    void reset() override {
        // Base class status
        header = ScheduleHeader();
        schedule.clear();
        explicitScheduleID = 0;
        explicitSchedule.clear();
        distributing = false;
        // Derived class status
        received.clear();
        replaceCountdown = 5;
        nextHeader = ScheduleHeader();
        nextSchedule.clear();
    };

    virtual ~DynamicScheduleDownlinkPhase() {};
    ScheduleHeader decodePacket(Packet& pkt);
    void printHeader(ScheduleHeader& header);
    void calculateCountdown(ScheduleHeader& newHeader);
    bool isScheduleComplete();
    void replaceRunningSchedule();
    void printStatus();

private:
    // Vector of bool with size = total packets of a schedule
    // To check if all the schedule packets are received
    std::vector<bool> received;
    // Countdown for replacing current schedule
    // Initial value = 5, Replaced if = 0
    unsigned int replaceCountdown = 5;
    // Next schedule being received
    ScheduleHeader nextHeader;
    std::vector<ScheduleElement> nextSchedule;
    // Pointer to StreamManager, to deliver info elements
    StreamManager* const streamMgr;
};

}
