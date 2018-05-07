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

#include "tree_topology_context.h"
#include "../../mac_context.h"
#include <iterator>
#include <functional>

namespace mxnet {

void DynamicTreeTopologyContext::receivedMessage(UplinkMessage msg, unsigned char sender, short rssi) {
    TopologyContext::receivedMessage(msg, sender, rssi);
    if (msg.getAssignee() != ctx.getNetworkId()) return;
    //store the link from it to the current node
    if (enqueuedTopologyMessages.hasKey(sender)) {
        delete enqueuedTopologyMessages.getByKey(sender);
        enqueuedTopologyMessages.update(sender, new RoutingLink(sender, ctx.getNetworkId()));
    } else enqueuedTopologyMessages.enqueue(sender, new RoutingLink(sender, ctx.getNetworkId()));
    //store all the links it forwards
    auto* tMsg = static_cast<RoutingVector*>(msg.getTopologyMessage());
    auto links = tMsg->getLinks();
    for (auto link : links) {
        auto from = link->getNodeId();
        if (enqueuedTopologyMessages.hasKey(from)) {
            delete enqueuedTopologyMessages.getByKey(from);
            enqueuedTopologyMessages.update(from, new RoutingLink(from, link->getPredecessor()));
        } else enqueuedTopologyMessages.enqueue(from, new RoutingLink(from, link->getPredecessor()));
    }
    delete tMsg;
}

TopologyMessage* DynamicTreeTopologyContext::getMyTopologyMessage() {
    auto config = ctx.getNetworkConfig();
    auto count = std::min(enqueuedTopologyMessages.size(), static_cast<std::size_t>(config.getMaxForwardedTopologies()));
    std::vector<RoutingLink*> links(count);
    auto forward = dequeueMessages(count);
    std::transform(forward.begin(), forward.end(), std::back_inserter(links), [](TopologyElement* elem){
        return static_cast<RoutingLink*>(elem);
    });
    return new RoutingVector(std::move(links), config);
}

void MasterTreeTopologyContext::receivedMessage(UplinkMessage msg, unsigned char sender, short rssi) {
    MasterTopologyContext::receivedMessage(msg, sender, rssi);
    auto* tMsg = static_cast<RoutingVector*>(msg.getTopologyMessage());
    std::vector<RoutingLink*> links = tMsg->getLinks();
    for (auto* link : links) {//TODO manage removals, defining the use cases and how to deal with them
        if (!topology.hasEdge(link->getNodeId(), link->getPredecessor()))
            topology.addEdge(link->getNodeId(), link->getPredecessor());
        delete link;
    }
    delete tMsg;
}

void MasterTreeTopologyContext::unreceivedMessage(unsigned char sender) {
    MasterTopologyContext::unreceivedMessage(sender);
    //TODO manage the removal of the whole tree branch after a certain threshold
}

} /* namespace mxnet */
