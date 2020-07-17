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

/**
 * NOTE ON CRYPTOGRAPHY
 *
 * Uplink is the only phase in which calls to encrypt and/or authenticate
 * packets are not done in the phase classes, but hidden in the uplink messages
 * methods: send and recv. The reason for this is that ReceiveUplinkMessage::recv
 * performs sanity checks on the message that can only be done once the packet
 * has already been decrypted (if encryption was enabled). To keep some
 * consistency, the same was done in SendUplinkMessages::send.
 * It is important to note that this implementation difference does not
 * translate in different behavior. Like all other packets, uplink packets are
 * authenticated and/or encrypted if those functionalities are ON in config.
 * The OCB object used is rekeyed in the KeyManager like all others.
 * All packets must always been authenticated and encrypted (or verified and
 * decrypted) AFTER calling setNonce on the OCB object. In the special case of
 * uplink, this means that we must always call send/recv after calling setNonce
 * on the Send/ReceiveUplinkMessage object.
 * This is ugly, but many things are ugly, and there is no time in this world
 * for beauty.
 */

void UplinkPhase::receiveUplink(long long slotStart, unsigned char currentNode)
{
#ifdef CRYPTO
    KeyManager& keyManager = *(ctx.getKeyManager());
    AesOcb& ocb = keyManager.getUplinkOCB();
    ReceiveUplinkMessage message(ctx.getNetworkConfig(), ocb);
    unsigned int masterIndex = keyManager.getMasterIndex();
    unsigned int tileNumber = ctx.getCurrentTile(slotStart);
    if(ctx.getNetworkConfig().getAuthenticateControlMessages()) {
        message.setIV(tileNumber, 1, masterIndex);
    }
#else
    ReceiveUplinkMessage message(ctx.getNetworkConfig());
#endif
    
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

#ifdef CRYPTO
                if(ctx.getNetworkConfig().getAuthenticateControlMessages()) {
                    unsigned int seqNo = i + 1;
                    message.setIV(tileNumber, seqNo, masterIndex);
                }
#endif
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

