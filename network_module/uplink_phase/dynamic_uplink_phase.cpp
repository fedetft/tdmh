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

#include "../util/debug_settings.h"
#include "dynamic_uplink_phase.h"
#include "../downlink_phase/timesync/timesync_downlink.h"
#include "uplink_message.h"
#include <limits>
#include <cassert>
#ifdef CRYPTO
#include "../crypto/key_management/key_manager.h"
#include "../crypto/aes_gcm.h"
#endif

using namespace miosix;

namespace mxnet {

void DynamicUplinkPhase::execute(long long slotStart)
{
    auto currentNode = getAndUpdateCurrentNode();
    
    if (ENABLE_UPLINK_DYN_VERB_DBG)
         print_dbg("[U] N=%u NT=%lld\n", currentNode, NetworkTime::fromLocalTime(slotStart).get());
    
    if (currentNode == myId) sendMyUplink(slotStart);
    else receiveUplink(slotStart, currentNode);
}

void DynamicUplinkPhase::sendMyUplink(long long slotStart)
{
#ifdef CRYPTO
    KeyManager& keyManager = *(ctx.getKeyManager());
    unsigned int masterIndex = keyManager.getMasterIndex();
    unsigned int tileNumber = ctx.getCurrentTile(slotStart);
#endif
    /* If we don't have a predecessor, we send an uplink message without
       SMEs or topologies with myID as assignee, to speed up the topology
       collection */
    if(!myNeighborTable.hasPredecessor()) {
        SendUplinkMessage message(ctx.getNetworkConfig(), ctx.getHop(),
                                  myNeighborTable.isBadAssignee(), ctx.getNetworkId(), //NOTE: why the network id?
                                  myNeighborTable.getMyTopologyElement(),
                                  0, 0
#ifdef CRYPTO
                                  , keyManager.getUplinkGCM()
#endif
                                  );
        if(ENABLE_UPLINK_DYN_INFO_DBG)
            print_dbg("[U] N=%u -> @%llu\n", ctx.getNetworkId(), NetworkTime::fromLocalTime(slotStart).get());

        ctx.configureTransceiver(ctx.getTransceiverConfig());
#ifdef CRYPTO
        if(ctx.getNetworkConfig().getAuthenticateControlMessages()) {
            message.setIV(tileNumber, 1, masterIndex);
        }
#endif
        message.send(ctx,slotStart);
        ctx.transceiverIdle();
        if(ENABLE_UPLINK_DBG)
            print_dbg("[U] No predecessor\n");
    
        if(ENABLE_TOPOLOGY_DYN_SHORT_SUMMARY)
            print_dbg("->%d\n",ctx.getNetworkId());

        if(ENABLE_UPLINK_DBG){
            message.printHeader();
        }
    }

    /* Otherwise pick the best predecessor and send enqueued SMEs and topologies */
    else {
        streamMgr->dequeueSMEs(smeQueue);
        SendUplinkMessage message(ctx.getNetworkConfig(), ctx.getHop(),
                                  myNeighborTable.isBadAssignee(),
                                  myNeighborTable.getBestPredecessor(),
                                  myNeighborTable.getMyTopologyElement(),
                                  topologyQueue.size(), smeQueue.size()
#ifdef CRYPTO
                                  , keyManager.getUplinkGCM()
#endif
                                  );
        if(ENABLE_UPLINK_DBG) {
            if( myNeighborTable.bestPredecessorIsBad() ) {
                print_dbg("[U] Assignee chosen is bad\n");
            }
        }
        if(ENABLE_UPLINK_DYN_INFO_DBG)
            print_dbg("[U] N=%u -> @%llu\n", ctx.getNetworkId(), NetworkTime::fromLocalTime(slotStart).get());

        ctx.configureTransceiver(ctx.getTransceiverConfig());
        for(int i = 0; i < message.getNumPackets(); i++)
            {
                message.serializeTopologiesAndSMEs(topologyQueue,smeQueue);
#ifdef CRYPTO
                if(ctx.getNetworkConfig().getAuthenticateControlMessages()) {
                    unsigned int seqNo = i + 1;
                    message.setIV(tileNumber, seqNo, masterIndex);
                }
#endif
                message.send(ctx,slotStart);
                slotStart += packetArrivalAndProcessingTime + transmissionInterval;
            }
        ctx.transceiverIdle();
        if(ENABLE_UPLINK_DBG){
            message.printHeader();
        }
    
        if(ENABLE_TOPOLOGY_DYN_SHORT_SUMMARY)
            print_dbg("->%d\n",ctx.getNetworkId());
    }
}

} // namespace mxnet
