/***************************************************************************
 *   Copyright (C)  2019 by Federico Amedeo Izzo, Federico Terraneo        *
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
#include <map>
#include <vector>
#include <utility>

namespace mxnet {

/**
 * NeighborTable contains informations on the neighbors of a node
 */
class NeighborTable {
public:
    NeighborTable(const NetworkConfiguration& config, const unsigned char myId,
                  const unsigned char myHop);

    void receivedMessage(unsigned char currentNode, unsigned char currentHop,
                         int rssi, RuntimeBitset senderTopology);

    void missedMessage(unsigned char currentNode);

    bool hasPredecessor() { return (predecessors.size() != 0); };

    unsigned char getBestPredecessor() { return predecessors.front().first; };

    TopologyElement getMyTopologyElement() { return myTopologyElement; };

private:

    /**
     * Add a node to the predecessor list, replacing if already present
     */
    void addPredecessor(std::pair<unsigned char, short> node);

    /**
     * Remove the node from the predecessor list if present
     */
    void removePredecessor(unsigned char nodeId);

    /* Constant value from NetworkConfiguration */
    const unsigned short maxTimeout;
    const short minRssi;
    const unsigned char myId;
    const unsigned char myHop;

    /* TopologyElement containing neighbors of this node */
    TopologyElement myTopologyElement;

    /* map with key: unsigned char id, value: unsigned char timeoutCounter,
       used to remove nodes from the list of neighbors after not receiving
       their message for timeoutCounter times */
    std::map<unsigned char, unsigned short> activeNeighbors;

    /* vector containing predecessor nodes (with hop < this node)
       used as a heap (with stl methods make_heap, push_heap and pop_heap)
       The pair is <node ID, last RSSI>.
       This list is kept sorted by descending rssi, and it is
       used to pick the predecessor node with highest rssi */
    std::vector<std::pair<unsigned char, short>> predecessors;

};

} /* namespace mxnet */
