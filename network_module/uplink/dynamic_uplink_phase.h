/***************************************************************************
 *   Copyright (C)  2019 by Polidori Paolo, Federico Amedeo Izzo,          *
 *                          Federico Terraneo                              *
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
#include "../updatable_queue.h"
#include "neighbor_table.h"

namespace mxnet {

/** The DynamicUplinkPhase is used to collect the network topology and Stream
 *  Management Elements from the dynamic nodes and forward it to the master node.
 *
 *  The collection is done in a round-robin fashion, the getAndUpdateCurrentNode()
 *  method of the base class is called at every round to determine which node
 *  is transmitting on the current round, that node will send its Uplink Messages
 *  with sendMyUplinkMessage, all the other nodes will listen for incoming
 *  Uplink Messages
 */
class DynamicUplinkPhase : public UplinkPhase {
public:
    DynamicUplinkPhase(MACContext& ctx, StreamManager* const streamMgr) :
        UplinkPhase(ctx, streamMgr) {}
    virtual ~DynamicUplinkPhase() {};

    /**
     * Calls getAndUpdateCurrentNode() from the base class to check if it's our turn
     * to transmit the UplinkMessage, and depending on the returned nodeID, we call
     * sendMyUplinkMessage() or receiveUplinkMessage().
     */
    virtual void execute(long long slotStart) override;

    /**
     * Starts expecting a message from the node to which the slot is assigned
     * and modifies the TopologyContext as needed.
     */
    void receiveUplinkMessage(long long slotStart, unsigned char currentNode);

    /**
     * Called when it's our turn to transmit in the round-robin.
     * It sends the UplinkMessage containing our local TopologyElement and SMEs
     * together with forwarded TopologyElements and SMEs.
     */
    void sendMyUplinkMessage(long long slotStart);

private:
    UpdatableQueue<TopologyElement> topologyQueue;
    UpdatableQueue<StreamManagementElement> smeQueue;
    NeighborTable myNeighborTable;
};

} /* namespace mxnet */
