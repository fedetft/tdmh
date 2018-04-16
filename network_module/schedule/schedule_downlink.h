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

#include "interfaces-impl/transceiver.h"
#include "interfaces-impl/power_manager.h"
#include <utility>
#include "../mac_phase.h"
#include "schedule_context.h"
#include "../mac_context.h"

namespace mxnet {
class ScheduleDownlinkPhase : public MACPhase {
public:
    ScheduleDownlinkPhase() = delete;
    ScheduleDownlinkPhase(const ScheduleDownlinkPhase& orig) = delete;
    virtual ~ScheduleDownlinkPhase() {};
    unsigned long long getDuration() override {
        return networkConfig.getScheduleDownlinkPerSlotframeCount() * (phaseStartupTime + networkConfig.getMaxHops() * rebroadcastInterval);
    }
    static const int phaseStartupTime = 450000;
    static const int rebroadcastInterval = 5000000; //32us per-byte + 600us total delta
    
protected:
    ScheduleDownlinkPhase(MACContext& ctx) :
        MACPhase(ctx),
        networkConfig(ctx.getNetworkConfig()),
        scheduleContext(ctx.getScheduleContext()) {};
    
    const NetworkConfiguration& networkConfig;
    ScheduleContext* const scheduleContext;
};
}

