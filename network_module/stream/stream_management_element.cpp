/***************************************************************************
 *   Copyright (C) 2018-2019 by Polidori Paolo, Federico Amedeo Izzo       *
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

#include "stream_management_element.h"
#include "../util/packet.h"
#include <cstring>

namespace mxnet {

void StreamManagementElement::serialize(Packet& pkt) const {
    pkt.put(&id, sizeof(StreamId));
    pkt.put(&parameters, sizeof(StreamParameters));
    pkt.put(&type, sizeof(SMEType));
#ifdef WITH_SME_SEQNO
    pkt.put(&seqNo, sizeof(seqNo));
#endif //WITH_SME_SEQNO
}

void StreamManagementElement::deserialize(Packet& pkt) {
    pkt.get(&id, sizeof(StreamId));
    pkt.get(&parameters, sizeof(StreamParameters));
    pkt.get(&type, sizeof(SMEType));
#ifdef WITH_SME_SEQNO
    pkt.get(&seqNo, sizeof(seqNo));
#endif //WITH_SME_SEQNO
}

bool StreamManagementElement::validateInPacket(Packet& packet, unsigned int offset, unsigned short maxNodes)
{
    if(packet.size()-offset<maxSize()) return false; //Not enough bytes in packet
    
    auto id =         StreamId::fromBytes(&packet[offset]);         offset+=sizeof(id);
    auto parameters = StreamParameters::fromBytes(&packet[offset]); offset+=sizeof(parameters);
    SMEType type =    static_cast<SMEType>(packet[offset]);
    static_assert(sizeof(SMEType)==1,"");
    
    bool result = true;
    if(id.src>=maxNodes) result = false;
    switch(type)
    {
        case SMEType::LISTEN:
            if(id.dst>=maxNodes) result = false;
            if(!id.isServer()) result = false;
            break;
        case SMEType::CONNECT:
            if(id.dst>=maxNodes) result = false;
            break;
        case SMEType::CLOSED:
            if(id.dst>=maxNodes) result = false;
            break;
        case SMEType::RESEND_SCHEDULE:
            if(id.dst>=maxNodes) result = false;
            if(id.src==0) result = false; //Master node can't ask resend
            if(id.dst!=0 || id.srcPort!=0 || id.dstPort!=0) result = false; //Wrong resend packet
            break;
        case SMEType::CHALLENGE:
            if(id.src==0) result = false; //Master does not send challenges
            break;
        case SMEType::UNINITIALIZED:
            break;
        default:
            result = false;
    }
    return result;
}

#ifdef WITH_SME_SEQNO
int StreamManagementElement::seqCounter = 0;
#endif //WITH_SME_SEQNO

} /* namespace mxnet */
