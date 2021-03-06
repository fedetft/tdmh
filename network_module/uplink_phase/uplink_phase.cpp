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

#include "uplink_phase.h"
#include "uplink_message.h"

namespace mxnet {

void UplinkPhase::alignToNetworkTime(NetworkTime nt)
{
    auto controlSuperframeDuration = ctx.getNetworkConfig().getControlSuperframeDuration();
    auto tileDuration = ctx.getNetworkConfig().getTileDuration();
    auto numUplinkPerSuperframe = ctx.getNetworkConfig().getNumUplinkSlotperSuperframe();
    auto controlSuperframe = ctx.getNetworkConfig().getControlSuperframeStructure();
    // NOTE: Add half slot size to make the computation more robust to noise in time
    auto time = nt.get() + ctx.getDataSlotDuration()/2;
    auto superframeCount = time / controlSuperframeDuration;
    auto timeWithinSuperframe = time % controlSuperframeDuration;
    
    //contains the number of uplink phases already executed
    long long phase = superframeCount * numUplinkPerSuperframe;
    
    for(int i = 0; i < controlSuperframe.size(); i++)
    {
        if(timeWithinSuperframe < tileDuration) break;
        timeWithinSuperframe -= tileDuration;
        if(controlSuperframe.isControlUplink(i)) phase++;
    }
    nextNode = nodesCount - 1 - (phase % nodesCount);
}

unsigned char UplinkPhase::getAndUpdateCurrentNode()
{
    auto currentNode = nextNode;
    if (nextNode == 0) nextNode = nodesCount - 1;
    else nextNode--;
    return currentNode;
}


void UplinkPhase::receiveUplink(long long slotStart, unsigned char currentNode)
{
    ReceiveUplinkMessage message(ctx.getNetworkConfig());
    
    ctx.configureTransceiver(ctx.getTransceiverConfig());
    if(message.recv(ctx, slotStart))
    {
        auto numPackets = message.getNumPackets();
        TopologyElement senderTopology = message.getSenderTopology(currentNode);
        myNeighborTable.receivedMessage(message.getHop(), message.getRssi(),
                                    message.getBadAssignee(), senderTopology);
        
        if(ENABLE_UPLINK_DYN_INFO_DBG)
            print_dbg("[U]<-N=%u @%llu %hddBm\n",currentNode,
                    NetworkTime::fromLocalTime(slotStart).get(),message.getRssi());
        if(ENABLE_TOPOLOGY_DYN_SHORT_SUMMARY)
            print_dbg("<-%d %ddBm\n",currentNode,message.getRssi());
    
        if(message.getAssignee() == myId)
        {
            topologyQueue.enqueue(currentNode, std::move(senderTopology));
            message.deserializeTopologiesAndSMEs(topologyQueue, smeQueue);
            
            for(int i = 1; i < numPackets; i++)
            {
                // NOTE: If we fail to receive a Packet of the UplinkMessage,
                // do not wait for remaining packets
                // TODO verify that packetArrivalAndProcessingTime + transmissionInterval are correct
                slotStart += packetArrivalAndProcessingTime + transmissionInterval;
                if(message.recv(ctx, slotStart) == false) break;
                message.deserializeTopologiesAndSMEs(topologyQueue, smeQueue);
            }
        }
        
    } else {
        myNeighborTable.missedMessage(currentNode);
        
        if(ENABLE_TOPOLOGY_DYN_SHORT_SUMMARY)
            print_dbg("  %d\n",currentNode);
    }
    ctx.transceiverIdle();
}
} // namespace mxnet

