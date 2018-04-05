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

#include "topology_message.h"
#include <cstring>
#include <stdexcept>
#include <cassert>

namespace mxnet {

NeighborTable::NeighborTable(unsigned char nodeCount, std::vector<unsigned char> neighbors) : neighbors(nodeCount, false) {
    for(auto n : neighbors)
        this->neighbors[n] = true;
}

NeighborTable::NeighborTable(unsigned char nodeCount, std::vector<bool>& neighbors) : neighbors(nodeCount) {
    assert(neighbors.size() <= nodeCount);
    for(auto n : neighbors)
            this->neighbors = n;
}

NeighborTable::NeighborTable(const NeighborTable& other) : NeighborTable(other.neighbors.size()) {
    memcpy(neighbors.data(), other.neighbors.data(), other.getByteSize());
}

NeighborTable::NeighborTable& operator=(const NeighborTable& other) {
    neighbors = RuntimeBitset(other.neighbors.size);
    memcpy(neighbors.data(), other.neighbors.data(), other.getByteSize());
}

std::vector<unsigned char>& NeighborTable::getNeighbors() {
    std::vector<unsigned char> retval;
    for (int i = 0; i < neighbors; i++)
        if(neighbors[i]) retval.push_back(i);
    return retval;
}

void NeighborTable::setNeighbors(std::vector<unsigned char>& neighbors) {
    assert(neighbors.size() <= this->neighbors.size());
    this->neighbors.setAll(false);
    for(auto n : neighbors)
        this->neighbors[n] = true;
}

void NeighborTable::setNeighbors(std::vector<bool>& neighbors) {
    assert(neighbors.size() <= this->neighbors.size());
    for(auto n : neighbors)
        this->neighbors = n;
}

void NeighborTable::serialize(td::vector<unsigned char>::iterator pkt) {
    memcpy(pkt, neighbors.data(), neighbors.size());
}

NeighborTable NeighborTable::deserialize(std::vector<unsigned char>& pkt, unsigned char nodeCount) {
    assert(nodeCount <= pkt.size());
    NeighborTable retval(nodeCount);
    memcpy(retval.neighbors.data(), pkt.data(), nodeCount);
    return retval;
}

NeighborTable NeighborTable::deserialize(td::vector<unsigned char>::iterator pkt, unsigned char nodeCount) {
    NeighborTable retval(nodeCount);
    memcpy(retval.neighbors.data(), pkt, nodeCount);
    return retval;
}

bool NeighborTable::operator ==(const NeighborTable& b) const {
    return neighbors == b.neighbors;
}

void NeighborTable::setRaw(std::vector<unsigned char>& data) {
    memcpy(neighbors.data(), data.data(), neighbors.wordSize());
}

ForwardedNeighborMessage::ForwardedNeighborMessage(
        const NetworkConfiguration* const config,
        unsigned char nodeId,
        std::vector<unsigned char> neighbors) :
            MACMessage(), neighbors(config->getMaxNodes(), neighbors), nodeId(nodeId) {
}

ForwardedNeighborMessage::ForwardedNeighborMessage(
        const NetworkConfiguration* const config,
        unsigned char nodeId,
        std::vector<bool>& neighbors) :
            MACMessage(), neighbors(config->getMaxNodes(), neighbors), nodeId(nodeId) {
}

ForwardedNeighborMessage::ForwardedNeighborMessage(unsigned char nodeId, NeighborTable&& neighbors) :
            MACMessage(), neighbors(neighbors), nodeId(nodeId){
}

bool ForwardedNeighborMessage::operator ==(const ForwardedNeighborMessage& b) const {
    return this->nodeId == nodeId && NeighborTable::operator ==(b);
}

unsigned short ForwardedNeighborMessage::getSize() {
    return neighbors.getSize() + 1;
}

void ForwardedNeighborMessage::serialize(td::vector<unsigned char>::iterator pkt) {
    dest[0] = pkt[0];
    neighbors.serialize(pkt + 1);
}

ForwardedNeighborMessage ForwardedNeighborMessage::deserialize(std::vector<unsigned char>& pkt, const NetworkConfiguration* const config) {
    assert(config->getMaxNodes() + 1 < pkt.size());
    ForwardedNeighborMessage retval(config, NeighborTable::deserialize(pkt.begin() + 1, config->getMaxNodes()));
    retval.nodeId = pkt[0];
    return retval;
}

ForwardedNeighborMessage ForwardedNeighborMessage::deserialize(td::vector<unsigned char>::iterator pkt, const NetworkConfiguration* const config) {
    ForwardedNeighborMessage retval(config, NeighborTable::deserialize(pkt + 1, config->getMaxNodes()));
    retval.nodeId = pkt[0];
    return retval;
}

NeighborMessage::NeighborMessage(
        std::vector<unsigned char>&& neighbors,
        std::vector<ForwardedNeighborMessage>&& forwarded,
        const NetworkConfiguration* const config) :
            TopologyMessage(config), neighbors(config->getMaxNodes(), neighbors), forwardedTopologies(forwarded) {
}

NeighborMessage::NeighborMessage(
        std::vector<bool>&& neighbors,
        std::vector<ForwardedNeighborMessage>&& forwarded,
        const NetworkConfiguration* const config) :
            TopologyMessage(config), neighbors(config->getMaxNodes(), neighbors), forwardedTopologies(forwarded) {
        }

NeighborMessage::NeighborMessage(NeighborTable&& neighbors,
        std::vector<ForwardedNeighborMessage>&& forwarded,
        const NetworkConfiguration* const config) :
                    TopologyMessage(config), neighbors(neighbors), forwardedTopologies(forwarded) {
}

void NeighborMessage::serialize(td::vector<unsigned char>::iterator pkt) {
    neighbors.serialize(pkt);
    auto curSize = neighbors.getSize();
    pkt[curSize++] = forwardedTopologies.size();
    for (auto t: forwardedTopologies) {
        t.serialize(pkt + curSize);
        curSize += t.getSize();
    }
}

NeighborMessage* NeighborMessage::deserialize(std::vector<unsigned char>& pkt, const NetworkConfiguration* const config) {
    auto neighbors = NeighborTable::deserialize(pkt, config->getMaxNodes());
    auto idx = neighbors.getByteSize();
    std::vector<ForwardedNeighborMessage> forwarded(pkt[idx++]);
    assert(pkt.size() >= (idx + forward.size() * (1 + neighbors.getSize())));
    for (int i = 0; i < forwarded.size(); i++){
        forwarded[i] = ForwardedNeighborMessage::fromPkt(pkt.begin() + idx, config);
        idx += forwarded[i].getSize();
    }
    return new NeighborMessage(neighbors, std::move(forwarded), config);
}

NeighborMessage* NeighborMessage::deserialize(td::vector<unsigned char>::iterator pkt, const NetworkConfiguration* const config) {
    auto neighbors = NeighborTable::deserialize(pkt, config->getMaxNodes());
    auto idx = neighbors.getByteSize();
    std::vector<ForwardedNeighborMessage> forwarded(pkt[idx++]);
    for (int i = 0; i < forwarded.size(); i++){
        forwarded[i] = ForwardedNeighborMessage::fromPkt(pkt.begin() + idx, config);
        idx += forwarded[i].getSize();
    }
    return new NeighborMessage(neighbors, std::move(forwarded), config);
}

bool NeighborMessage::operator ==(const NeighborMessage& b) const {
    return neighbors == b.neighbors && forwardedTopologies == b.forwardedTopologies;
}

bool RoutingLink::operator ==(const RoutingLink& b) const {
    return memcmp(content, b.content, sizeof(RoutingLinkPkt)) == 0;
}

void RoutingLink::serialize(td::vector<unsigned char>::iterator pkt) {
    memcpy(content, pkt, sizeof(RoutingLinkPkt));
}

RoutingLink RoutingLink::deserialize(std::vector<unsigned char>& pkt, const NetworkConfiguration* const config) {
    assert(pkt.size() >= sizeof(RoutingLinkPkt));
    RoutingLink retval;
    memcpy(retval.content, pkt.data(), sizeof(RoutingLinkPkt));
    return retval;
}

RoutingLink RoutingLink::deserialize(td::vector<unsigned char>::iterator pkt, const NetworkConfiguration* const config) {
    RoutingLink retval;
    memcpy(retval.content, pkt, sizeof(RoutingLinkPkt));
    return retval;
}

std::vector<unsigned char> RoutingVector::getPkt() {
    std::vector<unsigned char> retval(getSize());
    getPkt(retval);
    return retval;
}

void RoutingVector::serialize(td::vector<unsigned char>::iterator pkt) {
    auto linkSize = RoutingLink::getMaxSize();
    auto it = links.begin();
    for (int i = 0; it != links.end(); i += linkSize, it++)
        memcpy(pkt + i, links[i].getPkt(), linkSize);
}

RoutingVector* RoutingVector::deserialize(std::vector<unsigned char>& pkt, const NetworkConfiguration* const config) {
    unsigned char count = pkt[0];
    assert(sizeof(pkt) >= count * RoutingLink::getMaxSize());
    auto* retval = new RoutingVector();
    retval->links.resize(count);
    for (int i = 0, bytes = 0; i < count; i++, bytes += count)
        retval->links.push_back(RoutingLink::fromPkt(pkt.begin() + bytes));
    return retval;
}

RoutingVector* RoutingVector::deserialize(td::vector<unsigned char>::iterator pkt, const NetworkConfiguration* const config) {
    unsigned char count = pkt[0];
    auto* retval = new RoutingVector();
    retval->links.resize(count);
    for (int i = 0, bytes = 0; i < count; i++, bytes += count)
        retval->links.push_back(RoutingLink::fromPkt(pkt + bytes));
    return retval;
}

} /* namespace mxnet */
