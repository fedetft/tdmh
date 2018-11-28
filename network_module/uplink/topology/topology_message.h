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

#pragma once

#include "../../network_configuration.h"
#include "../../serializable_message.h"
#include "runtime_bitset.h"
#include <vector>
#include <iterator>
#include <algorithm>
#include <cassert>
#include <limits>

namespace mxnet {

/**
 * Represents a generic topology message that is included in the uplink message.
 */
class TopologyMessage : public SerializableMessage {
public:
    TopologyMessage() = delete;
    virtual ~TopologyMessage() {}
    virtual void deleteForwarded()=0;
protected:
    TopologyMessage(const NetworkConfiguration& config) : config(config) {}
    const NetworkConfiguration& config;
};

/**
 * Represents a generic forwarded topology information
 */
class TopologyElement : public SerializableMessage {
public:
    TopologyElement() {};
    virtual ~TopologyElement() {};
};

/**
 * Represents a bit array having the length of the number of nodes the network can contain
 * with the neighbors marked with 1s and the non-neighbors or non-connected nodes with 0s.
 */
class NeighborTable : public SerializableMessage {
public:
    NeighborTable(unsigned short nodeCount, std::vector<unsigned char> neighbors);
    NeighborTable(unsigned short nodeCount, std::vector<bool>& neighbors);
    NeighborTable() = delete;
    ~NeighborTable() {};
    NeighborTable(const NeighborTable& other);
    NeighborTable(NeighborTable&& other);
    NeighborTable& operator=(const NeighborTable& other);
    NeighborTable& operator=(NeighborTable&& other);

    /**
     * @return the neighbor address list
     */
    std::vector<unsigned char> getNeighbors();

    /**
     * marks a node as neighbor
     */
    void addNeighbor(unsigned char neighbor) { neighbors[neighbor] = true; }

    /**
     * removes a node from the neighbor list, if present
     */
    void removeNeighbor(unsigned char neighbor) { neighbors[neighbor] = false; }

    /**
     * checks whether a node is present in the neighbor list
     */
    bool hasNeighbor(unsigned char id) const { return neighbors[id]; }

    /**
     * sets the given addresses as neighbors, reinitializing the structure
     */
    void setNeighbors(std::vector<unsigned char>& neighbors);

    /**
     * reinitializes the internal structure with the given one, respecting the binary convention stated for the class
     */
    void setNeighbors(std::vector<bool>& neighbors);

    void serializeImpl(unsigned char* pkt) const override;

    static NeighborTable deserialize(std::vector<unsigned char>& pkt, unsigned short nodeCount);

    static NeighborTable deserialize(unsigned char* pkt, unsigned short nodeCount);

    std::size_t size() const override { return neighbors.size(); }

    /**
     * Returns a list of neighbors as a comma separated string, useful for printing
     */
    std::string getNeighborsString() const {
        std::ostringstream oss;
        for (unsigned i = 0; i < neighbors.bitSize(); i++)
            if (neighbors[i])
                oss << i << ", ";
        auto str = oss.str();
        return str.size()? str.substr(0, str.size() - 2) : str;
    }
    bool operator ==(const NeighborTable &b) const;
    bool operator !=(const NeighborTable &b) const {
        return !(*this == b);
    }
protected:
    NeighborTable(unsigned short nodeCount) : neighbors(nodeCount) {};
    NeighborTable(unsigned short nodeCount, bool initVal) : neighbors(nodeCount, initVal) {};

    /**
     * sets the data as byte values
     */
    void setRaw(std::vector<unsigned char>& data);
    RuntimeBitset neighbors;
};

class ForwardedNeighborMessage : public TopologyElement {
public:
    ForwardedNeighborMessage(const NetworkConfiguration& config, unsigned char nodeId, std::vector<unsigned char>& neighbors);
    ForwardedNeighborMessage(const NetworkConfiguration& config, unsigned char nodeId, std::vector<bool>& neighbors);
    ForwardedNeighborMessage(unsigned char nodeId, NeighborTable&& neighbors);
    ForwardedNeighborMessage() = delete;
    ~ForwardedNeighborMessage() {};
    bool operator ==(const ForwardedNeighborMessage &b) const;
    bool operator !=(const ForwardedNeighborMessage &b) const {
        return !(*this == b);
    }

    /**
     * @return the node to which the forwarded data belong
     */
    unsigned char getNodeId() const { return nodeId; }

    /**
     * @return the neighbor list forwarded
     */
    NeighborTable getNeighbors() const { return neighbors; }

