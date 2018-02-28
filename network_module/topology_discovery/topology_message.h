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

#ifndef NETWORK_MODULE_TOPOLOGY_DISCOVERY_TOPOLOGY_MESSAGE_H_
#define NETWORK_MODULE_TOPOLOGY_DISCOVERY_TOPOLOGY_MESSAGE_H_

#include <vector>

namespace miosix {

class TopologyMessage {
public:
    virtual ~TopologyMessage() {}
    virtual unsigned short getSize()=0;
    virtual unsigned short getMaxSize()=0;
    virtual std::vector<unsigned char> getPkt()=0;
protected:
    TopologyMessage() {}
};

//TODO do an improved version that avoids forwarding the assignee
class NeighborMessage : public TopologyMessage {
public:
    NeighborMessage(unsigned short numNodes, unsigned short nodesBits, unsigned char hopBits, unsigned short sender,
            unsigned short hop, unsigned short assignee, std::vector<bool>&& neighbor_list) :
        numNodes(numNodes), nodesBits(nodesBits), hopBits(hopBits), sender(sender), hop(hop), assignee(assignee),
        neighbors(neighbor_list) {
    }
    virtual ~NeighborMessage() {}
    virtual unsigned short getSize() {
        return getMaxSize();
    }
    static unsigned short getMaxSize(unsigned short numNodes, unsigned short nodesBits, unsigned char hopBits) {
        return (nodesBits << 1) + hopBits + numNodes - 1;
    }
    virtual unsigned short getMaxSize() {
        return NeighborMessage::getMaxSize(numNodes, nodesBits, hopBits);
    }
    virtual std::vector<unsigned char> getPkt();
    static NeighborMessage* fromPkt(unsigned short numNodes, unsigned short nodesBits, unsigned char hopBits,
            unsigned char* pkt, unsigned short bitLen, unsigned short startBit = 0);

    bool operator ==(const NeighborMessage &b) const;
    bool operator !=(const NeighborMessage &b) const {
        return !(*this == b);
    }

    unsigned short getAssignee() const {
        return assignee;
    }

    unsigned short getHop() const {
        return hop;
    }

    const bool getNeighbors(unsigned short i) const {
        return neighbors[i];
    }

    unsigned short getSender() const {
        return sender;
    }

private:
    NeighborMessage() {};
    unsigned short numNodes;
    unsigned short nodesBits;
    unsigned char hopBits;

    unsigned short sender;
    unsigned short hop;
    unsigned short assignee;
    std::vector<bool> neighbors;
};

class RoutingVector : public TopologyMessage {
public:
    RoutingVector(unsigned short numHops, unsigned short nodesBits) :
        vector(numHops), numHops(numHops), nodesBits(nodesBits) {}
    std::vector<unsigned short> vector;
    virtual unsigned short getSize() {
        return getMaxSize();
    }
    virtual unsigned short getMaxSize() {
        return nodesBits * numHops;
    }
    virtual std::vector<unsigned char> getPkt() { /* TODO */ }
private:
    RoutingVector() : TopologyMessage() {}
    unsigned short numHops;
    unsigned short nodesBits;
};

class VLRoutingVector : public TopologyMessage {
public:
    VLRoutingVector(unsigned short numHops, unsigned short nodesBits) :
        numHops(numHops), nodesBits(nodesBits) {}
    std::vector<unsigned short> vector;
    virtual unsigned short getSize() {
        return (vector.size() + 1) * numHops;
    }
    virtual unsigned short getMaxSize() {
        return nodesBits * (numHops + 1);
    }
    virtual std::vector<unsigned char> getPkt() { /* TODO */ }
private:
    VLRoutingVector() : TopologyMessage() {}
    unsigned short numHops;
    unsigned short nodesBits;
};
}



#endif /* NETWORK_MODULE_TOPOLOGY_DISCOVERY_TOPOLOGY_MESSAGE_H_ */