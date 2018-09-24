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

#include "topology_map.h"

namespace mxnet {

void TopologyMap::addEdge(unsigned char a, unsigned char b) {
    if(a == b) throw std::logic_error("TopologyMap.addEdge() does not accept auto-edges");

    auto it = edges.find(a);
    // Bitset does not exists yet, we have to create it, initializing to false;
    if(it == edges.end()) {
        auto ret = edges.insert(std::make_pair(a, RuntimeBitset(bitset_size, false)));
        // Add the new edge
        ret.first->second[b] = true;
    }
    // Bitset already exists, just add new edge.
    else {
        it->second[b] = true;
    }
    it = edges.find(b);
    // Bitset does not exists yet, we have to create it, initializing to false;
    if(it == edges.end()) {
        auto ret = edges.insert(std::make_pair(b, RuntimeBitset(bitset_size, false)));
        ret.first->second[a] = true;
    }
    // Bitset already exists, just add new edge.
    else {
        it->second[a] = true;
    }
}

std::vector<std::pair<unsigned char, unsigned char>> TopologyMap::getEdges() const {
    std::vector<std::pair<unsigned char, unsigned char>> v;
    for(auto& el : edges) {
        // Start for cycle from a+1 of (a,b) to avoid printing duplicate links
        // and add 1 to not consider (a,a) auto-arcs
        for (unsigned i = el.first+1; i < el.second.bitSize(); i++) {
            if(el.second[i]) v.push_back(std::make_pair(el.first, i));
        }
    }
    return v;
}

std::vector<unsigned char> TopologyMap::getEdges(unsigned char a) const {
    std::vector<unsigned char> v;
    auto it = edges.find(a);
    if(it == edges.end()) return v;
    else {
        for (unsigned i = 0; i < it->second.bitSize(); i++) {
            if(it->second[i]) v.push_back(i);
        }
    }
    return v;
}

bool TopologyMap::hasEdge(unsigned char a, unsigned char b) const {
    auto it = edges.find(a);
    if(it == edges.end()) return false;
    else {
        return it->second[b];
    }
}

bool TopologyMap::hasNode(unsigned char a) const {
    auto it = edges.find(a);
    return (it != edges.end());
}

// Returns true if the value was present
bool TopologyMap::removeEdge(unsigned char a, unsigned char b) {
    if(a == b) throw std::logic_error("TopologyMap.removeEdge() does not accept auto-edges");

    auto it = edges.find(a);
    if(it == edges.end()) return false;
    else {
        it->second[b] = false;
    }
    // If the BitVector is empty, delete it
    bool empty = true;
    for (unsigned i = 0; i < it->second.bitSize(); i++) {
        empty |= it->second[i];
    }
    if(empty) edges.erase(it);

    it = edges.find(b);
    // To end up here you must have found (a,b), so return true
    if(it == edges.end()) return true;
    else {
        it->second[a] = false;
    }
    // If the BitVector is empty, delete it
    empty = true;
    for (unsigned i = 0; i < it->second.bitSize(); i++) {
        empty |= it->second[i];
    }
    if(empty) edges.erase(it);

    return true;
}

bool TopologyMap::removeNode(unsigned char a) {
    // Remove edges (a,*)
    auto it = edges.find(a);
    if(it == edges.end()) return false;
    else edges.erase(it);

    // Remove edges (*,a)
    for(auto& el : edges) {
        el.second[a]=false;
        // Remove empty BitVectors
        bool empty = true;
        for (unsigned i = 0; i < el.second.bitSize(); i++) {
            empty |= el.second[i];
        }
        if(empty) edges.erase(el.first);
    }
    return true;
}

} /* namespace mxnet */
