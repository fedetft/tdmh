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

#include "../debug_settings.h"
#include "dynamic_uplink_phase.h"
#include "../timesync/timesync_downlink.h"
#include "uplink_message.h"
#include <limits>
#include <cassert>

using namespace miosix;

namespace mxnet {

void DynamicUplinkPhase::execute(long long slotStart)
{
    auto address = getAndUpdateCurrentNode();
    
    if (ENABLE_UPLINK_VERB_DBG)
        print_dbg("[U] N=%u T=%lld\n", address, slotStart);
    
    if (address == myId) sendMyUplink(slotStart);
    else receiveUplink(slotStart, address);
}

void DynamicUplinkPhase::receiveUplink(long long slotStart, unsigned char expectedNode)
{
    UplinkMessage message(ctx.getNetworkConfig());
    
    ctx.configureTransceiver(ctx.getTransceiverConfig());
    if(message.recv(ctx,expectedNode))
    {
        auto senderTopology = message.getSenderTopology();
        myNeighborTable.receivedMessage(expectedNode, messsage.getHop(),
                                    message.getRssi(), senderTopology);
        
        if(ENABLE_UPLINK_INFO_DBG)
            print_dbg("[U]<-N=%u @%llu %hddBm\n",expectedNode,message.getTimestamp(),message.getRssi());
        if(ENABLE_TOPOLOGY_SHORT_SUMMARY)
            print_dbg("<-%d %ddBm\n",currentNode,message.getRssi());
    
        if(message.getAssignee() == myId)
        {
            topologyQueue.enqueue(expectedNode,
                std::move(TopologyElement(expectedNode,senderTopology)));
            
            // Code to become a method of UplinkMessage -- begin
            while(message.getNumTopology() > 0)
            {
                auto topology = message.getForwardedTopology();
                topologyQueue.enqueue(topology.getId(),std::move(topology));
            }
            while(message.getNumSME() > 0)
            {
                auto sme = getSME();
                smeQueue.enqueue(sme.getStreamId(),sme);
            }
            // Code to become a method of UplinkMessage -- end
            
            for(unsigned char i = 1; i < numUplinkPackets; i++)
            {
                if(message.recv(ctx,expectedNode) == false) break;
                
                // Code to become a method of UplinkMessage -- begin
                while(message.getNumTopology() > 0)
                {
                    auto topology = message.getForwardedTopology();
                    topologyQueue.enqueue(topology.getId(),std::move(topology));
                }
                while(message.getNumSME() > 0)
                {
                    auto sme = getSME();
                    smeQueue.enqueue(sme.getStreamId(),sme);
                }
                // Code to become a method of UplinkMessage -- end
            }
        }
        
    } else {
        myNeighborTable.missedMessage(expectedNode);
        
        if(ENABLE_TOPOLOGY_SHORT_SUMMARY)
            print_dbg("  %d\n",expectedNode);
    }
    ctx.transceiverIdle();
}

void DynamicUplinkPhase::sendMyUplink(long long slotStart)
{
    UplinkMessage message(ctx.getNetworkConfig(), ctx.getHop(),
                          myNeighborTable.getBestPredecessor(),
                          myNeighborTable.getMyTopologyElement());
    
    int numPackets = message.putTopologiesAndSMEs(topologyQueue,smeQueue);
    
    if(ENABLE_UPLINK_INFO_DBG)
        print_dbg("[U] N=%u -> @%llu\n", ctx.getNetworkId(), slotStart);
    
    ctx.configureTransceiver(ctx.getTransceiverConfig());
    for(int i = 0; i < numPackets; i++)
    {
        message.send(ctx,slotStart);
        slotStart+=packetArrivalAndProcessingTime + transmissionInterval;
    }
    ctx.transceiverIdle();
    
    if(ENABLE_TOPOLOGY_SHORT_SUMMARY)
        print_dbg("->%d\n",ctx.getNetworkId());
}

} // namespace mxnet
