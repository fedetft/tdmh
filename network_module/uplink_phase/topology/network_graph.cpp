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

#include "network_graph.h"
#include <set>

namespace mxnet {

//
// class ImmediateRemovalNetworkGraph
//

bool ImmediateRemovalNetworkGraph::hasNode(unsigned char a) {
    auto it = graph.find(a);
    return (it != graph.end());
}

std::vector<std::pair<unsigned char, unsigned char>> ImmediateRemovalNetworkGraph::getEdges() {
    std::vector<std::pair<unsigned char, unsigned char>> result;
    for(auto& el : graph) {
        // Search (a,b) with b > a
        for (unsigned i = el.first; i < maxNodes; i++) {
            if(el.second[i]) result.push_back(std::make_pair(el.first, i));
        }
    }
    return result;
}

std::vector<unsigned char> ImmediateRemovalNetworkGraph::getEdges(unsigned char a) {
    std::vector<unsigned char> result;
    auto it = graph.find(a);
    if(it == graph.end()) return result;
    else {
        for (unsigned i = 0; i < maxNodes; i++) {
            if(it->second[i]) result.push_back(i);
        }
    }
    return result;
}

bool ImmediateRemovalNetworkGraph::addEdge(unsigned char a, unsigned char b) {
    auto it = graph.find(a);
    if(it == graph.end()) it = graph.insert(std::make_pair(a, RuntimeBitset(bitsetSize, false))).first;
    bool added = it->second[b]==false;
    if(added)
    {
        it->second[b] = true;
        setBit(b,a);
    }
    return added;
}

bool ImmediateRemovalNetworkGraph::removeEdge(unsigned char a, unsigned char b) {
    auto it = graph.find(a);
    if(it == graph.end() || it->second[b]==false) return false; //Already not present
    it->second[b] = false;
    // If the BitVector is empty, delete it
    if(it->second.empty()) graph.erase(it);
    
    clearBit(b,a);
    /* Set this flag to true because removing edges may generate a graph
        where some nodes are not connected to the master node, these nodes
        needs to be eliminated with removeNotConnected() */
    possiblyNotConnected_flag = true;
    return true;
}

bool ImmediateRemovalNetworkGraph::removeUnreachableNodes() {
    std::set<unsigned char> reachable;
    std::set<unsigned char> openSet;
    // Add master-node to the openSet
    openSet.insert(0);
    // Terminate when openSet is empty
    while (!openSet.empty()) {
        // Cycle over open_set
        for(auto node = openSet.begin(); node != openSet.end() ; ) {
            // Add all child nodes not present in reachable to open_Set
            for (unsigned child = 0; child < maxNodes; child++) {
                if(reachable.find(child) == reachable.end()) { 
                    if(getBit(*node, child)) openSet.insert(child);
                }
            }
            // Move node from open_set to Reachable set
            reachable.insert(*node);
            openSet.erase(node++);
        }
    }
    bool removed = false;
    // Remove all nodes not present in reachable set
    for(auto node = graph.begin(); node != graph.end() ; ) {
        if(reachable.find((*node).first) == reachable.end()) {
            graph.erase(node++);
            removed = true;
        }
        else{
            for (unsigned child = 0; child < maxNodes; child++) {
                if(reachable.find(child) == reachable.end() && getBit((*node).first, child)) {
                    clearBit((*node).first, child);
                    removed = true;
                }
            }
            ++node;
        }
    }
    possiblyNotConnected_flag = false;
    return removed;
}

bool ImmediateRemovalNetworkGraph::getBit(unsigned char a, unsigned char b) {
    auto it = graph.find(a);
    if(it == graph.end()) return false;
    else {
        return it->second[b];
    }
}

void ImmediateRemovalNetworkGraph::setBit(unsigned char a, unsigned char b) {
    auto it = graph.find(a);
    // RuntimeBitset does not exists yet, we have to create it, initializing to false;
    if(it == graph.end()) it = graph.insert(std::make_pair(a, RuntimeBitset(bitsetSize, false))).first;
    it->second[b] = true;
}

void ImmediateRemovalNetworkGraph::clearBit(unsigned char a, unsigned char b) {
    auto it = graph.find(a);
    if(it != graph.end()) {
        // If edge is present, remove it
        if(it->second[b] == true) {
            it->second[b] = false;
            // If the BitVector is empty, delete it
            if(it->second.empty()) graph.erase(it);
        }
    }
}

} /* namespace mxnet */
