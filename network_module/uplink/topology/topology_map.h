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

#include "runtime_bitset.h"
#include <map>
#include <vector>
#include <cstdint>
#include <stdexcept>

namespace mxnet {

/**
 * Class representing a topology as collected by the master.
 * The data structure stores the (a, b) edges
 * in a list of which every element is
 * a vector containing arcs with the same 'a' node
 * in the form of (a, b) pairs.
 *
 * This data structure is both space efficient and with
 * bounded access complexity (worst case: #nodes+#nodes=2*#nodes)
 *
 * The structure includes both (a, b) and (b, a) to keep
 * access complexity low, otherwise a whole structure scan
 * would have been necessary.
 */

class TopologyMap {
public:
    TopologyMap(std::size_t size) : bitset_size(size) {};

    /**
     * Adds an edge between two nodes
     * @param a network id of a node
     * @param b network if of the other node
     */
    void addEdge(unsigned char a, unsigned char b);

    /**
     * Returns the list of edges the composing the network graph,
     * one per link. Therefore if the pair (a, b) is present, the pair (b, a) will not.
     * @return the list of pairs of network addresses
     */
    std::vector<std::pair<unsigned char, unsigned char>> getUniqueEdges() const;

    /**
     * Returns the list of edges the composing the network graph,
     * including duplicate links
     */
    std::vector<std::pair<unsigned char, unsigned char>> getEdges() const;

    /**
     * Returns a list of edges connecting a node to others,
     * in form of list of connected addresses.
     * @return the list of connected nodes.
     */
    std::vector<unsigned char> getEdges(unsigned char a) const;

    /**
     * Checks wether a connection between two nodes is present in the graph.
     * @return if the link is present
     */
    bool hasEdge(unsigned char a, unsigned char b) const;

    /**
     * Checks wether a node is present in the graph.
     * @return if the node is present
     */
    bool hasNode(unsigned char a) const;

    /**
     * @return the number of nodes present in the graph
     */
    unsigned int getNodeCount() const {
        return edges.size();
    };

    /**
     * Returns true if the map doesn't contain any node
     */
    bool isEmpty() const {
        return edges.empty();  
    };

    /**
     * Return true if the map was modified since last time the flag was cleared 
     */
    bool wasModified() const {
        return modified_flag;
    };

    /**
     * Set the modified_flag to false
     */
    void clearModifiedFlag() {
        modified_flag = false;
    };

    /**
     * Removes a connection between two nodes.
     * @param a the node of an edge
     * @param b the node of the other edge
     * @return if the node is present
     */
    bool removeEdge(unsigned char a, unsigned char b);

    /**
     * Removes a node from the graph.
     * @param a the address of the node to be removed
     * @return if the node is present
     */
    bool removeNode(unsigned char a);


protected:
    std::size_t bitset_size;
    std::map<unsigned char, RuntimeBitset> edges;
    //IMPORTANT: this bit must be set to true whenever the data structure is modified
    bool modified_flag = false;
};

} /* namespace mxnet */
