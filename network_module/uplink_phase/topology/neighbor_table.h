/***************************************************************************
 *   Copyright (C) 2019 by Federico Amedeo Izzo, Federico Terraneo         *
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

#include "topology_element.h"
#include "../../network_configuration.h"
#include <map>
#include <vector>
#include <utility>
#include <tuple>

namespace mxnet {

/**
 * NeighborTable contains informations on the neighbors of a node
 */
class NeighborTable {
public:
    NeighborTable(const NetworkConfiguration& config, const unsigned char myId,
                  const unsigned char myHop);

    void clear(const unsigned char newHop) {
        myTopologyElement = TopologyElement(myId,maxNodes,weakTop);
        activeNeighbors.clear();
        weakActiveNeighbors.clear();
        predecessors.clear();
        setHop(newHop);
        badAssignee = true;
    }

    void receivedMessage(unsigned char currentHop, int rssi, bool bad,
                         TopologyElement senderTopology);

    void missedMessage(unsigned char currentNode);

    bool hasPredecessor() { return (predecessors.size() != 0); };

    bool isBadAssignee() { return badAssignee; };

    bool bestPredecessorIsBad() { return std::get<1>(predecessors.front()) < minRssi; };

    unsigned char getBestPredecessor() { return std::get<0>(predecessors.front()); };

    TopologyElement getMyTopologyElement() { return myTopologyElement; };

private:

    void setHop(unsigned char newHop) {
        myHop = newHop;
    }

    /**
     * Add a node to the predecessor list, replacing if already present
     */
    void addPredecessor(std::tuple<unsigned char, short, unsigned char> node);

    /**
     * Remove the node from the predecessor list if present
     */
    void removePredecessor(unsigned char nodeId, bool force);

    /* Constant value from NetworkConfiguration */
    const bool weakTop;
    const unsigned short maxTimeout;
    const unsigned short weakTimeout;
    const short minRssi;
    const unsigned short maxNodes;
    const unsigned char myId;

    /* Current hop in the network, reset with clear() after resync */
    unsigned char myHop;

    /* Whether I am a bad assignee for others */
    bool badAssignee;

    /* TopologyElement containing neighbors of this node */
    TopologyElement myTopologyElement;

    /* map with key: unsigned char id, value: unsigned char timeoutCounter,
       used to remove nodes from the list of neighbors after not receiving
       their message for timeoutCounter times.
       This timeout is used for removing nodes from my local topology. */
    std::map<unsigned char, unsigned char> activeNeighbors;
    std::map<unsigned char, unsigned char> weakActiveNeighbors;

    /* vector containing predecessor nodes (with hop < this node)
       used as a heap (with stl methods make_heap, push_heap and pop_heap)
       The tuple is <node ID, last RSSI, timeoutCounter>.
       This timeout is used for removing nodes from my list of predecessors.
       This list is kept sorted by descending rssi, and it is
       used to pick the predecessor node with highest rssi */
    std::vector<std::tuple<unsigned char, short, unsigned char>> predecessors;

};

} /* namespace mxnet */
