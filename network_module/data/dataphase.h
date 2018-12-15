/***************************************************************************
 *   Copyright (C)  2018 by Polidori Paolo                                 *
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
#include "../schedule/schedule_distribution.h"
#include "../timesync/networktime.h"
#include <map>

namespace mxnet {
/**
 * Represents the data phase, which, divided by slots, is used to move the upper layer data among the nodes,
 * as a previously received schedule devises.
 */
class DataPhase : public MACPhase {
public:
    DataPhase(MACContext& ctx) :
        MACPhase(ctx), slotsInFrame(0), dataSlot(std::numeric_limits<unsigned short>::max() - 1),
        scheduleDownlink(ctx.getScheduleDownlink()),
        queues({{10, std::vector<unsigned char>(0)}, {11, std::vector<unsigned char>(0)}, {100, std::vector<unsigned char>(0)},
        {101, std::vector<unsigned char>(0)}, {20, std::vector<unsigned char>(0)}, {21, std::vector<unsigned char>(0)},
        {30, std::vector<unsigned char>(0)}, {31, std::vector<unsigned char>(0)}}) {}
    
    virtual ~DataPhase() {}

    void setDataSuperframeSize(unsigned short slotsInFrame) {
        this->slotsInFrame = slotsInFrame;
    }

    /**
     * Align data phase to the network time
     */
    void alignToNetworkTime(NetworkTime nt);
    
    virtual void execute(long long slotStart) override;

    void advance(long long slotStart) override;

    static unsigned long long getDuration() {
        return packetArrivalAndProcessingTime + transmissionInterval;
    }

    static const int transmissionInterval = 1000000; //1ms
    static const int packetArrivalAndProcessingTime = 5000000;//32 us * 127 B + tp = 5ms
    static const int packetTime = 4256000;//physical time for transmitting/receiving the packet: 4256us

private:
    DataPhase(const DataPhase& orig) = delete;
    DataPhase& operator= (const DataPhase& orig) = delete;
    
    void nextSlot() {
        if (++dataSlot >= slotsInFrame) {
            dataSlot = 0;
            //curSched = scheduleDownlink->getFirstSchedule();
        }
    }
    unsigned slotsInFrame;
    unsigned short dataSlot;
    ScheduleDownlinkPhase* const scheduleDownlink;
    //std::set<DynamicScheduleElement*>::iterator curSched;
    //TODO remove
    std::map<unsigned short, std::vector<unsigned char>> queues;
};
}

