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
#include "../mac_phase.h"
#include "../mac_context.h"
#include "../downlink_phase/timesync/networktime.h"
#include "../stream/stream_manager.h"
#include "../util/updatable_queue.h"
#include "topology/topology_element.h"
#include "topology/neighbor_table.h"

namespace mxnet {

/**
 * The UplinkPhase is used to collect the network topology and Stream Management
 * Elements.
 *
 * The collection is done in a round-robin fashion, at every round a node will
 * send its Uplink Messages, all the other nodes will listen for incoming
 * Uplink Messages constructing their neighbor tables and forwarding them to the
 * master.
 */
class UplinkPhase : public MACPhase
{
public:
    UplinkPhase(const UplinkPhase&) = delete;
    UplinkPhase& operator=(const UplinkPhase&) = delete;

    /**
     * \return the duration in nanoseconds of an uplink slot
     */
    static unsigned long long getDuration(const NetworkConfiguration& netConfig)
    {   
        unsigned char numUplinkPackets = netConfig.getNumUplinkPackets();
        unsigned long long duration = (packetArrivalAndProcessingTime + transmissionInterval) * numUplinkPackets;
#ifdef CRYPTO
        //NOTE: assuming maxControlPktSize is equal to maxDataPktSize == 125
        //NOTE: time reported here is maximum between tx and rx side as when
        //sending multiple packets encryption in the sender node and decryption
        //in the receiving node occur in parallel
        //NOTE: account for presence/absence of print_dbg. The time is actually
        //even lower if encryption is disabled
        if(ENABLE_CRYPTO_UPLINK_DBG)
            duration += 342000 * numUplinkPackets + 272000;
        else
            duration += 197000 * numUplinkPackets + 179000;
#endif	

#ifdef	PROPAGATION_DELAY_COMPENSATION
        //to do: compute new uplink phase duration
        duration += (packetArrivalAndProcessingTime + transmissionInterval)*2;
#endif

        return duration;
    }

    /**
     * Align uplink phase to the network time when (re)synchronizing
     */
    void alignToNetworkTime(NetworkTime nt);

    /**
     * Called when the node clock synchronization error is too high to operate
     * but the node is not desynchronized. ITs purpose is to update the phase
     * state without causing packet transmissions/receptions.
     */
    void advance(long long slotStart) override { getAndUpdateCurrentNode(); }
    
    //TODO: check the duration calculation, it is currently hardcoded
    static const int transmissionInterval = 1000000; //1ms
    static const int packetArrivalAndProcessingTime = 5000000;//32 us * 133 B + tp = 5ms

protected:
    UplinkPhase(MACContext& ctx, StreamManager* const streamMgr) :
            MACPhase(ctx),
            streamMgr(streamMgr),
            myId(ctx.getNetworkId()),
            nodesCount(ctx.getNetworkConfig().getMaxNodes()),
            nextNode(nodesCount - 1),
            myNeighborTable(ctx.getNetworkConfig(),
                            ctx.getNetworkId(),
                            ctx.getHop()) {}
    

    /**
     * Starts expecting a message from the node to which the slot is assigned
     * and modifies the TopologyContext as needed.
     * \return a pair containig the hop of the sender(first) and the timestamp of the last received message(second)
     */
    std::pair<int, long long> receiveUplink(long long slotStart, unsigned char currentNode);

    /**
     * Called at every execute() or advance() updates the state of the
     * round-robin scheme used for uplink.
     * \return which node id is expected to transmit in this uplink
     */
    unsigned char getAndUpdateCurrentNode();
    
    StreamManager* const streamMgr; ///< Used to get SMEs
    const unsigned char myId;       ///< Cached NetworkId of this node
    const unsigned char nodesCount; ///< Cached NetworkConfiguration::getMaxNodes()
    
    unsigned char nextNode;         ///< Next node to talk in the round-robin
    // Queues used in dynamic nodes to collect and forward topologies and sme
    // and in master node to process received topologies and sme
    UpdatableQueue<unsigned char,TopologyElement> topologyQueue;
    UpdatableQueue<SMEKey,StreamManagementElement> smeQueue;
    NeighborTable myNeighborTable;


};

} // namespace mxnet
