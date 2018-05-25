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

#include "topology_context.h"
#include "../../mac_context.h"

namespace mxnet {

unsigned char DynamicTopologyContext::getBestPredecessor() {
    return (ctx.getHop() > 1)? min_element(predecessors.begin(),
            predecessors.end(),
            Predecessor::CompareRSSI())->getNodeId(): 0;
}

void DynamicTopologyContext::receivedMessage(UplinkMessage msg, unsigned char sender, short rssi) {
    if (ctx.getHop() <= msg.getHop()) return;
    auto exist = std::find_if(predecessors.begin(), predecessors.end(), [sender](Predecessor el){
        return el.getNodeId() == sender;
    });
    if (exist == predecessors.end()) {
        if (rssi >= ctx.getNetworkConfig().getMinNeighborRSSI())
            predecessors.push_back(Predecessor(sender, rssi));
    } else {
        exist->seen();
        exist->setRssi(rssi);
    }
}

void DynamicTopologyContext::unreceivedMessage(unsigned char sender) {
    auto exist = std::find_if(predecessors.begin(), predecessors.end(), [sender](Predecessor el){
        return el.getNodeId() == sender;
    });
    if (exist == predecessors.end()) return;
    exist->unseen();
    if (exist->getUnseen() >= ctx.getNetworkConfig().getMaxRoundsUnavailableBecomesDead())
        predecessors.erase(exist);
}

std::vector<TopologyElement*> DynamicTopologyContext::dequeueMessages(std::size_t count) {
    std::vector<TopologyElement*> retval(std::min(enqueuedTopologyMessages.size(), count));
    for (unsigned i = 0; i < retval.size(); i++)
        retval[i] = enqueuedTopologyMessages.dequeue();
    return retval;
}

void DynamicTopologyContext::changeHop(unsigned hop) {
    //hop == 1 means directly attached to master
    predecessors.clear();
    if (hop == 1) { 
        //removing all the elements, since the first hop has only the master as predecessor
        //setting the max rssi, since there is no alternative in any case...
        predecessors.push_back(Predecessor(0, 5));
    }
}

bool DynamicTopologyContext::hasPredecessor() {
    return ctx.getHop() < 2 || predecessors.size();
}

void TopologyContext::receivedMessage(UplinkMessage msg, unsigned char sender, short rssi) {
    if (msg.getHop() != ctx.getHop() + 1) return;
    auto it = std::lower_bound(successors.begin(), successors.end(), sender);
    if(it->getNodeId() != sender) {
        successors.push_back(Successor(sender));
    } else it->seen();
}

void TopologyContext::unreceivedMessage(unsigned char sender) {
    auto it = std::lower_bound(successors.begin(), successors.end(), sender);
    if(it == successors.end() || it->getNodeId() != sender) return;
    it->unseen();
    if (it->getUnseen() >= ctx.getNetworkConfig().getMaxRoundsUnavailableBecomesDead())
        successors.erase(it);
}

void MasterTopologyContext::receivedMessage(UplinkMessage msg, unsigned char sender, short rssi) {
    if (neighborsUnseenFor.find(sender) == neighborsUnseenFor.end()) {
        topology.addEdge(sender, 0);
    }
    neighborsUnseenFor[sender] = 0;
}

void MasterTopologyContext::unreceivedMessage(unsigned char sender) {
    if (neighborsUnseenFor.find(sender) == neighborsUnseenFor.end()) return;
    if (++neighborsUnseenFor[sender] > ctx.getNetworkConfig().getMaxRoundsUnavailableBecomesDead())
        topology.removeEdge(sender, 0);
}

} /* namespace mxnet */
