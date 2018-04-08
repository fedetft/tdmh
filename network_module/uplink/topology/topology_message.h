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

class TopologyMessage : public SerializableMessage {
public:
    TopologyMessage() = delete;
    virtual ~TopologyMessage() {}
    virtual void deleteForwarded()=0;
protected:
    TopologyMessage(const NetworkConfiguration* const config) : config(config) {}
    const NetworkConfiguration* const config;
};

class TopologyElement : public SerializableMessage {
public:
    TopologyElement() {};
    virtual ~TopologyElement() {};
};

class NeighborTable : public SerializableMessage {
public:
    NeighborTable(unsigned char nodeCount, std::vector<unsigned char> neighbors);
    NeighborTable(unsigned char nodeCount, std::vector<bool>& neighbors);
    NeighborTable() = delete;
    ~NeighborTable() {};
    NeighborTable(const NeighborTable& other);
    NeighborTable(NeighborTable&& other) = default;
    NeighborTable& operator=(const NeighborTable& other) {
        new (&neighbors) RuntimeBitset(other.neighbors);
    }
    NeighborTable& operator=(NeighborTable&& other) = default;
    std::vector<unsigned char> getNeighbors();
    void addNeighbor(unsigned char neighbor) { neighbors[neighbor] = true; }
    void removeNeighbor(unsigned char neighbor) { neighbors[neighbor] = false; }
    bool hasNeighbor(unsigned char id) { return neighbors[id]; }
    void setNeighbors(std::vector<unsigned char>& neighbors);
    void setNeighbors(std::vector<bool>& neighbors);

    void serialize(unsigned char* pkt) override;
    static NeighborTable deserialize(std::vector<unsigned char>& pkt, unsigned char nodeCount);
    static NeighborTable deserialize(unsigned char* pkt, unsigned char nodeCount);
    std::size_t getSize() override { return neighbors.wordSize(); }

    std::string getNeighborsString() const {
        std::string str;
        for (int i = 0; i < neighbors.size(); i++)
            if (neighbors[i])
                str += std::to_string(i) + ", ";
        return str.size()? str.substr(0, str.size() - 2) : str;
    }
    bool operator ==(const NeighborTable &b) const;
    bool operator !=(const NeighborTable &b) const {
        return !(*this == b);
    }
protected:
    NeighborTable(unsigned char nodeCount) : neighbors(nodeCount) {};
    NeighborTable(unsigned char nodeCount, bool initVal) : neighbors(nodeCount, initVal) {};
    void setRaw(std::vector<unsigned char>& data);
    RuntimeBitset neighbors;
};

class ForwardedNeighborMessage : public TopologyElement {
public:
    ForwardedNeighborMessage(const NetworkConfiguration* const config, unsigned char nodeId, std::vector<unsigned char>& neighbors);
    ForwardedNeighborMessage(const NetworkConfiguration* const config, unsigned char nodeId, std::vector<bool>& neighbors);
    ForwardedNeighborMessage(unsigned char nodeId, NeighborTable&& neighbors);
    ForwardedNeighborMessage() = delete;
    ~ForwardedNeighborMessage() {};
    bool operator ==(const ForwardedNeighborMessage &b) const;
    bool operator !=(const ForwardedNeighborMessage &b) const {
        return !(*this == b);
    }
    unsigned char getNodeId() { return nodeId; }
    NeighborTable getNeighbors() { return neighbors; }

    void serialize(unsigned char* pkt) override;
    static ForwardedNeighborMessage* deserialize(std::vector<unsigned char>& pkt, const NetworkConfiguration* const config);
    static ForwardedNeighborMessage* deserialize(unsigned char* pkt, const NetworkConfiguration* const config);
    std::size_t getSize() override;
protected:
    ForwardedNeighborMessage(NeighborTable&& table) : neighbors(table) {};
    unsigned char nodeId;
    NeighborTable neighbors;
};

class NeighborMessage : public TopologyMessage  {
public:
    NeighborMessage(std::vector<unsigned char>& neighbors, std::vector<ForwardedNeighborMessage*>&& forwarded, const NetworkConfiguration* const config);
    NeighborMessage(std::vector<bool>&& neighbors, std::vector<ForwardedNeighborMessage*>&& forwarded, const NetworkConfiguration* const config);
    NeighborMessage(NeighborTable neighbors, std::vector<ForwardedNeighborMessage*>&& forwarded, const NetworkConfiguration* const config);
    NeighborMessage() = delete;
    virtual ~NeighborMessage() {};
    void deleteForwarded() override {
        forwardedTopologies.clear();
    }
    bool operator ==(const NeighborMessage &b) const;
    bool operator !=(const NeighborMessage &b) const {
        return !(*this == b);
    }
    NeighborTable getNeighbors() { return neighbors; }
    std::vector<ForwardedNeighborMessage*> getForwardedTopologies() { return forwardedTopologies; }

    void serialize(unsigned char* pkt) override;
    static NeighborMessage* deserialize(std::vector<unsigned char>& pkt, const NetworkConfiguration* const config);
    static NeighborMessage* deserialize(unsigned char* pkt, const NetworkConfiguration* const config);
    std::size_t getSize() override {
        return getSize(config, forwardedTopologies.size());
    }

    static std::size_t getMaxSize(const NetworkConfiguration* const config) {
        return getSize(config, config->getMaxForwardedTopologies());
    }
    static std::size_t getSize(const NetworkConfiguration* const config, unsigned char forwardedTopologies) {
        return ((config->getMaxNodes() - 1) >> 3) + 1 + forwardedTopologies * (8 + ((config->getMaxNodes() - 1) >> 3) + 1);
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
    unsigned char getNodeId() { return content.nodeId; }
    unsigned char getPredecessor() { return content.predecessorId; }
    static unsigned short getMaxSize() { return sizeof(RoutingLinkPkt); }

    using TopologyElement::serialize;
    void serialize(unsigned char* pkt) override;
    static RoutingLink* deserialize(std::vector<unsigned char>& pkt, const NetworkConfiguration* const config);
    static RoutingLink* deserialize(unsigned char* pkt, const NetworkConfiguration* const config);
    std::size_t getSize() override { return getMaxSize(); }
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
    RoutingVector(std::vector<RoutingLink*>&& links, const NetworkConfiguration* const config) :
        TopologyMessage(config), links(links) {}
    RoutingVector() = delete;
    virtual ~RoutingVector() {};
    std::vector<RoutingLink*>& getLinks() { return links; }
    void deleteForwarded() override {
        links.clear();
    };
    static unsigned short getMaxSize(const NetworkConfiguration* const config) {
        return config->getMaxForwardedTopologies() * RoutingLink::getMaxSize();
    }
    static std::size_t getSize(unsigned char forwardedTopologies) {
        return forwardedTopologies * RoutingLink::getMaxSize();
    }
    using TopologyMessage::serialize;
    void serialize(unsigned char* pkt) override;
    static RoutingVector* deserialize(std::vector<unsigned char>& pkt, const NetworkConfiguration* const config);
    static RoutingVector* deserialize(unsigned char* pkt, const NetworkConfiguration* const config);
    std::size_t getSize() override { return getSize(links.size()); }
private:
    std::vector<RoutingLink*> links;
};
}

