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
#include "../../util/debug_settings.h"
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
    params(config),
    maxNodes(config.getMaxNodes()),
    myId(myId),
    myTopologyElement(myId,maxNodes,params.useWeakTopologies) {
        setHop(myHop);
        badAssignee = true;
        neighbors.resize(maxNodes);
    }


void NeighborTable::receivedMessage(unsigned char currentHop, int rssi,
                                    bool bad, const TopologyElement& senderTopology) {
    unsigned char currentNode = senderTopology.getId();
    bool strongRec = senderTopology.getNeighbors()[myId];
    bool weakRec = false;
    if(params.useWeakTopologies) {
        weakRec = senderTopology.getWeakNeighbors()[myId];
    }

    // Handle state machine and topology element
    neighbors[currentNode].updateReceived(params, rssi, strongRec, weakRec);
    updateTopologyElement(neighbors[currentNode].getStatus(), currentNode);
    short avgRssi = neighbors[currentNode].getAvgRssi();

    // Handle predecessor set
    // Check if currentNode is a predecessor
    if(currentHop < myHop) {
        // Add to predecessors, overwrite if present
        if (bad) {
            // Artificially lower priority if a node is declared badAssignee
            addPredecessor(make_tuple(currentNode, avgRssi-128, params.strongTimeout));
        } else {
            addPredecessor(make_tuple(currentNode, avgRssi, params.strongTimeout));
        }
    }
    else {
        // Remove from predecessors if present (this happens if node desyncs/resyncs)
        removePredecessor(currentNode, true);
    }

    computeBadAssignee();
}

void NeighborTable::missedMessage(unsigned char currentNode) {
    // Handle state machine and topology element
    neighbors[currentNode].updateMissed(params);
    updateTopologyElement(neighbors[currentNode].getStatus(), currentNode);

    // Handle predecessor set
    removePredecessor(currentNode, false);
    computeBadAssignee();
}

void NeighborTable::updateTopologyElement(Neighbor::Status status, unsigned char node) {
    switch (status) {
    case Neighbor::Status::UNKNOWN:
        myTopologyElement.removeNode(node);
        if(params.useWeakTopologies) {
            myTopologyElement.weakRemoveNode(node);
        }
        break;
    case Neighbor::Status::WEAK:
        myTopologyElement.removeNode(node);
        if(params.useWeakTopologies) {
            myTopologyElement.weakAddNode(node);
        }
        break;
    case Neighbor::Status::STRONG:
        myTopologyElement.addNode(node);
        if(params.useWeakTopologies) {
            myTopologyElement.weakAddNode(node);
        }
        break;
    default:
        break;
    }
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

void NeighborTable::computeBadAssignee() {
    if(myId == 0) {
        // Master is never a bad assignee
        badAssignee = false;
    } else if(!hasPredecessor()) {
        badAssignee = true;
    }
    // If my best predecessor is a bad assignee, I am a bad assignee
    else if(get<1>(predecessors.front()) < params.minStrongRssi) {
        badAssignee = true;
    } else {
        badAssignee = false;
    }
}

void Neighbor::updateReceived(const NeighborParams& params, short rssi, bool strongRec, bool weakRec){
    if(params.useWeakTopologies) {
        switch(status) {
        case Status::UNKNOWN:
            /* A link is created as strong if received once with high rssi, 
             * or for reciprocity */
            if(rssi >= params.minStrongRssi || strongRec) {
                status = Status::STRONG;
                resetAvgRssi(rssi);
                freqTimeoutCtr = 0;
            /* A link is created as weak if received often with low rssi,
             * or for reciprocity */
            } else if(rssi >= params.minWeakRssi || weakRec) {
                freqTimeoutCtr += params.unknownNeighborIncrement;
                if(freqTimeoutCtr >= params.unknownNeighborThreshold || weakRec) {
                    status = Status::WEAK;
                    resetAvgRssi(rssi);
                    freqTimeoutCtr = 0;
                }
            } else freqTimeoutCtr = max(0, freqTimeoutCtr - params.unknownNeighborDecrement);
            break;
        case Status::WEAK:
            freqTimeoutCtr = 0;
            /* Upgrade to strong link if rssi rises above high threshold,
             * or for reciprocity */
            if(updateAvgRssi(rssi) >= params.minStrongRssi || strongRec) {
                status = Status::STRONG;
            }
            break;
        case Status::STRONG:
            freqTimeoutCtr = 0;
            /* Downgrade to weak link if rssi falls below low threshold,
             * or for reciprocity */
            if(updateAvgRssi(rssi) <= params.minMidRssi || !strongRec) {
                status = Status::WEAK;
            }
            break;
        default:
            break;
        }
    } else {
        switch(status) {
        case Status::UNKNOWN:
            freqTimeoutCtr += params.unknownNeighborIncrement;
            if(rssi >= params.minStrongRssi || strongRec) {
                status = Status::STRONG;
                resetAvgRssi(rssi);
                freqTimeoutCtr = 0;
            }
            break;
        case Status::STRONG:
            freqTimeoutCtr = 0;
            updateAvgRssi(rssi);
            break;
        default:
            break;
        }
    }
}

void Neighbor::updateMissed(const NeighborParams& params){
    switch(status) {
    case Status::UNKNOWN:
        freqTimeoutCtr = max(0, freqTimeoutCtr - params.unknownNeighborDecrement);
        break;
    /* Both weak and strong links disappear for timeout */
    case Status::WEAK:
        freqTimeoutCtr++;
        if(freqTimeoutCtr >= params.weakTimeout) {
            status = Status::UNKNOWN;
            freqTimeoutCtr = 0;
        }
        break;
    case Status::STRONG:
        freqTimeoutCtr++;
        if(freqTimeoutCtr >= params.strongTimeout) {
            status = Status::UNKNOWN;
            freqTimeoutCtr = 0;
        }
        break;
    default:
        break;
    }
}

} /* namespace mxnet */
