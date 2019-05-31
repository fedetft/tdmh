/***************************************************************************
 *   Copyright (C)  2018 by Federico Amedeo Izzo                           *
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

#include "schedule_element.h"

namespace mxnet {

void ScheduleHeader::serialize(Packet& pkt) const {
    pkt.put(&header, sizeof(ScheduleHeaderPkt));
}

ScheduleHeader ScheduleHeader::deserialize(Packet& pkt) {
    ScheduleHeader result;
    pkt.get(&result.header, sizeof(ScheduleHeaderPkt));
    return result;
}


void ScheduleElement::serialize(Packet& pkt) const {
    pkt.put(&id, sizeof(StreamId));
    pkt.put(&params, sizeof(StreamParameters));
    pkt.put(&content, sizeof(ScheduleElementPkt));
}

ScheduleElement ScheduleElement::deserialize(Packet& pkt) {
    ScheduleElement result;
    pkt.get(&result.id, sizeof(StreamId));
    pkt.get(&result.params, sizeof(StreamParameters));
    pkt.get(&result.content, sizeof(ScheduleElementPkt));
    return result;
}

void InfoElement::serialize(Packet& pkt) const {
    pkt.put(&id, sizeof(StreamId));
    pkt.put(&params, sizeof(StreamParameters));
    pkt.put(&content, sizeof(ScheduleElementPkt));
}

InfoElement InfoElement::deserialize(Packet& pkt) {
    InfoElement result;
    pkt.get(&result.id, sizeof(StreamId));
    pkt.get(&result.params, sizeof(StreamParameters));
    pkt.get(&result.content, sizeof(ScheduleElementPkt));
    return result;
}

void SchedulePacket::serialize(Packet& pkt) const {
    header.serialize(pkt);
    for(auto e : elements)
        e.serialize(pkt);
}

SchedulePacket SchedulePacket::deserialize(Packet& pkt) {
    ScheduleHeader header = ScheduleHeader::deserialize(pkt);
    auto count = pkt.size() / ScheduleElement::maxSize();
    std::vector<ScheduleElement> elements;
    elements.clear();
    elements.reserve(count);
    for(unsigned int i=0; i < count; i++) {
        elements.push_back(ScheduleElement::deserialize(pkt));
    }
    SchedulePacket result(header, elements);
    return result;
}

} /* namespace mxnet */
