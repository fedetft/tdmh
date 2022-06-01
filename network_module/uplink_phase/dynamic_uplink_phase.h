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

#include "../network_configuration.h"
#include "uplink_phase.h"

#ifdef PROPAGATION_DELAY_COMPENSATION
#include "delay_compensation_filter.h"
#endif

namespace mxnet {

/**
 * The DynamicUplinkPhase is used to implement the uplink phase by all nodes
 * in the network except the master.
 */
class DynamicUplinkPhase : public UplinkPhase
{
public:
    DynamicUplinkPhase(MACContext& ctx, StreamManager* const streamMgr) :
        UplinkPhase(ctx, streamMgr) {};

    /**
     * Calls getAndUpdateCurrentNode() from the base class to check if it's our turn
     * to transmit the UplinkMessage, and depending on the returned nodeID, we call
     * sendMyUplinkMessage() or receiveUplinkMessage().
     */
    void execute(long long slotStart) override;

    /**
     * Reset the internal status of the UplinkPhase after resynchronization
     */
    void resync() override {
        // Base class status
        nextNode = nodesCount - 1;
        // Derived class status
        topologyQueue.clear();
        smeQueue.clear();
        myNeighborTable.clear(ctx.getHop());
    };

    /**
     * Called after losing the Timesync synchronization
     */
    void desync() override
    {
#ifdef PROPAGATION_DELAY_COMPENSATION
        compFilter.reset();
#endif
    }

    /**
     * Called when it's our turn to transmit in the round-robin.
     * It sends the UplinkMessage containing our local TopologyElement and SMEs
     * together with forwarded TopologyElements and SMEs.
     * /return timestamp of the last send message
     */
    long long sendMyUplink(long long slotStart);


#ifdef PROPAGATION_DELAY_COMPENSATION
    /**
     * Called after uplink messages are received from other nodes
     */
    void sendDelayCompensationLedBar(long long sendTime);

    /**
     * Called after uplink messages are send in the uplink phase reserved to this node
     */
    void recvDelayCompensationLedBar(long long sentTimestamp, long long tExpected);

    /**
     * Getter for filtered compensation delay
     */
    int getCompensationDelayFromMaster();
#endif


private:

#ifdef PROPAGATION_DELAY_COMPENSATION
    DelayCompensationFilter compFilter;
#endif
};

} // namespace mxnet
