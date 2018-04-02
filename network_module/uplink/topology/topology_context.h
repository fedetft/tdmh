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

#include "neighbor.h"
#include "topology_message.h"
#include "../uplink_message.h"
#include "../../updatable_queue.h"
#include "topology_map.h"
#include <list>
#include <algorithm>

namespace mxnet {

class MACContext;
class TopologyContext {
public:
    TopologyContext() = delete;
    TopologyContext(MACContext& ctx) : ctx(ctx) {};
    virtual ~TopologyContext() {}

    bool hasSuccessor(unsigned char nodeId) {
        return std::find_if(successors.begin(), successors.end(), [nodeId](Successor s){
            return s.getNodeId() == nodeId;
        }) != successors.end();
    }

    virtual NetworkConfiguration::TopologyMode getTopologyType()=0;
    virtual void receivedMessage(UplinkMessage msg, unsigned char sender, short rssi);
    virtual void unreceivedMessage(unsigned char sender);

protected:
    MACContext& ctx;
    std::list<Successor> successors;
};

class MasterTopologyContext : public TopologyContext {
public:
    MasterTopologyContext() = delete;
    MasterTopologyContext(MACContext& ctx) : TopologyContext(ctx) {};
    virtual ~MasterTopologyContext() {}
    virtual void receivedMessage(UplinkMessage msg, unsigned char sender, short rssi);
    virtual void unreceivedMessage(unsigned char sender);
protected:
    std::map<unsigned char, unsigned char> neighborsUnseenFor;
    TopologyMap<unsigned char> topology;
};

class DynamicTopologyContext : public TopologyContext {
public:
    DynamicTopologyContext() = delete;
    DynamicTopologyContext(MACContext& ctx) : TopologyContext(ctx) {};
    virtual ~DynamicTopologyContext() {}
    virtual void receivedMessage(UplinkMessage msg, unsigned char sender, short rssi);
    virtual void unreceivedMessage(unsigned char sender);
    virtual std::vector<TopologyElement*> dequeueMessages(unsigned short count);
    virtual TopologyMessage* getMyTopologyMessage()=0;
    virtual unsigned char getBestPredecessor();
    virtual bool hasPredecessor();

protected:
    UpdatableQueue<unsigned char, TopologyElement*> enqueuedTopologyMessages;
    std::list<Predecessor> predecessors;
};

} /* namespace mxnet */
