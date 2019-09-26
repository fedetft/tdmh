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

namespace mxnet {

class DynamicScheduleDownlinkPhase : public ScheduleDownlinkPhase {
public:
    DynamicScheduleDownlinkPhase(MACContext& ctx) : ScheduleDownlinkPhase(ctx),
                                                    myId(ctx.getNetworkId()) {};
    DynamicScheduleDownlinkPhase() = delete;
    DynamicScheduleDownlinkPhase(const DynamicScheduleDownlinkPhase& orig) = delete;
    void execute(long long slotStart) override;
    /**
     * Reset the internal status of the ScheduleDownlinkPhase after resynchronization
     */
    void resync() override {
        // Base class status
        header = ScheduleHeader();
        schedule.clear();
        infos.clear();
        explicitScheduleID = 0;
        explicitSchedule.clear();
        // Derived class status
        received.clear();
    };
    /**
     * This method is called after desynchronization
     */
    void desync() override {}

private:
    void extractInfoElements(SchedulePacket& spkt);
    void printHeader(ScheduleHeader& header);
    bool isScheduleComplete();
    void printStatus();

    /* NetworkId of this node */
    unsigned char myId;

    // Vector of bool with size = total packets of a schedule
    // To check if all the schedule packets are received
    std::vector<bool> received;
    /* NOTE: we don't need to save here a copy of the current schedule
       being distributed, as we need only the copy in the base class.
       This is because the master can produce a new schedule only after
       the previous one has been activated */
};

}
