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

std::vector<unsigned char> NeighborTable::getNeighbors() {
    std::vector<unsigned char> retval;
    for (int i = 0; i < neighbors; i++)
        if(neighbors[i]) retval.push_back(i);
    return retval;
}

void NeighborTable::setNeighbors(std::vector<unsigned char> neighbors) {
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

std::vector<unsigned char> NeighborTable::serialize() {
    std::vector<unsigned char> retval(neighbors.size());
    serialize(retval);
    return retval;
}

void NeighborTable::serialize(std::vector<unsigned char>& dest) {
    assert(dest.size() >= neighbors.size());
    memcpy(dest, neighbors.data(), neighbors.size());
}

NeighborTable NeighborTable::deserialize(std::vector<unsigned char> data, unsigned char nodeCount) {
    NeighborTable retval(nodeCount);
    memcpy(retval.neighbors.data(), neighbors.data(), nodeCount);
    return retval;
}

bool NeighborTable::operator ==(const NeighborTable& b) const {
    return neighbors == b.neighbors;
}

void NeighborTable::setRaw(std::vector<unsigned char> data) {
    memcpy(neighbors.data(), data.data(), neighbors.wordSize());
}

ForwardedNeighborMessage::ForwardedNeighborMessage(
        NetworkConfiguration* const config,
        unsigned char nodeId,
        std::vector<unsigned char> neighbors) :
            MACMessage(), neighbors(config->getMaxNodes(), neighbors), nodeId(nodeId) {
}

ForwardedNeighborMessage::ForwardedNeighborMessage(
        NetworkConfiguration* const config,
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
    return getByteSize() + 1;
}

std::vector<unsigned char> ForwardedNeighborMessage::getPkt() {
    std::vector<unsigned char> retval(getSize());
    getPkt(retval);
    return retval;
}

void ForwardedNeighborMessage::getPkt(std::vector<unsigned char>& dest) {
    assert(dest.size() > neighbors.getByteSize());
    auto it = dest.begin();
    dest[0] = *(it++);
    neighbors.serialize(std::vector<unsigned char>(it, dest.end()));
}

ForwardedNeighborMessage ForwardedNeighborMessage::fromPkt(std::vector<unsigned char> pkt, NetworkConfiguration* const config) {
    assert(config->getMaxNodes() < pkt.size());
    ForwardedNeighborMessage retval(config, NeighborTable::deserialize(std::vector<unsigned char>(pkt.begin() + 1, pkt.end()), config->getMaxNodes()));
    retval.nodeId = pkt[0];
    return retval;
}

NeighborMessage::NeighborMessage(
        std::vector<unsigned char> neighbors,
        std::vector<ForwardedNeighborMessage>&& forwarded,
        NetworkConfiguration* const config) :
            TopologyMessage(config), neighbors(config->getMaxNodes(), neighbors), forwardedTopologies(forwarded) {
}

NeighborMessage::NeighborMessage(
        std::vector<bool>&& neighbors,
        std::vector<ForwardedNeighborMessage>&& forwarded,
        NetworkConfiguration* const config) :
            TopologyMessage(config), neighbors(config->getMaxNodes(), neighbors), forwardedTopologies(forwarded) {
        }

NeighborMessage::NeighborMessage(NeighborTable&& neighbors,
        std::vector<ForwardedNeighborMessage>&& forwarded,
        NetworkConfiguration* const config) :
                    TopologyMessage(config), neighbors(neighbors), forwardedTopologies(forwarded) {
}

std::vector<unsigned char> NeighborMessage::getPkt() {
    std::vector retval(getSize());
    getPkt(retval);
    return retval;
}

void NeighborMessage::getPkt(std::vector<unsigned char>& dest) {
    assert(dest >= getSize());
    neighbors.serialize(dest);
    auto curSize = getByteSize();
    dest[curSize++] = forwardedTopologies.size();
    for (auto t: forwardedTopologies) {
        t.getPkt(std::vector<unsigned char>(dest.begin() + curSize, dest.end()));
        curSize += t.getSize();
    }
}

NeighborMessage* NeighborMessage::fromPkt(std::vector<unsigned char> pkt, NetworkConfiguration* const config) {
    auto neighbors = NeighborTable::deserialize(pkt, config->getMaxNodes());
    auto idx = neighbors.getByteSize();
    std::vector<ForwardedNeighborMessage> forwarded(pkt[idx++]);
    assert(pkt >= idx + forward.size() * (1 + neighbors.getByteSize()));
    for (int i = 0; i < forwarded.size(); i++){
        forwarded[i] = ForwardedNeighborMessage::fromPkt(std::vector<unsigned char>(pkt.begin() + idx, pkt.end()), config);
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

std::vector<unsigned char> RoutingLink::getPkt() {
    return std::vector<unsigned char> {{
        content.nodeId, content.predecessorId
    }};
}

void RoutingLink::getPkt(std::vector<unsigned char>& dest) {
    assert(dest.size() >= 2);
    dest[0] = content.nodeId;
    dest[1] = content.predecessorId;
}

RoutingLink* RoutingLink::fromPkt(std::vector<unsigned char> pkt, NetworkConfiguration* const config) {
    auto* retval = new RoutingLink();
    memcpy(retval->content, pkt.data(), sizeof(RoutingLinkPkt));
    return retval;
}

std::vector<unsigned char> RoutingVector::getPkt() {
    std::vector<unsigned char> retval(getSize());
    getPkt(retval);
    return retval;
}

void RoutingVector::getPkt(std::vector<unsigned char>& dest) {
    assert(dest.size() >= getSize());
    auto linkSize = RoutingLink::getMaxSize();
    auto it = links.begin();
    for (int i = 0; it != links.end(); i += linkSize, it++)
        memcpy(dest.data() + i, links[i]->getPkt(), linkSize);
}

RoutingVector* RoutingVector::fromPkt(std::vector<unsigned char> pkt, NetworkConfiguration* const config) {
    unsigned char count = pkt[0];
    if (sizeof(pkt) < count * RoutingLink::getMaxSize())
        throw std::runtime_error("non parsable routing topology packet");
    auto* retval = new RoutingVector();
    retval->links.resize(count);
    for (int i = 0, bytes = 0; i < count; i++, bytes += count)
        retval->links.push_back(RoutingLink::fromPkt(std::vector(pkt.begin() + bytes, pkt.end())));
    return retval;
}

} /* namespace mxnet */
