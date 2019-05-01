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

#include "network_graph.h"
#include <set>

namespace mxnet {

//
// class NetworkGraph
//

NetworkGraph::~NetworkGraph() {}

bool NetworkGraph::hasNode(unsigned char a) {
    auto it = graph.find(a);
    return (it != graph.end());
}

std::vector<std::pair<unsigned char, unsigned char>> NetworkGraph::getEdges() {
    std::vector<std::pair<unsigned char, unsigned char>> result;
    for(auto& el : graph) {
        for (unsigned i = 0; i < maxNodes; i++) {
            if(el.second[i]) result.push_back(std::make_pair(el.first, i));
        }
    }
    return result;
}

std::vector<unsigned char> NetworkGraph::getEdges(unsigned char a) {
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

bool NetworkGraph::removeUnreachableNodes() {
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

bool NetworkGraph::getBit(unsigned char a, unsigned char b) {
    auto it = graph.find(a);
    if(it == graph.end()) return false;
    else {
        return it->second[b];
    }
}

void NetworkGraph::setBit(unsigned char a, unsigned char b) {
    auto it = graph.find(a);
    // RuntimeBitset does not exists yet, we have to create it, initializing to false;
    if(it == graph.end()) {
        auto ret = graph.insert(std::make_pair(a, RuntimeBitset(bitsetSize, false)));
        // Add the new edge
        ret.first->second[b] = true;
    }
    // Bitset already exists, just add new edge if not already present.
    else if(it->second[b] == false) {
        it->second[b] = true;
    }
}

void NetworkGraph::clearBit(unsigned char a, unsigned char b) {
    auto it = graph.find(a);
    if(it != graph.end()) {
        // If edge is present, remove it
        if(it->second[b] == true) {
            it->second[b] = false;
        }
        // If the BitVector is empty, delete it
        if(it->second.empty()) graph.erase(it);
    }
}

std::vector<std::pair<unsigned char, unsigned char>> DelayedRemovalNetworkGraph::getEdges() {
    std::vector<std::pair<unsigned char, unsigned char>> result;
    for(auto& el : graph) {
        for (unsigned i = 0; i < maxNodes; i++) {
            if(el.second[i] || el.second[i + maxNodes]) result.push_back(std::make_pair(el.first, i));
        }
    }
    return result;
}

std::vector<unsigned char> DelayedRemovalNetworkGraph::getEdges(unsigned char a) {
    std::vector<unsigned char> result;
    auto it = graph.find(a);
    if(it == graph.end()) return result;
    else {
        for (unsigned i = 0; i < maxNodes; i++) {
            if(it->second[i] || it->second[i + maxNodes]) result.push_back(i);
        }
    }
    return result;
}

bool DelayedRemovalNetworkGraph::getBit(unsigned char a, unsigned char b) {
    auto it = graph.find(a);
    if(it == graph.end()) return false;
    else {
        // If bit1 OR bit2 is true, it means the counter is not 0
        return (it->second[b] || it->second[b + maxNodes]);
    }
}

void DelayedRemovalNetworkGraph::setBit(unsigned char a, unsigned char b) {
    auto it = graph.find(a);
    // RuntimeBitset does not exists yet, we have to create it, initializing to false;
    if(it == graph.end()) {
        auto ret = graph.insert(std::make_pair(a, RuntimeBitset(bitsetSize, false)));
        // Add the new edge (LSB bit)
        ret.first->second[b] = true;
        // Add the new edge (MSB bit)
        ret.first->second[b + maxNodes] = true;
    }
    // Bitset already exists, just add new edge.
    else {
        // (LSB bit)
        if(it->second[b] == false) {
            it->second[b] = true;
        }
        // (MSB bit)
        else if(it->second[b + maxNodes] == false) {
            it->second[b + maxNodes] = true;
        }
    }
}

void DelayedRemovalNetworkGraph::clearBit(unsigned char a, unsigned char b) {
    auto it = graph.find(a);
    if(it != graph.end()) {
        // If edge is present, remove it
        if(it->second[b] == true) {
            // (LSB bit)
            it->second[b] = false;
            // (MSB bit)
            it->second[b + maxNodes] = false;

        }
        // If the BitVector is empty, delete it
        if(it->second.empty()) graph.erase(it);
    }
}

bool DelayedRemovalNetworkGraph::decrementBits(unsigned char a, unsigned char b) {
    auto it = graph.find(a);
    if(it != graph.end()) {
        bool LSB = it->second[b];
        bool MSB = it->second[b + maxNodes];

        // value = 0
        if((LSB == 0) && (MSB == 0)) {
            // Do nothing
        }
        // value = 1
        else if((LSB == 1) && (MSB == 0)) {
            it->second[b] = false;
        }
        // value = 2
        else if((LSB == 0) && (MSB == 1)) {
            it->second[b] = true;
            it->second[b + maxNodes] = false;
        }
        // value = 3
        else if((LSB == 1) && (MSB == 1)) {
            it->second[b] = false;
            it->second[b + maxNodes] = true;
        }
        
        bool counter_zero = it->second[b] || it->second[b + maxNodes];
        // If the BitVector is empty, delete it
        if(it->second.empty()) graph.erase(it);
        return counter_zero;
    }
    // If the RuntimeBitset is absent, we assume the counter is 0
    return true;
}


} /* namespace mxnet */
