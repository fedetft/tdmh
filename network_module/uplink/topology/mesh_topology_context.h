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

#include "topology_context.h"
#include "topology_message.h"
#include <map>
#include <functional>
#include <type_traits>

namespace mxnet {

class DynamicMeshTopologyContext : public DynamicTopologyContext {
public:
    DynamicMeshTopologyContext(MACContext& ctx);
    virtual ~DynamicMeshTopologyContext() {};
    NetworkConfiguration::TopologyMode getTopologyType() const override {
        return NetworkConfiguration::TopologyMode::NEIGHBOR_COLLECTION;
    }

    /**
     * Sets the node as neighbor and, if the current node is the assignee, its data is stored.
     * Also calls DynamicTopologyContext::receivedMessage
     */
    void receivedMessage(UplinkMessage msg, unsigned char sender, short rssi) override;

    /**
     * If the node is a neighbor, it is marked as unseen.
     * Also calls DynamicTopologyContext::unreceivedMessage
     */
    void unreceivedMessage(unsigned char sender) override;

    /**
     * Builds the topology message to send, including the neighbor table and the forwarded topologies
     */
    TopologyMessage* getMyTopologyMessage() override;

    /**
     * Used by the timesync phase to set/remove the master node as neighbor in case the node belongs/does not belong to hop 1,
     * since in such case it won't receive any uplink from the master, it will just need the timesync with
     * hop count = 0.
     */
    void changeHop(unsigned hop) override {
        DynamicTopologyContext::changeHop(hop);
        if (hop == 1) neighbors.addNeighbor(0);
        else neighbors.removeNeighbor(0);
    }
protected:
    /**
     * Checks whether a received message needs to be stored or an old copy
     * is already stored and needs to be updated
     */
    void checkEnqueueOrUpdate(ForwardedNeighborMessage* msg);
    NeighborTable neighbors;
    std::map<unsigned char, unsigned char> neighborsUnseenFor;
};

class MasterMeshTopologyContext : public MasterTopologyContext {
public:
    MasterMeshTopologyContext(MACContext& ctx) : MasterTopologyContext(ctx) {};
    virtual ~MasterMeshTopologyContext() {}

    NetworkConfiguration::TopologyMode getTopologyType() const override {
        return NetworkConfiguration::TopologyMode::NEIGHBOR_COLLECTION;
    }

    /**
     * Adds the edges for the received topology to the network graph.
     * Also calls DynamicTopologyContext::receivedMessage
     */
    void receivedMessage(UplinkMessage msg, unsigned char sender, short rssi) override;
    void print() const;

    template<class InputIt1, class InputIt2>
    static void setDifferenceDo(InputIt1 first1, InputIt1 last1,
            InputIt2 first2, InputIt2 last2, std::function<void(typename std::iterator_traits<InputIt1>::value_type)> fun) {
        while (first1 != last1) {
            if (first2 == last2) {
                while (first1 != last1)
                    fun(*first1++);
            } else if (*first1 < *first2) {
                fun(*first1++);
            } else {
                if (*first2 >= *first1) {
                    ++first1;
                }
                ++first2;
            }
        }
    }
protected:
    /**
     * Adds or removes edges from the topology maps, based on the message content
     */
    void manageTopologyUpdate(unsigned char sender, NeighborTable neighbors);
};

} /* namespace mxnet */
