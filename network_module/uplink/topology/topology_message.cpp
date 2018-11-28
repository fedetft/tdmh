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

NeighborTable::NeighborTable(unsigned short nodeCount, std::vector<unsigned char> neighbors) : neighbors(nodeCount, false) {
    for(auto n : neighbors)
        this->neighbors[n] = true;
}

NeighborTable::NeighborTable(unsigned short nodeCount, std::vector<bool>& neighbors) : neighbors(nodeCount) {
    setNeighbors(neighbors);
}

NeighborTable::NeighborTable(const NeighborTable& other) : neighbors(other.neighbors) {}

NeighborTable::NeighborTable(NeighborTable&& other) : neighbors(std::move(other.neighbors)) {}

NeighborTable& NeighborTable::operator =(const NeighborTable& other) {
    neighbors = other.neighbors;
    return *this;
}

NeighborTable& NeighborTable::operator =(NeighborTable&& other) {
    neighbors = std::move(other.neighbors);
    return *this;
}

std::vector<unsigned char> NeighborTable::getNeighbors() {
    std::vector<unsigned char> retval;
    for (unsigned i = 0; i < neighbors.bitSize(); i++)
        if(neighbors[i]) retval.push_back(i);
    return retval;
}

void NeighborTable::setNeighbors(std::vector<unsigned char>& neighbors) {
    assert(neighbors.size() <= this->neighbors.bitSize());
    this->neighbors.setAll(false);
    for(auto n : neighbors)
        this->neighbors[n] = true;
}

void NeighborTable::setNeighbors(std::vector<bool>& neighbors) {
    assert(neighbors.size() <= this->neighbors.bitSize());
    for(unsigned i = 0; i < neighbors.size(); i++)
        this->neighbors[i] = neighbors[i];
}

void NeighborTable::serializeImpl(unsigned char* pkt) const {
    memcpy(pkt, neighbors.data(), neighbors.size());
}

NeighborTable NeighborTable::deserialize(std::vector<unsigned char>& pkt, unsigned short nodeCount) {
    unsigned short nodeBytes = nodeCount / std::numeric_limits<unsigned char>::digits;
    assert(nodeBytes <= pkt.size());
    NeighborTable retval(nodeCount);
    memcpy(retval.neighbors.data(), pkt.data(), nodeBytes);
    return retval;
}

NeighborTable NeighborTable::deserialize(unsigned char* pkt, unsigned short nodeCount) {
    auto nodeBytes = nodeCount / std::numeric_limits<unsigned char>::digits;
    NeighborTable retval(nodeCount);
    memcpy(retval.neighbors.data(), pkt, nodeBytes);
    return retval;
}

bool NeighborTable::operator ==(const NeighborTable& b) const {
    return neighbors == b.neighbors;
}

void NeighborTable::setRaw(std::vector<unsigned char>& data) {
    memcpy(neighbors.data(), data.data(), neighbors.size());
}

ForwardedNeighborMessage::ForwardedNeighborMessage(
        const NetworkConfiguration& config,
        unsigned char nodeId,
        std::vector<unsigned char>& neighbors) :
        TopologyElement(), nodeId(nodeId), neighbors(config.getMaxNodes(), neighbors) {
}

ForwardedNeighborMessage::ForwardedNeighborMessage(
        const NetworkConfiguration& config,
        unsigned char nodeId,
        std::vector<bool>& neighbors) :
        TopologyElement(), nodeId(nodeId), neighbors(config.getMaxNodes(), neighbors) {
}

ForwardedNeighborMessage::ForwardedNeighborMessage(unsigned char nodeId, NeighborTable&& neighbors) :
        TopologyElement(), nodeId(nodeId), neighbors(neighbors) {
}

bool ForwardedNeighborMessage::operator ==(const ForwardedNeighborMessage& b) const {
    return nodeId == b.nodeId && neighbors == b.neighbors;
}

std::size_t ForwardedNeighborMessage::size() const {
    return neighbors.size() + 1;
}

void ForwardedNeighborMessage::serializeImpl(unsigned char* pkt) const {
    pkt[0] = nodeId;
    neighbors.serializeImpl(pkt + 1);
}

ForwardedNeighborMessage* ForwardedNeighborMessage::deserialize(std::vector<unsigned char>& pkt, const NetworkConfiguration& config) {
    assert(static_cast<unsigned>(config.getMaxNodes() + 1) < pkt.size());
    return deserialize(pkt.data(), config);
}

