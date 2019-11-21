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

#pragma once

#include "../../util/runtime_bitset.h"
#include <vector>
#include <utility>
#include <map>
#include <stdexcept>

#define GRAPH_TYPE ImmediateRemovalNetworkGraph
//#define GRAPH_TYPE DelayedRemovalNetworkGraph

namespace mxnet {


/**
 * ImmediateRemovalNetworkGraph contains the complete graph of the network.
 * It is used only by the Master node to collect the toplogy information received
 * by Dynamic nodes
 */
class ImmediateRemovalNetworkGraph {
public:
    ImmediateRemovalNetworkGraph(unsigned short maxNodes) : maxNodes(maxNodes),
                                                            bitsetSize(maxNodes) {}

    bool hasNode(unsigned char a);

    bool hasEdge(unsigned char a, unsigned char b) {
        return getBit(a,b);
    }

    bool hasUnreachableNodes() {
        return possiblyNotConnected_flag;
    }

    // NOTE: The graph stores (a,b) and (b,a) for easier searching
    // however getEdges() returns only (a,b) for shorter topology prints
    std::vector<std::pair<unsigned char, unsigned char>> getEdges();

    std::vector<unsigned char> getEdges(unsigned char a);

    /**
     * \param a one of the two nodes (order is irrelevant)
     * \param b the other node
     * \return true if the graph was modified, that is the edge was added to the graph
     */
    bool addEdge(unsigned char a, unsigned char b);

    /**
     * \param a one of the two nodes (order is irrelevant)
     * \param b the other node
     * \return true if the graph was modified, that is the edge was removed from the graph
     */
    bool removeEdge(unsigned char a, unsigned char b);

    /* This method performs a walk of the graph to find all the nodes that
       are not reachable from the node 0 (Master node), these nodes are eliminated
       from the graph because they create problems to TDMH since we cannot have
       a flow of information between these nodes and the master and vice-versa
       @return true if one or more nodes has been eliminated */
    bool removeUnreachableNodes();

protected:

    /* This method returns the value of a bit in the RuntimeBitset */
    bool getBit(unsigned char a, unsigned char b);

    /* This method sets a bit in the RuntimeBitset to 1 */
    void setBit(unsigned char a, unsigned char b);

    /* This method sets a bit in the RuntimeBitset to 0 */
    void clearBit(unsigned char a, unsigned char b);

    /* Flag that indicates that some nodes in the graph may not be connected
       to the master node, set after removeEdge, reset after calling
        the removeNotConnected() method */
    bool possiblyNotConnected_flag = false;

    /* Value equal to half of bitsetSize, used as offset to access the MSB bit
       of the counter */
    std::size_t maxNodes;

    /* Size of the Runtime Bitset, containing number of supported nodes
       to be used when creating RuntimeBitset instances inside graph */
    std::size_t bitsetSize;

    /* Map with a RuntimeBitset for each node of the network, representing
       his adjacency list */
    std::map<unsigned char, RuntimeBitset> graph;
};

} /* namespace mxnet */
