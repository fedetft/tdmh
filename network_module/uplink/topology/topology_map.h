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

template<typename T>
class TopologyMap {
public:
    TopologyMap();
    virtual ~TopologyMap();
    void addEdge(T a, T b);
    std::vector<std::pair<T, T>> getEdges() const;
    std::vector<T> getEdges(T a) const;
    bool hasEdge(T a, T b) const;
    bool removeEdge(T a, T b);
    bool removeNode(T a);


protected:
    std::multimap<T, T> edges;
};

} /* namespace mxnet */

template<typename T>
inline mxnet::TopologyMap<T>::TopologyMap() {
}

template<typename T>
inline mxnet::TopologyMap<T>::~TopologyMap() {
}

template<typename T>
inline void mxnet::TopologyMap<T>::addEdge(T a, T b) {
    edges.insert(std::make_pair(a, b));
    edges.insert(std::make_pair(b, a));
}

template<typename T>
inline std::vector<std::pair<T, T>> mxnet::TopologyMap<T>::getEdges() const {
    std::vector<std::pair<T, T>> v;
    for(auto it : edges)
        if (it.first < it.second)
            v.push_back(std::make_pair(it.first, it.second));
    return v;
}

template<typename T>
inline std::vector<T> mxnet::TopologyMap<T>::getEdges(T a) const {
    auto it = edges.equal_range(a);
    std::vector<T> v;
    std::transform(it.first, it.second, std::back_inserter(v), [](std::pair<T, T> const& el) { return el.second; });
    return v;
}

template<typename T>
inline bool mxnet::TopologyMap<T>::hasEdge(T a, T b) const {
    auto it = edges.equal_range(a);
    return std::count_if(it.first, it.second, [b](std::pair<T, T> const& el) { return el.second == b; }) > 0;
}

template<typename T>
inline bool mxnet::TopologyMap<T>::removeEdge(T a, T b) {
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
inline bool mxnet::TopologyMap<T>::removeNode(T a) {
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
