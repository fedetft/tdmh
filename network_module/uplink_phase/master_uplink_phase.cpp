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

#include "master_uplink_phase.h"
#include "../util/debug_settings.h"
#include <limits>

using namespace miosix;

namespace mxnet {

void MasterUplinkPhase::execute(long long slotStart)
{
    auto currentNode = getAndUpdateCurrentNode();
    
    if(ENABLE_UPLINK_VERB_DBG)
        print_dbg("[U] N=%u T=%lld\n", currentNode, slotStart);
    
    ReceiveUplinkMessage message(ctx.getNetworkConfig());
    
    ctx.configureTransceiver(ctx.getTransceiverConfig());
    if(message.recv(ctx, slotStart))
    {
        auto numPackets = message.getNumPackets();
        topology.receivedMessage(currentNode, message.getHop(),
                                 message.getRssi(), message.getSenderTopology());
        
        if(ENABLE_UPLINK_INFO_DBG)
            print_dbg("[U]<-N=%u @%llu %hddBm\n",currentNode,message.getTimestamp(),message.getRssi());
        if(ENABLE_TOPOLOGY_SHORT_SUMMARY)
            print_dbg("<-%d %ddBm\n",currentNode,message.getRssi());
    
        if(message.getAssignee() == myId)
        {
            message.deserializeTopologiesAndSMEs(topologyQueue, smeQueue);
            for(int i = 1; i < numPackets; i++)
            {
                // NOTE: If we fail to receive a Packet of the UplinkMessage,
                // do not wait for remaining packets
                // TODO: do recv at next uplink slot (slotstart + slot duration)
                if(message.recv(ctx, slotStart) == false) break;
                message.deserializeTopologiesAndSMEs(topologyQueue, smeQueue);
            }
        }
        // Receive topology and smes from the network
        topology.handleForwardedTopologies(topologyQueue);
        scheduleComputation.receiveSMEs(smeQueue);
        
    } else {
        topology.missedMessage(currentNode);
        
        if(ENABLE_TOPOLOGY_SHORT_SUMMARY)
            print_dbg("  %d\n",currentNode);
    }
    ctx.transceiverIdle();
    
    if(ENABLE_TOPOLOGY_INFO_DBG && currentNode == 1)
    {
        print_dbg("[U] Current topology @%llu:\n", getTime());
        for (auto it : topology.getEdges())
            print_dbg("[%d - %d]\n", it.first, it.second);
    }
}

} // namespace mxnet