    void serializeImpl(unsigned char* pkt) const override;
    static ForwardedNeighborMessage* deserialize(std::vector<unsigned char>& pkt, const NetworkConfiguration& config);
    static ForwardedNeighborMessage* deserialize(unsigned char* pkt, const NetworkConfiguration& config);
    std::size_t size() const override;
protected:
    ForwardedNeighborMessage(NeighborTable&& table) : neighbors(table) {};
    unsigned char nodeId;
    NeighborTable neighbors;
private:
    ForwardedNeighborMessage(const ForwardedNeighborMessage&)=delete;
    ForwardedNeighborMessage& operator=(const ForwardedNeighborMessage&)=delete;
};

class NeighborMessage : public TopologyMessage  {
public:
    NeighborMessage(std::vector<unsigned char>& neighbors, std::vector<ForwardedNeighborMessage*>&& forwarded, const NetworkConfiguration& config);
    NeighborMessage(std::vector<bool>&& neighbors, std::vector<ForwardedNeighborMessage*>&& forwarded, const NetworkConfiguration& config);
    NeighborMessage(NeighborTable neighbors, std::vector<ForwardedNeighborMessage*>&& forwarded, const NetworkConfiguration& config);
    NeighborMessage() = delete;
    virtual ~NeighborMessage() {};

    /**
     * frees the memory used to store the forwarded data
     */
    void deleteForwarded() override {
        for (auto it = forwardedTopologies.begin() ; it != forwardedTopologies.end(); ++it)
            delete (*it);
        forwardedTopologies.clear();
    }
    bool operator ==(const NeighborMessage &b) const;
    bool operator !=(const NeighborMessage &b) const {
        return !(*this == b);
    }

    /**
     * @return the neighbor table of the sender
     */
    NeighborTable getNeighbors() const { return neighbors; }

    /**
     * @return the neighbor tables the sender forwarded
     */
    std::vector<ForwardedNeighborMessage*> getForwardedTopologies() const { return forwardedTopologies; }

    void serializeImpl(unsigned char* pkt) const override;
    static NeighborMessage* deserialize(std::vector<unsigned char>& pkt, const NetworkConfiguration& config);
    static NeighborMessage* deserialize(unsigned char* pkt, const NetworkConfiguration& config);
    std::size_t size() const override {
        return size(config, forwardedTopologies.size());
    }

    /**
     * @return the maximum size such message can have, based on the network configuration
     */
    static std::size_t maxSize(const NetworkConfiguration& config) {
        return size(config, config.getMaxForwardedTopologies());
    }

    /**
     * @return the size such message can have, based on the network configuration and the actually forwarded topologies
     */
    static std::size_t size(const NetworkConfiguration& config, unsigned char forwardedTopologies) {
        unsigned char nodeBytes = ((config.getMaxNodes() - 1) >> 3) + 1;
        return nodeBytes + forwardedTopologies * (1 /* topology source addr */ + nodeBytes) + 1 /* topologies count */;
    }

protected:
    NeighborTable neighbors;
    std::vector<ForwardedNeighborMessage*> forwardedTopologies;
};

class RoutingLink : public TopologyElement {
public:
    RoutingLink(unsigned char nodeId, unsigned char predecessorId) :
        content({nodeId, predecessorId}) {};
    virtual ~RoutingLink() {};
    bool operator ==(const RoutingLink &b) const;
    bool operator !=(const RoutingLink &b) const {
        return !(*this == b);
    }

    /**
     * @return the sender id
     */
    unsigned char getNodeId() const { return content.nodeId; }

    /**
     * @return the predecessor that the sender chose.
     */
    unsigned char getPredecessor() const { return content.predecessorId; }

    /**
     * @return the size of any message
     */
    static unsigned short maxSize() { return sizeof(RoutingLinkPkt); }

    void serializeImpl(unsigned char* pkt) const override;
    static RoutingLink* deserialize(std::vector<unsigned char>& pkt, const NetworkConfiguration& config);
    static RoutingLink* deserialize(unsigned char* pkt, const NetworkConfiguration& config);
    std::size_t size() const override { return maxSize(); }
protected:
    struct RoutingLinkPkt {
        unsigned char nodeId;
        unsigned char predecessorId;
    }__attribute((packed));
    RoutingLink() {};
    RoutingLinkPkt content;
};

class RoutingVector : public TopologyMessage {
public:
    RoutingVector(std::vector<RoutingLink*>&& links, const NetworkConfiguration& config) :
        TopologyMessage(config), links(links) {}
    RoutingVector() = delete;
    virtual ~RoutingVector() {};
    std::vector<RoutingLink*>& getLinks() { return links; }

    /**
     * Frees the memory used by the list of forwarded elements
     */
    void deleteForwarded() override {
        for (auto it = links.begin() ; it != links.end(); ++it)
            delete (*it);
        links.clear();
    };

    /**
     * @return the maximum size a topology message can have, based on the current configuration.
     */
    static unsigned short maxSize(const NetworkConfiguration& config) {
        return config.getMaxForwardedTopologies() * RoutingLink::maxSize();
    }

    /**
     * @return the maximum size a topology message can have, based on the forwarded elements count.
     */
    static std::size_t size(unsigned char forwardedTopologies) {
        return forwardedTopologies * RoutingLink::maxSize();
    }
    void serializeImpl(unsigned char* pkt) const override;
    static RoutingVector* deserialize(std::vector<unsigned char>& pkt, const NetworkConfiguration& config);
    static RoutingVector* deserialize(unsigned char* pkt, const NetworkConfiguration& config);
    std::size_t size() const override { return size(links.size()); }
private:
    std::vector<RoutingLink*> links;
};
}

