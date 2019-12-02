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

#include "topology_element.h"
#include "../../network_configuration.h"
#include <vector>
#include <utility>
#include <tuple>

namespace mxnet {


class NeighborParams {
public:
    NeighborParams(const NetworkConfiguration& config) :
        useWeakTopologies(config.getUseWeakTopologies()),
        strongTimeout(config.getMaxRoundsUnavailableBecomesDead()),
        weakTimeout(config.getMaxRoundsWeakLinkBecomesDead()),
        minStrongRssi(config.getMinNeighborRSSI()),
        minWeakRssi(config.getMinWeakNeighborRSSI()) {}

    static const unsigned char unknownNeighborThreshold = 11;
    static const unsigned char unknownNeighborIncrement = 5;
    static const unsigned char unknownNeighborDecrement = 1;

    const bool useWeakTopologies;
    const unsigned short strongTimeout;
    const unsigned short weakTimeout;
    const short minStrongRssi;
    const short minWeakRssi;
};

/* Encapsulates the state of a neighbor, existent or not */
class Neighbor {
public:
    enum class Status : unsigned char {
        UNKNOWN,    // a node absent from both strong and weak topology
        WEAK,       // a node present in weak topology only
        STRONG      // a node present in both strong and weak topologies
    };

    Neighbor() {
        reset();
    }

    short getAvgRssi() { return avgRssi/one; };
    Status getStatus() { return status; };
    /* Update neighbor state after receiving an uplink message */
    void updateReceived(const NeighborParams& params, short rssi, bool strongRec, bool weakRec);
    /* Update neighbor state after missing an uplink message */
    void updateMissed(const NeighborParams& params);

    void reset() {
        status = Status::UNKNOWN;
        avgRssi = 0;
        freqTimeoutCtr = 0;
    }

private:
    //One in fixed point notation, defines # of bits of fracpart
    static constexpr short one=16;

    /* One pole low pass filter */
    short updateAvgRssi(short rssi) {
        // Hardcoded filter a=0.75
        constexpr short a=static_cast<short>(0.75f*one);
        avgRssi = avgRssi*a/one + (one-a)*rssi;
        return avgRssi/one;
    }

    void resetAvgRssi(short rssi) { avgRssi = rssi; };

    Status status;
    short avgRssi;
    /* Counter used both for link removal timeout and frequency counter for
     * link insertion */
    unsigned char freqTimeoutCtr;
};

/**
 * NeighborTable contains informations on the neighbors of a node
 */
class NeighborTable {
public:
    NeighborTable(const NetworkConfiguration& config, const unsigned char myId,
                  const unsigned char myHop);

    void clear(const unsigned char newHop) {
        myTopologyElement.clear();
        predecessors.clear();
        setHop(newHop);
        badAssignee = true;
        for (auto neigh : neighbors) {
            neigh.reset();
        }
    }

    void receivedMessage(unsigned char currentHop, int rssi, bool bad,
                         const TopologyElement& senderTopology);

    void missedMessage(unsigned char currentNode);

    void updateTopologyElement(Neighbor::Status status, unsigned char node);

    bool hasPredecessor() { return (predecessors.size() != 0); };

    bool isBadAssignee() { return badAssignee; };

    bool bestPredecessorIsBad() { return std::get<1>(predecessors.front()) < params.minStrongRssi; };

    unsigned char getBestPredecessor() { return std::get<0>(predecessors.front()); };

    const TopologyElement& getMyTopologyElement() { return myTopologyElement; };

private:

    void setHop(unsigned char newHop) { myHop = newHop; }

    /**
     * Add a node to the predecessor list, replacing if already present
     */
    void addPredecessor(std::tuple<unsigned char, short, unsigned char> node);

    /**
     * Remove the node from the predecessor list if present
     */
    void removePredecessor(unsigned char nodeId, bool force);

    /* Constant value from NetworkConfiguration */
    NeighborParams params;
    const unsigned short maxNodes;
    const unsigned char myId;

    /* Current hop in the network, reset with clear() after resync */
    unsigned char myHop;

    /* Whether I am a bad assignee for others */
    bool badAssignee;

    /* TopologyElement containing neighbors of this node */
    TopologyElement myTopologyElement;

    /* Collection of neighbors, existent or not  */
    std::vector<Neighbor> neighbors;

    /* vector containing predecessor nodes (with hop < this node)
       used as a heap (with stl methods make_heap, push_heap and pop_heap)
       The tuple is <node ID, last RSSI, timeoutCounter>.
       This timeout is used for removing nodes from my list of predecessors.
       This list is kept sorted by descending rssi, and it is
       used to pick the predecessor node with highest rssi */
    std::vector<std::tuple<unsigned char, short, unsigned char>> predecessors;

};


} /* namespace mxnet */
