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

#include "network_topology.h"

namespace mxnet {

//
// class NetworkTopology
//

void NetworkTopology::receivedMessage(unsigned char currentNode, unsigned char currentHop,
                                      int rssi, RuntimeBitset senderTopology) {
    // Received message from direct neighbor
    auto it = activeDirectNeighbors.find(currentNode);
    if(currentHop == 1) {
        // If currentNode is present in activeDirectNeighbors
        if (it != activeDirectNeighbors.end()) {
            // Reset timeout because we received uplink from currentNode
            it->second = maxTimeout;

            // Lock mutex to access NetworkGraph (shared with ScheduleComputation).
            graph_mutex.lock();

            // Reset timeout on the graph
            graph.resetCounter(0, currentNode);
        }
        // If currentNode is not present in activeDirectNeighbors
        else {
            // Add node to activeDirectNeighbors with timeout=max
            activeDirectNeighbors[currentNode] = maxTimeout;

            // Lock mutex to access NetworkGraph (shared with ScheduleComputation).
            graph_mutex.lock();

            graph.addEdge(0, currentNode);
            // Set flag since we added an arc that was not present before
            modified_flag = true;
        }
        doReceivedTopology(TopologyElement(currentNode, senderTopology));

        // Unlock mutex, we finished modifying the graph
        graph_mutex.unlock();
    }
    // Received message from non direct neighbor
    else {
        // If currentNode is present in activeDirectNeighbors
        if (it != activeDirectNeighbors.end()) {
            // Remove node from activeDirectNeighbors
            activeDirectNeighbors.erase(currentNode);
        }
        // If currentNode is not present in activeDirectNeighbors, do nothing.
    }
}

void NetworkTopology::missedMessage(unsigned char currentNode) {
    // If currentNode is present in activeDirectNeighbors means its at hop=1
    // and we have seen it recently
    auto it = activeDirectNeighbors.find(currentNode);
    if (it != activeDirectNeighbors.end()) {
        /* Decrement timeout because we missed the uplink message from currentNode
           if timeout is zero, neighbor node is considered dead */
        if(it->second-- <= 0) {
            removeDirectNeighbor(currentNode);
        }
    }
}

void NetworkTopology::handleForwardedTopologies(UpdatableQueue<unsigned char,
                                                TopologyElement>& topologies) {
    // Lock mutex to access NetworkGraph (shared with ScheduleComputation).
    graph_mutex.lock();

    int numTopologies = topologies.size();
    for(int i=0; i<numTopologies; i++) {
        doReceivedTopology(topologies.dequeue());
    }

    // Unlock mutex, we finished modifying the graph
    graph_mutex.unlock();

}

void NetworkTopology::removeDirectNeighbor(unsigned char neighbor) {
    activeDirectNeighbors.erase(neighbor);
    // Lock mutex to access NetworkGraph (shared with ScheduleComputation).
    graph_mutex.lock();

    graph.removeEdge(0, neighbor);
    // Set flag since we added an arc that was not present before
    modified_flag = true;

    // Unlock mutex, we finished modifying the graph
    graph_mutex.unlock();
}

void NetworkTopology::doReceivedTopology(const TopologyElement& topology) {
    // Mutex already locked by caller
    unsigned char src = topology.getId();
    RuntimeBitset bitset = topology.getNeighbors();
    
    if(ENABLE_TOPOLOGY_BITMASK_DBG)
    {
        std::string s;
        s.reserve(bitset.bitSize()+2);
        s+='[';
        for(unsigned int i=0;i<bitset.bitSize();i++) s+=bitset[i] ? '1' : '0';
        s+=']';
        print_dbg("[U] Topo %03d: %s\n",src,s.c_str());
    }

    /* Update graph according to received topology */
    // Avoid arcs toward node 0 (handled by acriveDirectNeighbors)
    for (unsigned i = 1; i < bitset.bitSize(); i++) {
        // Avoid auto-arcs (useless in NetworkGraph)
        if(i == src)
            continue;
        // Arc is present in topology, add or resetCounter
        if(bitset[i]) {
            // Arc already present in graph
            if(graph.hasEdge(src, i)) {
                graph.resetCounter(src, i);
            }
            // Arc not present in graph
            else {
                graph.addEdge(src, i);
                // Set flag since we added an arc that was not present before
                modified_flag = true;
            }
        }
        // Arc is not present in topology, decrementCounter
        else {
            // Arc already present in graph
            if(graph.hasEdge(src, i)) {
                bool removed = graph.decrementCounter(src, i);
                if(removed) {
                    graph.removeEdge(src, i);
                    // Set flag since we removed an arc from the graph
                    modified_flag = true;
                }
            }
        }
    }
}

} /* namespace mxnet */
