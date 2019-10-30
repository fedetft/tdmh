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

#include "neighbor_table.h"
#include <algorithm>
using namespace std;

namespace mxnet {

    /**
     *  Used by make_heap, push_heap and pop_heap,
     *  returns true if the first value has lower rssi than the second
     */
    struct comparePredecessors{ 
        bool operator()(const tuple<unsigned char, short, unsigned char>& a,
                        const tuple<unsigned char, short, unsigned char>& b) const{
            return get<1>(a) < get<1>(b); 
        } 
    };

//
// class NeighborTable
//

NeighborTable::NeighborTable(const NetworkConfiguration& config, const unsigned char myId,
                             const unsigned char myHop) :
    maxTimeout(config.getMaxRoundsUnavailableBecomesDead()),
    minRssi(config.getMinNeighborRSSI()),
    maxNodes(config.getMaxNodes()),
    myId(myId),
    myTopologyElement(myId,maxNodes) {
    setHop(myHop);
    setBadAssignee(true);
}


void NeighborTable::receivedMessage(unsigned char currentNode, unsigned char currentHop,
                                    int rssi, bool bad, RuntimeBitset senderTopology) {
    // If currentNode is present in activeNeighbors
    auto it = activeNeighbors.find(currentNode);
    if (it != activeNeighbors.end()) {
        // Reset timeout because we received uplink from currentNode
        it->second = maxTimeout;
    }
    else {
        // If rssi > treshold
        if(rssi >= minRssi) {
            // Add to activeNeighbors with timeout=max
            activeNeighbors[currentNode] = maxTimeout;
            // Add to myTopologyElement
            myTopologyElement.addNode(currentNode);
        }
        // If rssi < treshold but we are present in senderTopology
        else if(senderTopology[myId] == true) {
            // Add to activeNeighbors with timeout=max
            activeNeighbors[currentNode] = maxTimeout;
            // Add to myTopologyElement
            myTopologyElement.addNode(currentNode);
        }
    }
    // Check if currentNode is a predecessor
    if(currentHop < myHop) {
        // Add to predecessors, overwrite if present
        if (bad) {
            // Artificially lower priority if a node is declared badAssignee
            addPredecessor(make_tuple(currentNode, rssi-128, maxTimeout));
        } else {
            addPredecessor(make_tuple(currentNode, rssi, maxTimeout));
        }
    }
    else {
        // Remove from predecessors if present (this happens if node desyncs/resyncs)
        removePredecessor(currentNode, true);
    }

    // Evaluate whether I am a bad assignee for others
    if(myId == 0) {
        // Master is never a bad assignee
        setBadAssignee(false);
    } else if(!hasPredecessor()) {
        setBadAssignee(true);
    }
    // If my best predecessor is a bad assignee, I am a bad assignee
    else if(get<1>(predecessors.front()) < minRssi) {
        setBadAssignee(true);
    } else {
        setBadAssignee(false);
    }

}

void NeighborTable::missedMessage(unsigned char currentNode) {
    // If currentNode is present in activeNeighbors
    auto it = activeNeighbors.find(currentNode);
    if (it != activeNeighbors.end()) {
        /* Decrement timeout because we missed the uplink message from currentNode        
           if timeout is zero, neighbor node is considered dead */
        if(--it->second == 0) {
            activeNeighbors.erase(currentNode);
            myTopologyElement.removeNode(currentNode);
        }
    }
    
    removePredecessor(currentNode, false);
}

void NeighborTable::addPredecessor(tuple<unsigned char, short, unsigned char> node) {
    // Remove old value if already present in the heap
    removePredecessor(get<0>(node), true);
    // Add new value
    predecessors.push_back(node);
    // Sort new value in heap
    push_heap(predecessors.begin(), predecessors.end(), comparePredecessors());
}

void NeighborTable::removePredecessor(unsigned char nodeId, bool force) {
    // Check if value is already present in the heap
    auto it = find_if(predecessors.begin(), 
                           predecessors.end(), 
                           [&nodeId](const tuple<unsigned char, short, unsigned char>& p)
                           { return get<0>(p) == nodeId; });
    // Remove old value if present
    if(it != predecessors.end()) {
        if(force || --get<2>(*it) == 0) {
            predecessors.erase(it); 
            make_heap(predecessors.begin(), predecessors.end(), comparePredecessors());
        }
    }
}

} /* namespace mxnet */
