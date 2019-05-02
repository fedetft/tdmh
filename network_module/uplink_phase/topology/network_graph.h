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
 * NetworkGraph contains the complete graph of the network.
 * It is used only by the Master node to collect the toplogy information received
 * by Dynamic nodes
 */
class NetworkGraph {
public:
    NetworkGraph(unsigned short maxNodes, unsigned short bitsetSize) : maxNodes(maxNodes),
                                                                       bitsetSize(bitsetSize) {}

    virtual ~NetworkGraph() = 0;

    bool hasNode(unsigned char a);

    bool hasEdge(unsigned char a, unsigned char b) {
        return getBit(a,b);
    }

    bool hasUnreachableNodes() {
        return possiblyNotConnected_flag;
    }

    virtual std::vector<std::pair<unsigned char, unsigned char>> getEdges();

    virtual std::vector<unsigned char> getEdges(unsigned char a);

    void addEdge(unsigned char a, unsigned char b) {
        if(a == b) throw std::logic_error("TopologyMap.addEdge() does not accept auto-edges");
        /* Create edge a->b */
        setBit(a,b);
        /* Create edge b->a */
        setBit(b,a);
    }

    void removeEdge(unsigned char a, unsigned char b) {
        if(a == b) throw std::logic_error("TopologyMap.removeEdge() does not accept auto-edges");
        /* Remove edge a->b */
        clearBit(a,b);
        /* Remove edge b->a */
        clearBit(b,a);
        /* Set this flag to true because removing edges may generate a graph
           where some nodes are not connected to the master node, these nodes
           needs to be eliminated with removeNotConnected() */
        possiblyNotConnected_flag = true;
    }

    /* The basic NetworkGraph has no counters, so just check that the arc exists
       NOTE: when this function is used in DelayedRemovalNetworkGraph, it sets
       both bits to 1 */
    void resetCounter(unsigned char a, unsigned char b) {
        if(a == b) throw std::logic_error("TopologyMap.removeEdge() does not accept auto-edges");
        addEdge(a,b);
        addEdge(b,a);
    }

    /* The basic NetworkGraph has no counter, so just delete the arc */
    virtual bool decrementCounter(unsigned char a, unsigned char b) {
        if(a == b) throw std::logic_error("TopologyMap.removeEdge() does not accept auto-edges");
        removeEdge(a,b);
        removeEdge(b,a);
        return true;
    }

    /* This method performs a walk of the graph to find all the nodes that
       are not reachable from the node 0 (Master node), these nodes are eliminated
       from the graph because they create problems to TDMH since we cannot have
       a flow of information between these nodes and the master and vice-versa
       @return true if one or more nodes has been eliminated */
    bool removeUnreachableNodes();

protected:

    /* This method returns the value of a bit in the RuntimeBitset */
    virtual bool getBit(unsigned char a, unsigned char b);

    /* This method sets a bit in the RuntimeBitset to 1 */
    virtual void setBit(unsigned char a, unsigned char b);

    /* This method sets a bit in the RuntimeBitset to 0 */
    virtual void clearBit(unsigned char a, unsigned char b);

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

class ImmediateRemovalNetworkGraph : public NetworkGraph {
public:
    ImmediateRemovalNetworkGraph(unsigned short maxNodes) : NetworkGraph(maxNodes,
                                                                         maxNodes) {}
    ~ImmediateRemovalNetworkGraph() {}

};


/**
 * DelayedRemovalNetworkGraph contains the complete graph of the network.
 * It is used only by the Master node to collect the toplogy information received
 * by Dynamic nodes
 */
/** NOTE: This class containg a two-bit counter for each edge, to provide a mechanism
 * that is able to remove the edge only after not hearing it for a long time.
 * Because of this we need RuntimeBitset of double size
 */
/** NOTE: The first half of the double-sized RuntimeBitset stores the LSB bit
 * of the timer, is equivalent to the normal NetworkGraph class, this allows to
 * reuse the NetworkGraph implementations.
 * The second part of the RuntimeBitset contains the other bit of the counter (MSB)
 * for each node
 */
 
class DelayedRemovalNetworkGraph : public NetworkGraph {
public:
    DelayedRemovalNetworkGraph(unsigned short maxNodes) : NetworkGraph(maxNodes,
                                                                       maxNodes*2) {}

    ~DelayedRemovalNetworkGraph() {}

    /* NOTE: The two methods getEdges are reimplemented because they
       don't use the getBit/setBit primitives. (the could but the resulting
       code would be less efficient) */
    std::vector<std::pair<unsigned char, unsigned char>> getEdges();

    std::vector<unsigned char> getEdges(unsigned char a);

    /* This methods decrements bits of the counter to true */
    bool decrementCounter(unsigned char a, unsigned char b) {
        return decrementBits(a,b) || decrementBits(b,a); 
    }

private:

    /* This method returns the value of a bit in the RuntimeBitset */
    bool getBit(unsigned char a, unsigned char b);

    /* This method sets both bits in the RuntimeBitset to 1 */
    void setBit(unsigned char a, unsigned char b);

    /* This method sets both bits in the RuntimeBitset to 0 */
    void clearBit(unsigned char a, unsigned char b);

    /* This method decrement the value of the counter
       returns true if the final value is zero */
    bool decrementBits(unsigned char a, unsigned char b);


};

} /* namespace mxnet */
