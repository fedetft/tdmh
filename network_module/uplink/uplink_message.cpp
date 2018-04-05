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

#include "uplink_message.h"
#include <cstring>

namespace mxnet {

void UplinkMessage::serialize(td::vector<unsigned char>::iterator pkt) {
    memcpy(pkt, content, sizeof(UplinkMessagePkt));
    pkt += sizeof(UplinkMessagePkt);
    topology->getPkt(pkt);
    pkt += topology->getSize();
    for (StreamManagementElement* sme : smes) {
        sme->serialize(pkt);
        pkt += sme->getSize();
    }
}

UplinkMessage UplinkMessage::deserialize(std::vector<unsigned char>& pkt, const NetworkConfiguration* const config) {
    return deserialize(pkt.begin(), config);
}

UplinkMessage UplinkMessage::deserialize(td::vector<unsigned char>::iterator pkt, const NetworkConfiguration* const config) {
    UplinkMessage retval;
    memcpy(retval.content, pkt, sizeof(count));
    switch (config->getTopologyMode()) {
    case NetworkConfiguration::NEIGHBOR_COLLECTION:
        retval.topology = NeighborMessage::fromPkt(pkt + sizeof(retval.content), config);
        break;
    case NetworkConfiguration::ROUTING_VECTOR:
        retval.topology = RoutingVector::fromPkt(pkt + sizeof(retval.content), config);
        break;
    }
    retval.smes = StreamManagementElement::fromPkt(pkt + sizeof(retval.content) + retval.topology->getSize());
    return retval;
}

} /* namespace mxnet */
