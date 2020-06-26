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

#include "master_uplink_phase.h"
#include "uplink_message.h"
#include "../util/debug_settings.h"
#include <limits>

using namespace miosix;

namespace mxnet {

void MasterUplinkPhase::execute(long long slotStart)
{
    auto currentNode = getAndUpdateCurrentNode();
    
    if(ENABLE_UPLINK_VERB_DBG)
        print_dbg("[U] N=%u NT=%lld\n", currentNode, NetworkTime::fromLocalTime(slotStart).get());

    if (currentNode == myId) sendMyUplink(slotStart);
    else receiveUplink(slotStart, currentNode);

    // Consume elements from the topology queue
    topology.handleTopologies(topologyQueue);
    // Enqueue SMEs produced by the Master node itself
    streamMgr->dequeueSMEs(smeQueue);
    // Consume elements from the SME queue
#ifdef CRYPTO
    streamColl->receiveSMEs(smeQueue, *(ctx.getKeyManager()));
#else
    streamColl->receiveSMEs(smeQueue);
#endif
    
    #ifndef _MIOSIX
    if(ENABLE_TOPOLOGY_INFO_DBG)
    {
        auto graph=topology.getGraph();
        print_dbg("[U] Current topology @%lld:\n",
            NetworkTime::fromLocalTime(slotStart).get());
        for(auto it : graph.getEdges())
            print_dbg("[%d - %d]\n", it.first, it.second);
    }
    #else
    static_assert(ENABLE_TOPOLOGY_INFO_DBG==false,"ENABLE_TOPOLOGY_INFO_DBG can be used only in the simulator");
    #endif
}

void MasterUplinkPhase::sendMyUplink(long long slotStart)
{
#ifdef CRYPTO
    KeyManager& keyManager = *(ctx.getKeyManager());
    unsigned int masterIndex = keyManager.getMasterIndex();
    unsigned int tileNumber = ctx.getCurrentTile(slotStart);
#endif
    SendUplinkMessage message(ctx.getNetworkConfig(), 0, false, ctx.getNetworkId(),
                              myNeighborTable.getMyTopologyElement(),
                              0, 0
#ifdef CRYPTO
                              , keyManager.getUplinkGCM()
#endif
                              );
    if(ENABLE_UPLINK_DYN_INFO_DBG)
        print_dbg("[U] N=%u -> @%llu\n", ctx.getNetworkId(), NetworkTime::fromLocalTime(slotStart).get());

#ifdef CRYPTO
    if(ctx.getNetworkConfig().getAuthenticateControlMessages()) {
        message.setIV(tileNumber, 1, masterIndex);
    }
#endif
    ctx.configureTransceiver(ctx.getTransceiverConfig());
    message.send(ctx,slotStart);
    ctx.transceiverIdle();

    //send my topology to myself
    TopologyElement myTopology = myNeighborTable.getMyTopologyElement();
    topologyQueue.enqueue(0, std::move(myTopology));

    if(ENABLE_TOPOLOGY_DYN_SHORT_SUMMARY)
        print_dbg("->%d\n",ctx.getNetworkId());
    if(ENABLE_UPLINK_DBG){
        message.printHeader();
    }
}

} // namespace mxnet
