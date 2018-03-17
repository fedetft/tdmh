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
#include "../maccontext.h"
#include "topology_context.h"
#include "../debug_settings.h"
#include "../bitwise_ops.h"
#include <stdexcept>

namespace miosix {

unsigned short DynamicTopologyContext::getBestPredecessor() {
    return (ctx.getHop() > 1)? min_element(predecessorsRSSIUnseenSince.begin(),
            predecessorsRSSIUnseenSince.end(),
            CompareRSSI(ctx.getNetworkConfig()->maxRoundsUnreliableParent
                    ))->first: 0;
}

unsigned short DynamicMeshTopologyContext::receivedMessage(unsigned char* pkt, unsigned short len,
        unsigned short nodeIdByTopologySlot, short rssi) {
    auto config = ctx.getNetworkConfig();
    auto size = NeighborMessage::getMaxSize(config->maxNodes, config->networkIdBits, config->hopBits);
    if (len < size)
        throw std::runtime_error("Received invalid length topology packet");
    //message from my direct neighbor
    NeighborMessage* newData = NeighborMessage::fromPkt(pkt, len);
    if (newData == nullptr) throw std::runtime_error("Wrongly checked received invalid length topology packet");
    if (newData->getSender() != nodeIdByTopologySlot) {
        unreceivedMessage(nodeIdByTopologySlot);
        throw std::runtime_error("Received topology packet from a node whose timeslot is different");
    }
    print_dbg("received: [%d/%d] : %s\n", newData->getSender(), newData->getHop(), newData->getNeighborsString().c_str());
    //add it as neighbor and update last seen
    neighborsUnseenSince[newData->getSender()] = 0;
    if (newData->getHop() < ctx.getHop())
        //if it comes from the previous hop, set its RSSI for choosing the best assignee
        predecessorsRSSIUnseenSince[newData->getSender()] = std::make_pair(rssi, 0);
    //check if my job is done
    if (newData->getAssignee() != ctx.getNetworkId()) {
        delete newData;
        return size;
    }
    //okay, i need to forward its message...
    checkEnqueueOrUpdate(newData);
    //...and all the messages which carries
    unsigned short readSize;
    for (readSize = size; readSize + size <= len; readSize += size) {
        newData = NeighborMessage::fromPkt(pkt, len - readSize, readSize);
        print_dbg("received: [%d/%d] : %s\n", newData->getSender(), newData->getHop(), newData->getNeighborsString().c_str());
        checkEnqueueOrUpdate(newData);
    }
    auto q = dynamic_cast<DynamicTopologyContext*>(ctx.getTopologyContext())->enqueuedSMEs;
    for (auto i = readSize; i + 24 <= len; i += 24) {
        unsigned char val[3];
        BitwiseOps::bitwisePopulateBitArrTop<unsigned char>(val, 24, pkt, len, 0, 24, i);
        i += 24;
        if(q.hasKey(std::make_pair(val[0], val[1])))
            q.update(std::make_pair(val[0], val[1]), std::make_tuple(val[0], val[1], val[2]));
        else
            q.enqueue(std::make_pair(val[0], val[1]), std::make_tuple(val[0], val[1], val[2]));
    }
    return readSize;
}

void DynamicMeshTopologyContext::unreceivedMessage(unsigned short nodeIdByTopologySlot) {
    auto it = neighborsUnseenSince.find(nodeIdByTopologySlot);
    if (it != neighborsUnseenSince.end()) {
        if (it->second >= ctx.getNetworkConfig()->maxRoundsUnavailableBecomesDead)
            neighborsUnseenSince.erase(it);
        else
            it->second++;
    }
    auto it2 = predecessorsRSSIUnseenSince.find(nodeIdByTopologySlot);
    if (it2 != predecessorsRSSIUnseenSince.end()) {
        if (it2->second.second >= ctx.getNetworkConfig()->maxRoundsUnreliableParent)
            predecessorsRSSIUnseenSince.erase(it2);
        else
            it2->second.second++;
    }
}

std::vector<TopologyMessage*> DynamicMeshTopologyContext::dequeueMessages(unsigned short count) {
    std::vector<TopologyMessage*> retval;
    for (unsigned short i = 0; i < count && !enqueuedTopologyMessages.isEmpty(); i++) {
        retval.push_back(enqueuedTopologyMessages.dequeue());
    }
    return retval;
}

TopologyMessage* DynamicMeshTopologyContext::getMyTopologyMessage() {
    auto* config = ctx.getNetworkConfig();
    std::vector<unsigned short> neighbors;
    if (ctx.getHop() == 1) neighbors.push_back(0);
    print_dbg("neighbors: ");
    for(auto it : neighborsUnseenSince) {
        neighbors.push_back(it.first);
        print_dbg("%d, ", it.first);
    }
    print_dbg("\n");
    return hasPredecessor()? new NeighborMessage(config->maxNodes, config->networkIdBits, config->hopBits,
            ctx.getNetworkId(), ctx.getHop(), getBestPredecessor(), std::move(neighbors)) : nullptr;
}

void DynamicMeshTopologyContext::checkEnqueueOrUpdate(NeighborMessage* msg) {
    //The node chosen me for forwarding the data
    if (enqueuedTopologyMessages.hasKey(msg->getSender())) {
        //if i already know the node
        auto* oldData = dynamic_cast<NeighborMessage*>(enqueuedTopologyMessages.getByKey(msg->getSender()));
        //if i have old data
        if (*oldData != *msg) {
            //if it's different, update it and reset its position in the queue
            enqueuedTopologyMessages.update(msg->getSender(), msg);
            delete oldData;
        }
    } else {//new neighbor's data
        enqueuedTopologyMessages.enqueue(msg->getSender(), msg);
    }
}

unsigned short MasterMeshTopologyContext::receivedMessage(unsigned char* pkt, unsigned short len, unsigned short nodeIdByTopologySlot, short rssi) {
    auto config = ctx.getNetworkConfig();
    auto size = NeighborMessage::getMaxSize(config->maxNodes, config->networkIdBits, config->hopBits);
    if (len < size)
        throw std::runtime_error("Received invalid length topology packet");
    unsigned short i;
    for (i = 0; i + size <= len;) {
        auto* newData = NeighborMessage::fromPkt(pkt, len - i, i);
        if (ENABLE_TOPOLOGY_INFO_DBG)
            print_dbg("Received topology :[%d/%d->%d]:%s\n", newData->getSender(), newData->getHop(), newData->getAssignee(), newData->getNeighborsString().c_str());
        if (newData == nullptr) throw std::runtime_error("Wrongly checked received invalid length topology packet");
        auto sender = newData->getSender();
        if (i == 0 && sender != nodeIdByTopologySlot) {
            unreceivedMessage(nodeIdByTopologySlot);
            throw std::runtime_error("Received topology packet from a node whose timeslot is different");
        }
        i += newData->getSize();
        for (unsigned short node = 0; node < config->maxNodes; node++) {
            if (newData->getNeighbors(node)) {
                if (!topology.hasEdge(sender, node)) {
                    print_dbg("Topology added %d -> %d\n", sender, node);
                    topology.addEdge(sender, node);
                }
            } else {
                if (topology.hasEdge(sender, node)) {
                    print_dbg("Topology removed %d -> %d\n", sender, node);
                    topology.removeEdge(sender, node);
                }
            }
        }
    }
    for (; i + 24 <= len; i += 24) {
        unsigned char val[3];
        BitwiseOps::bitwisePopulateBitArrTop<unsigned char>(val, 24, pkt, len, 0, 24, i);
        i += 24;
        auto p = std::make_pair(val[0], val[1]);
        if(val[2] == 0) {
            auto d = streams.find(p);
            if(d != streams.end()) {
                streams.erase(d);
            }
        } else
                streams[p] = val[2];
    }
    return i;
}

void MasterMeshTopologyContext::unreceivedMessage(unsigned short nodeIdByTopologySlot) {
    //TODO remove nodes if not seen for long time, avoiding to rely on their neighbors only
}

void MasterMeshTopologyContext::print() {
    for (auto it : topology.getEdges())
        print_dbg("[%d] - [%d]\n", it.first, it.second);
    for(auto it : streams)
        print_dbg("stream : %d -> %d [%d]\n", it.first.first, it.first.second, it.second);
}

bool DynamicTopologyContext::hasPredecessor() {
    return ctx.getHop() < 2 || predecessorsRSSIUnseenSince.size();
}

} /* namespace miosix */
