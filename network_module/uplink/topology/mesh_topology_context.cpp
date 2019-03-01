/***************************************************************************
 *   Copyright (C)  2018 by Polidori Paolo                                 *
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

#include "mesh_topology_context.h"
#include "../../mac_context.h"
#include "../../debug_settings.h"
#include <algorithm>

using namespace miosix;

namespace mxnet {

DynamicMeshTopologyContext::DynamicMeshTopologyContext(MACContext& ctx)  :
    DynamicTopologyContext(ctx),
    neighbors(ctx.getNetworkConfig().getMaxNodes(), std::vector<unsigned char>()) {};

void DynamicMeshTopologyContext::receivedMessage(UplinkMessage msg, unsigned char sender, short rssi) {
    DynamicTopologyContext::receivedMessage(msg, sender, rssi);
    if (rssi >= ctx.getNetworkConfig().getMinNeighborRSSI()) {
        neighbors.addNeighbor(sender);
        neighborsUnseenFor[sender] = 0;
    }
    /* 28/20/2019 Fede&Izzo: the forwarded Topologies and SME
       do not need the check on RSSI because this information is received from other nodes */
    auto it = neighborsUnseenFor.find(sender);
    if (it != neighborsUnseenFor.end())
        it->second = 0;
    auto* tMsg = static_cast<NeighborMessage*>(msg.getTopologyMessage());
    if (it == neighborsUnseenFor.end() || msg.getAssignee() != ctx.getNetworkId())
    {
        tMsg->deleteForwarded();
        delete tMsg;
        return;
    }
    checkEnqueueOrUpdate(new ForwardedNeighborMessage(sender, tMsg->getNeighbors()));
    for (auto elem : tMsg->getForwardedTopologies())
        checkEnqueueOrUpdate(elem);
    delete tMsg;
    return;
}

void DynamicMeshTopologyContext::unreceivedMessage(unsigned char nodeIdByTopologySlot) {
    DynamicTopologyContext::unreceivedMessage(nodeIdByTopologySlot);
    auto it = neighborsUnseenFor.find(nodeIdByTopologySlot);
    if (it == neighborsUnseenFor.end()) return;
    //if it is unseen for less than a configured number of uplink rounds, remove it
    if (++(it->second) < ctx.getNetworkConfig().getMaxRoundsUnavailableBecomesDead()) return;
    neighborsUnseenFor.erase(it);
    neighbors.removeNeighbor(nodeIdByTopologySlot);
}

TopologyMessage* DynamicMeshTopologyContext::getMyTopologyMessage(unsigned char extraTopologies) {
    auto& config = ctx.getNetworkConfig();
    std::vector<ForwardedNeighborMessage*> forwarded;
    // Forward topologies only if we know a predecessor
    if(hasPredecessor())
    {
        auto numTopologies = config.getGuaranteedTopologies() + extraTopologies;
        auto forward = dequeueMessages(numTopologies);
        forwarded.reserve(forward.size());
        std::transform(forward.begin(), forward.end(), std::back_inserter(forwarded), [](TopologyElement* elem){
            return dynamic_cast<ForwardedNeighborMessage*>(elem);
        });
    }
    return new NeighborMessage(neighbors, std::move(forwarded), config);
}

void DynamicMeshTopologyContext::checkEnqueueOrUpdate(ForwardedNeighborMessage* msg) {
    //The node chosen me for forwarding the data
    auto nodeId = msg->getNodeId();
    if (enqueuedTopologyMessages.hasKey(nodeId)) {
        //if i already know the node
        delete enqueuedTopologyMessages.getByKey(nodeId);
        enqueuedTopologyMessages.update(nodeId, msg);
    } else {//new neighbor's data
        enqueuedTopologyMessages.enqueue(nodeId, msg);
    }
}

void MasterMeshTopologyContext::receivedMessage(UplinkMessage msg, unsigned char sender, short rssi) {
    MasterTopologyContext::receivedMessage(msg, sender, rssi);
    /* 28/20/2019 Fede&Izzo: the manageTopologyUpdate does not need the check on RSSI because
       this topology information is received from other nodes */
    auto tMsg = static_cast<NeighborMessage*>(msg.getTopologyMessage());
    manageTopologyUpdate(sender, tMsg->getNeighbors());
    std::vector<ForwardedNeighborMessage*> fwds = tMsg->getForwardedTopologies();
    for (auto* fwd : fwds) {
        manageTopologyUpdate(fwd->getNodeId(), fwd->getNeighbors());
        delete fwd;
    }
    delete tMsg;
}

void MasterMeshTopologyContext::manageTopologyUpdate(unsigned char sender, NeighborTable neighbors) {
    auto newNeighbors = neighbors.getNeighbors();
    auto oldNeighbors = topology.getEdges(sender);
    setDifferenceDo<std::vector<unsigned char>::iterator,
                    std::vector<unsigned char>::iterator>(oldNeighbors.begin(),
                                                          oldNeighbors.end(),
                                                          newNeighbors.begin(),
                                                          newNeighbors.end(),
                                                          [this, sender](unsigned char id) {
            //this is not as easy as that. TODO add check for orphaned links, ex kite with first hop dying
        topology.removeEdge(sender, id);
        if(!topology.hasNode(sender))
            neighborsUnseenFor.erase(sender);
    });
    setDifferenceDo<std::vector<unsigned char>::iterator, std::vector<unsigned char>::iterator>(newNeighbors.begin(), newNeighbors.end(), oldNeighbors.begin(), oldNeighbors.end(), [this, sender](unsigned char id) {
        topology.addEdge(sender, id);
    });
}

void MasterMeshTopologyContext::print() const {
    print_dbg("[U] Current topology @%llu:\n", getTime());
    for (auto it : topology.getEdges())
        print_dbg("[%d - %d]\n", it.first, it.second);
}

}
/* namespace mxnet */
