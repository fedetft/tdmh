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

#include "../serializable_message.h"
#include "topology/topology_message.h"
#include "stream_management/stream_management_element.h"
#include <vector>

namespace mxnet {

class UplinkMessage : public SerializableMessage {
public:
    UplinkMessage(unsigned char hop,
            unsigned char assignee,
            TopologyMessage* const topology,
            std::vector<StreamManagementElement> smes) :
        content({hop, assignee}), topology(topology), smes(smes) {};

    virtual ~UplinkMessage() {};

    void serializeImpl(unsigned char* pkt) const override;

    static UplinkMessage deserialize(std::vector<unsigned char>& pkt, const NetworkConfiguration& config);

    static UplinkMessage deserialize(unsigned char*, std::size_t size, const NetworkConfiguration& config);

    /**
     * @return the size of the message, if no SME needs to be sent.
     * This corresponds to size of the size of the uplink + the size of the topology part,
     * devisable from the number of forwarded topologies.
     */
    static std::size_t getSizeWithoutSMEs(TopologyMessage* const tMsg) {
        return sizeof(UplinkMessagePkt) + tMsg->size();
    }
    
    /**
     * @return the minimum packet size, without topology or sme
     */
    static std::size_t getMinSize() {
        return sizeof(UplinkMessagePkt);
    }

    /**
     * @return the hop of the message sender
     */
    unsigned char getHop() const { return content.hop; }

    /**
     * @param hop the hop of the message sender
     */
    void setHop(unsigned char hop) { content.hop = hop; }

    /**
     * @return the node to which the message content is assigned, in order to pass it to the hop closer to the master.
     */
    unsigned char getAssignee() const { return content.assignee; }

    /**
     * @param assignee the node to which the message content is assigned, in order to pass it to the hop closer to the master.
     */
    void setAssignee(unsigned char assignee) { content.assignee = assignee; }

    /**
     * @return the TopologyMessage contained
     */
    TopologyMessage* getTopologyMessage() const { return topology; }

    /**
     * @return the StreamManagementElements contained
     */
    std::vector<StreamManagementElement> getSMEs() const { return smes; }

    std::size_t size() const override {
        return getSizeWithoutSMEs(topology) + smes.size() * StreamManagementElement::maxSize();
    }
protected:
    struct UplinkMessagePkt {
        unsigned char hop;
        unsigned char assignee;
    } __attribute__((packed));
    UplinkMessage(TopologyMessage* const topology) : topology(topology) {};
    UplinkMessagePkt content;
    TopologyMessage* const topology;
    std::vector<StreamManagementElement> smes;
};

} /* namespace mxnet */
