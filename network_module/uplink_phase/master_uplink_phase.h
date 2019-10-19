/***************************************************************************
 *   Copyright (C) 2018-2019 by Polidori Paolo, Terraneo Federico,         *
 *   Federico Amedeo Izzo                                                  *
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
#include "topology/network_topology.h"
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
        streamColl(scheduleComputation.getStreamCollection()),
        topology(ctx.getNetworkConfig()) {
        scheduleComputation.setTopology(&topology);
    }
    
    /**
     * Receives topology and SME from the other nodes
     */
    void execute(long long slotStart) override;

    /**
     * Master node do not need resync since it never loses synchronization
     */
    void resync() override {}

    /**
     * Master node do not need desync since it never loses synchronization
     */
    void desync() override {}

    /**
     * Called when it's our turn to transmit in the round-robin.
     * It sends the UplinkMessage containing our local TopologyElement and SMEs
     * together with forwarded TopologyElements and SMEs.
     */
    void sendMyUplink(long long slotStart);
    
private:
    StreamCollection* const streamColl;
    NetworkTopology topology;    
};

} // namespace mxnet
