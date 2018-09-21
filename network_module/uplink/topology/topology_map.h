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

#include <map>
#include <vector>
#include <algorithm>

namespace mxnet {

/**
 * Class representing a topology as collected by the master.
 * In internally contains an unbalanced undirected acyclic graph,
 * with logarithmic access time.
 * @tparam T the data type used to represent a network id
 */
template<typename T>
class TopologyMap {
public:
    TopologyMap();
    virtual ~TopologyMap();

    /**
     * Adds an edge between two nodes
     * @param a network id of a node
     * @param b network if of the other node
     */
    void addEdge(T a, T b);

    /**
     * Returns the list of edges the composing the network graph,
     * one per link. Therefore if the pair (a, b) is present, the pair (b, a) will not.
     * @return the list of pairs of network addresses
     */
    std::vector<std::pair<T, T>> getEdges() const;

    /**
     * Returns a list of edges connecting a node to others,
     * in form of list of connected addresses.
     * @return the list of connected nodes.
     */
    std::vector<T> getEdges(T a) const;

    /**
     * Checks wether a connection between two nodes is present in the graph.
     * @return if the link is present
     */
    bool hasEdge(T a, T b) const;

    /**
     * Removes a connection between two nodes.
     * @param a the node of an edge
     * @param b the node of the other edge
     */
    bool removeEdge(T a, T b);

    /**
     * Removes a node from the graph.
     * @param a the address of the node to be removed
     */
    bool removeNode(T a);


protected:
    std::multimap<T, T> edges;
};

template<typename T>
inline TopologyMap<T>::TopologyMap() {
}

template<typename T>
inline TopologyMap<T>::~TopologyMap() {
}

template<typename T>
inline void TopologyMap<T>::addEdge(T a, T b) {
    edges.insert(std::make_pair(a, b));
    edges.insert(std::make_pair(b, a));
}

template<typename T>
inline std::vector<std::pair<T, T>> TopologyMap<T>::getEdges() const {
    std::vector<std::pair<T, T>> v;
    for(auto it : edges)
        if (it.first < it.second)
            v.push_back(std::make_pair(it.first, it.second));
    return v;
}

template<typename T>
inline std::vector<T> TopologyMap<T>::getEdges(T a) const {
    auto it = edges.equal_range(a);
    std::vector<T> v;
    std::transform(it.first, it.second, std::back_inserter(v), [](std::pair<T, T> const& el) { return el.second; });
    return v;
}

template<typename T>
inline bool TopologyMap<T>::hasEdge(T a, T b) const {
    auto it = edges.equal_range(a);
    return std::count_if(it.first, it.second, [b](std::pair<T, T> const& el) { return el.second == b; }) > 0;
}

template<typename T>
inline bool TopologyMap<T>::removeEdge(T a, T b) {
    bool retval = false;
    auto range = edges.equal_range(a);
    for (auto it = range.first; it != range.second && !retval;)
        if (it->second == b) {
            retval = true;
            it = edges.erase(it);
        } else it++;
    if (!retval) return false;
    range = edges.equal_range(b);
    for (auto it = range.first; it != range.second && retval;)
        if (it->second == a) {
            retval = false;
            it = edges.erase(it);
        } else it++;
    return true;
}

template<typename T>
inline bool TopologyMap<T>::removeNode(T a) {
    auto range = edges.equal_range(a);
    for (auto it = range.first; it != range.second;) {
        auto range2 = edges.equal_range(it->second);
        bool notFound = true;
        for (auto it2 = range2.first; it2 != range2.second && notFound;) {
            if (it2->second == a) {
                notFound = false;
                it2 = edges.erase(it2);
            } else it2++;
        }
    }
    return edges.erase(a) > 0;
}

} /* namespace mxnet */
