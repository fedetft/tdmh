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
            unsigned short assignee,
            TopologyMessage* const topology,
            std::vector<StreamManagementElement*> smes) :
        content({hop, assignee}), topology(topology), smes(smes) {};
    virtual ~UplinkMessage() {};
    void serialize(unsigned char* pkt) override;
    static UplinkMessage deserialize(std::vector<unsigned char>& pkt, const NetworkConfiguration& config);
    static UplinkMessage deserialize(unsigned char*, std::size_t size, const NetworkConfiguration& config);
    static std::size_t getSizeWithoutSMEs(TopologyMessage* const tMsg) {
        return sizeof(UplinkMessagePkt) + tMsg->getSize();
    }
    unsigned char getHop() { return content.hop; }
    void setHop(unsigned char hop) { content.hop = hop; }
    unsigned short getAssignee() { return content.assignee; }
    void setAssignee(unsigned short assignee) { content.assignee = assignee; }
    TopologyMessage* getTopologyMessage() { return topology; }
    std::vector<StreamManagementElement*> getSMEs() { return smes; }
    std::size_t getSize() override {
        return getSizeWithoutSMEs(topology) + smes.size() * StreamManagementElement::getMaxSize();
    }
protected:
    struct UplinkMessagePkt {
        unsigned char hop;
        unsigned short assignee;
    } __attribute__((packed));
    UplinkMessage(TopologyMessage* const topology) : topology(topology) {};
    UplinkMessagePkt content;
    TopologyMessage* const topology;
    std::vector<StreamManagementElement*> smes;
};

} /* namespace mxnet */
