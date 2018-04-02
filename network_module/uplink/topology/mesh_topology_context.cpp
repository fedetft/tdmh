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
#include <algorithm>

namespace mxnet {
void DynamicMeshTopologyContext::receivedMessage(UplinkMessage msg, unsigned char sender, short rssi) {
    DynamicTopologyContext::receivedMessage(msg, sender, rssi);
    neighbors.addNeighbor(sender);
    neighborsUnseenFor[sender] = 0;
    if (msg.getAssignee() != ctx.getNetworkId()) return;
    auto* tMsg = dynamic_cast<NeighborMessage*>(msg.getTopologyMessage());
    checkEnqueueOrUpdate(new ForwardedNeighborMessage(sender, tMsg->getNeighbors()));
    for (auto elem : tMsg->getForwardedTopologies())
        checkEnqueueOrUpdate(elem);
    delete tMsg;
    return;
}

void DynamicMeshTopologyContext::unreceivedMessage(unsigned char nodeIdByTopologySlot) {
    DynamicTopologyContext::unreceivedMessage(nodeIdByTopologySlot);
    auto it = map.find(nodeIdByTopologySlot);
    if (it == neighborsUnseenFor.end()) return;
    neighborsUnseenFor.erase(it);
    neighbors.removeNeighbor(nodeIdByTopologySlot);
}

std::vector<ForwardedNeighborMessage*> DynamicMeshTopologyContext::dequeueMessages(unsigned short count) {
    count = std::min(count, enqueuedTopologyMessages.size());
    std::vector<ForwardedNeighborMessage*> retval;
    for (unsigned short i = 0; i < count && !enqueuedTopologyMessages.isEmpty(); i++) {
        retval.push_back(enqueuedTopologyMessages.dequeue());
    }
    return retval;
}

TopologyMessage DynamicMeshTopologyContext::getMyTopologyMessage() {
    auto* config = ctx.getNetworkConfig();
    return new NeighborMessage(neighbors, dequeueMessages(config->getMaxForwardedTopologies()), config);
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
    auto tMsg = dynamic_cast<NeighborMessage*>(msg.getTopologyMessage());
    std::vector<ForwardedNeighborMessage*> fwds = tMsg->getForwardedTopologies();
    for (auto* fwd : fwds) {
        auto newNeighbors = fwd->getNeighbors().getNeighbors();
        auto oldNieghbors = topology.getEdges(sender);
        std::vector<unsigned char> toRemove;
        std::set_difference(oldNieghbors.begin(), oldNieghbors.end(), newNeighbors.begin(), newNeighbors.end(), toRemove.begin());
        for (auto id : toRemove)
            topology.removeEdge(sender, id);
        std::vector<unsigned char> toAdd;
        std::set_difference(newNeighbors.begin(), newNeighbors.end(), oldNieghbors.begin(), oldNieghbors.end(), toAdd.begin());
        for (auto id : toAdd)
            topology.addEdge(sender, id);
        delete fwd;
    }
    delete tMsg;
}

void MasterMeshTopologyContext::print() {
    print_dbg("[T] Current topology:\n");
    for (auto it : topology.getEdges())
        print_dbg("[%d - %d]\n", it.first, it.second);
}

} /* namespace mxnet */
