/***************************************************************************
 *   Copyright (C)  2018, 2019 by Polidori Paolo, Federico Amedeo Izzo,    *
 *                                Federico Terraneo                        *
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

#include "uplink_phase.h"
#include "../scheduler/schedule_computation.h"

namespace mxnet {

/**
 * The MasterUplinkPhase is used to implement the uplink phase by the master node
 */
class MasterUplinkPhase : public UplinkPhase
{
public:
    MasterUplinkPhase(MACContext& ctx, StreamManager* const streamMgr,
                      ScheduleComputation& scheduleComputation) :
        UplinkPhase(ctx, streamMgr),
        scheduleComputation(scheduleComputation){
        scheduleComputation.setUplinkPhase(this);
    }
    
    /**
     * Receives topology and SME from the other nodes
     */
    virtual void execute(long long slotStart) override;

    bool wasModified() {
        return true;
    }

    NetworkGraph getTopologyMap() {
        NetworkGraph result(1);
        return result;
    }

    void clearModifiedFlag();


private:
    ScheduleComputation& scheduleComputation;
    NetworkTopology topology;    
};

} // namespace mxnet
