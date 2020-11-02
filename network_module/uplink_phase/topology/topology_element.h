/***************************************************************************
 *   Copyright (C) 2019 by Federico Amedeo Izzo, Federico Terraneo,        *
 *   Valeria Mazzola                                                       *
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

#pragma once

#include "../../util/serializable_message.h"
#include "../../util/runtime_bitset.h"

namespace mxnet {

class Packet;

/**
 * TopologyElement containg a map of the neighbors of a given node on the network
 * and the Id of that node.
 * It is sent from the Dynamic nodes towards the Master node contained in UplinkMessage
 */
class TopologyElement : public SerializableMessage {
public:
    TopologyElement(unsigned short maxNodes, bool useWeakTopologies) :
        id(0), neighbors(maxNodes,0), weakTop(useWeakTopologies) {
            if(weakTop) {
                weakNeighbors = RuntimeBitset(maxNodes, 0);
            }
        }

    TopologyElement(unsigned char id, unsigned short maxNodes, bool useWeakTopologies) :
        id(id), neighbors(maxNodes,0), weakTop(useWeakTopologies) {
            if(weakTop) {
                weakNeighbors = RuntimeBitset(maxNodes, 0);
            }
        }

    TopologyElement(unsigned char id, const RuntimeBitset& neighbors,
                    const RuntimeBitset& weakNeighbors) :
        id(id), neighbors(neighbors), weakNeighbors(weakNeighbors), weakTop(1) {}

    TopologyElement(unsigned char id, const RuntimeBitset& neighbors) :
        id(id), neighbors(neighbors), weakNeighbors(), weakTop(0) {}

    // Zero copy constructors
    TopologyElement(unsigned char id, RuntimeBitset&& neighbors) :
        id(id), neighbors(std::move(neighbors)), weakTop(0)  {}

    TopologyElement(unsigned char id, RuntimeBitset&& neighbors,
                    RuntimeBitset&& weakNeighbors) :
        id(id), neighbors(std::move(neighbors)), weakNeighbors(std::move(weakNeighbors)),
        weakTop(1)  {}
    
    // Explicitly enable move semantics which has been disabled by the destructor declaration
    TopologyElement(const TopologyElement& rhs) = default;
    TopologyElement(TopologyElement&& rhs) = default;
    TopologyElement& operator=(const TopologyElement& rhs) = default;
    TopologyElement& operator=(TopologyElement&& rhs) = default;

    virtual ~TopologyElement() {}
    
    void clear()
    {
        neighbors.setAll(0);
        if(weakTop) weakNeighbors.setAll(0);
    }

    static unsigned short maxSize(unsigned short bitmaskSize, bool useWeakTopologies) {
        if(useWeakTopologies) return sizeof(unsigned char) + 2*bitmaskSize;
        else return sizeof(unsigned char) + bitmaskSize;
    }
    std::size_t size() const override {
        if(weakTop) return sizeof(unsigned char) + neighbors.size() + weakNeighbors.size();
        else return sizeof(unsigned char) + neighbors.size();
    }
    void serialize(Packet& pkt) const override;

    void deserialize(Packet& pkt) override;

    static bool validateInPacket(Packet& packet, unsigned int offset,
                                 unsigned short maxNodes);

    unsigned char getId() const { return id; }

    const RuntimeBitset& getNeighbors() const { return neighbors; }
    const RuntimeBitset& getWeakNeighbors() const { return weakNeighbors; }

    void addNode(unsigned char nodeId) { neighbors[nodeId] = true; }
    void removeNode(unsigned char nodeId) { neighbors[nodeId] = false; }

    void weakAddNode(unsigned char nodeId) { weakNeighbors[nodeId] = true; }
    void weakRemoveNode(unsigned char nodeId) { weakNeighbors[nodeId] = false; }

private:

    /* Network ID of the node */
    unsigned char id;
    /* Neighbors of the node */
    RuntimeBitset neighbors;
    /* Weak neighbors of the node: nodes whose uplink I receive with *any* rssi */
    RuntimeBitset weakNeighbors;
    /* Whether weak topology bitmask is being used and should be serialized*/
    bool weakTop;
};

} /* namespace mxnet */
