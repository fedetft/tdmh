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

namespace mxnet {

class DynamicMeshTopologyContext : public DynamicTopologyContext {
public:
    DynamicMeshTopologyContext(MACContext& ctx);
    virtual ~DynamicMeshTopologyContext() {}
    NetworkConfiguration::TopologyMode getTopologyType() const override {
        return NetworkConfiguration::TopologyMode::NEIGHBOR_COLLECTION;
    }
    void receivedMessage(UplinkMessage msg, unsigned char sender, short rssi) override;
    void unreceivedMessage(unsigned char sender) override;
    TopologyMessage* getMyTopologyMessage() override;
    void setMasterAsNeighbor(bool yes) override {
        DynamicTopologyContext::setMasterAsNeighbor(yes);
        if (yes) neighbors.addNeighbor(0);
        else neighbors.removeNeighbor(0);
    }
protected:
    void checkEnqueueOrUpdate(ForwardedNeighborMessage* msg);
    UpdatableQueue<unsigned short, ForwardedNeighborMessage*> enqueuedTopologyMessages;
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
    void receivedMessage(UplinkMessage msg, unsigned char sender, short rssi) override;
    void print() const;
protected:
};

} /* namespace mxnet */