ForwardedNeighborMessage* ForwardedNeighborMessage::deserialize(unsigned char* pkt, const NetworkConfiguration& config) {
    return new ForwardedNeighborMessage(pkt[0], NeighborTable::deserialize(pkt + 1, config.getMaxNodes()));
}

NeighborMessage::NeighborMessage(
        std::vector<unsigned char>& neighbors,
        std::vector<ForwardedNeighborMessage*>&& forwarded,
        const NetworkConfiguration& config) :
            TopologyMessage(config), neighbors(config.getMaxNodes(), neighbors), forwardedTopologies(forwarded) {
}

NeighborMessage::NeighborMessage(
        std::vector<bool>&& neighbors,
        std::vector<ForwardedNeighborMessage*>&& forwarded,
        const NetworkConfiguration& config) :
            TopologyMessage(config), neighbors(config.getMaxNodes(), neighbors), forwardedTopologies(forwarded) {
        }

NeighborMessage::NeighborMessage(NeighborTable neighbors,
        std::vector<ForwardedNeighborMessage*>&& forwarded,
        const NetworkConfiguration& config) :
                    TopologyMessage(config), neighbors(neighbors), forwardedTopologies(forwarded) {
}

void NeighborMessage::serializeImpl(unsigned char* pkt) const {
    neighbors.serializeImpl(pkt);
    auto curSize = neighbors.size();
    pkt[curSize++] = forwardedTopologies.size();
    for (auto t: forwardedTopologies) {
        t->serializeImpl(pkt + curSize);
        curSize += t->size();
    }
}

NeighborMessage* NeighborMessage::deserialize(std::vector<unsigned char>& pkt, const NetworkConfiguration& config) {
    return deserialize(pkt.data(), config);
}

NeighborMessage* NeighborMessage::deserialize(unsigned char* pkt, const NetworkConfiguration& config) {
    auto neighbors = NeighborTable::deserialize(pkt, config.getMaxNodes());
    auto idx = neighbors.size();
    std::vector<ForwardedNeighborMessage*> forwarded(pkt[idx++]);
    for (unsigned i = 0; i < forwarded.size(); i++) {
        forwarded[i] = ForwardedNeighborMessage::deserialize(pkt + idx, config);
        idx += forwarded[i]->size();
    }
    return new NeighborMessage(std::move(neighbors), std::move(forwarded), config);
}

bool NeighborMessage::operator ==(const NeighborMessage& b) const {
    return neighbors == b.neighbors && forwardedTopologies == b.forwardedTopologies;
}

bool RoutingLink::operator ==(const RoutingLink& b) const {
    return memcmp(&content, &b.content, sizeof(RoutingLinkPkt)) == 0;
}

void RoutingLink::serializeImpl(unsigned char* pkt) const {
    memcpy(pkt, &content, sizeof(RoutingLinkPkt));
}

RoutingLink* RoutingLink::deserialize(std::vector<unsigned char>& pkt, const NetworkConfiguration& config) {
    assert(pkt.size() >= sizeof(RoutingLinkPkt));
    return deserialize(pkt, config);
}

RoutingLink* RoutingLink::deserialize(unsigned char* pkt, const NetworkConfiguration& config) {
    auto* retval = new RoutingLink();
    memcpy(&retval->content, pkt, sizeof(RoutingLinkPkt));
    return retval;
}

void RoutingVector::serializeImpl(unsigned char* pkt) const {
    auto linkSize = RoutingLink::maxSize();
    auto it = links.begin();
    for (int i = 0; it != links.end(); i += linkSize, it++)
        links[i]->serializeImpl(pkt+i);
}

RoutingVector* RoutingVector::deserialize(std::vector<unsigned char>& pkt, const NetworkConfiguration& config) {
    unsigned char count = pkt[0];
    assert(sizeof(pkt) >= count * RoutingLink::maxSize());
    return deserialize(pkt.data(), config);
}

RoutingVector* RoutingVector::deserialize(unsigned char* pkt, const NetworkConfiguration& config) {
    unsigned char count = pkt[0];
    std::vector<RoutingLink*> links(count);
    for (int i = 0, bytes = 0; i < count; i++, bytes += RoutingLink::maxSize())
        links[i] = RoutingLink::deserialize(pkt + bytes, config);
    return new RoutingVector(std::move(links), config);
}

} /* namespace mxnet */
