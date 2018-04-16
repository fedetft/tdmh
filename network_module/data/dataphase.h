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
#include "../schedule/schedule_downlink.h"
#include "../mac_context.h"

namespace mxnet {
class DataPhase : public MACPhase {
public:
    DataPhase(MACContext& ctx, unsigned short slotsInFrame) :
            MACPhase(ctx), slotsInFrame(slotsInFrame), scheduleDownlink(ctx.getScheduleDownlink()) {};
    DataPhase(const DataPhase& orig) = delete;
    virtual ~DataPhase() {};

    virtual void execute(long long slotStart) override;
    unsigned long long getDuration() override {
        return packetArrivalAndProcessingTime + transmissionInterval;
    }

    static unsigned long long getDurationStatic() {
        return packetArrivalAndProcessingTime + transmissionInterval;
    }

    static const int transmissionInterval = 1000000; //1ms
    static const int packetArrivalAndProcessingTime = 5000000;//32 us * 127 B + tp = 5ms
    static const int packetTime = 4256000;//physical time for transmitting/receiving the packet: 4256us
private:
    void nextSlot() {
        if (++dataSlot > slotsInFrame) {
            dataSlot = 0;
            nextSched = scheduleDownlink->getFirstSchedule();
        }
    }
    const unsigned short slotsInFrame;
    unsigned short dataSlot;
    ScheduleDownlinkPhase* const scheduleDownlink;
    std::set<DynamicScheduleElement*>::iterator nextSched;
};
}

