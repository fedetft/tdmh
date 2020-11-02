/***************************************************************************
 *   Copyright (C) 2018-2020 by Federico Amedeo Izzo, Valeria Mazzola      *
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
#include "../util/packet.h"
#include <stdexcept>

namespace mxnet {

void ScheduleHeader::serialize(Packet& pkt) const {
    pkt.put(&header, sizeof(ScheduleHeaderPkt));
}

void ScheduleHeader::deserialize(Packet& pkt) {
    pkt.get(&header, sizeof(ScheduleHeaderPkt));
}


void ScheduleElement::serialize(Packet& pkt) const {
    switch(type) {
        case DownlinkElementType::SCHEDULE_ELEMENT:
        case DownlinkElementType::INFO_ELEMENT:
            pkt.put(&id, sizeof(StreamId));
            pkt.put(&params, sizeof(StreamParameters));
            pkt.put(&content, sizeof(ScheduleElementPkt));
            break;
        case DownlinkElementType::RESPONSE: {
            pkt.put(&nodeId, sizeof(unsigned char));
            pkt.put(response, sizeof(response));
            unsigned char t = static_cast<unsigned char>(type)<<4;
            pkt.put(&t, sizeof(unsigned char));
            break;
        }
    }
}

void ScheduleElement::deserialize(Packet& pkt) {
    static_assert(sizeof(StreamId)==3 && sizeof(StreamParameters)==2 && sizeof(ScheduleElementPkt)==5,"Recompute typeIndex");
    const int typeIndex=9;//Index of byte containing the content.type field
    // Extract type from packet
    type = static_cast<DownlinkElementType>(pkt[typeIndex]>>4);
    switch(type) {
        case DownlinkElementType::SCHEDULE_ELEMENT:
        case DownlinkElementType::INFO_ELEMENT:
            pkt.get(&id, sizeof(StreamId));
            pkt.get(&params, sizeof(StreamParameters));
            pkt.get(&content, sizeof(ScheduleElementPkt));
            break;
        case DownlinkElementType::RESPONSE:
            pkt.get(&nodeId, sizeof(unsigned char));
            pkt.get(response, sizeof(response));
            pkt.discard(sizeof(unsigned char));
            break;
        default:
            throw std::runtime_error("unknown downlink element type");
            break;
    }
}

void ScheduleElement::print()
{
    switch(type)
    {
        case DownlinkElementType::SCHEDULE_ELEMENT:
            print_dbg("SCHEDULE_ELEMENT (%d,%d,%d,%d) tx:%d rx:%d off:%d\n",
                    id.src,id.dst,id.srcPort,id.dstPort,content.tx,content.rx,content.offset);
            break;
        case DownlinkElementType::INFO_ELEMENT:
        {
            const char *ptr;
            switch(static_cast<InfoType>(content.offset))
            {
                case InfoType::SERVER_CLOSED: ptr="SERVER_CLOSED"; break;
                case InfoType::SERVER_OPENED: ptr="SERVER_OPENED"; break;
                case InfoType::STREAM_REJECT: ptr="STREAM_REJECT"; break;
                default: ptr="UNKNOWN"; break;
            }
            print_dbg("INFO_ELEMENT (%d,%d,%d,%d) %s\n",
                   id.src,id.dst,id.srcPort,id.dstPort,ptr);
            break;
        }
        case DownlinkElementType::RESPONSE:
            print_dbg("RESPONSE (%d,%d,%d,%d)\n",
                   id.src,id.dst,id.srcPort,id.dstPort);
            break;
    }
}

void SchedulePacket::serialize(Packet& pkt) const {
    pkt.putPanHeader(panId);
    header.serialize(pkt);
    for(auto e : elements)
        e.serialize(pkt);
}

void SchedulePacket::deserialize(Packet& pkt) {
    pkt.removePanHeader();
    header.deserialize(pkt);
    auto count = pkt.size() / ScheduleElement::maxSize();
    elements.clear();
    elements.reserve(count);
    for(unsigned int i=0; i < count; i++) {
        ScheduleElement elem;
        elem.deserialize(pkt);
        elements.push_back(elem);
    }
}

std::size_t SchedulePacket::size() const {
    return panHeaderSize +
        header.size() +
        elements.size();
}

unsigned int SchedulePacket::getPacketCapacity() {
#ifdef CRYPTO
    const unsigned int authTagSize = 16;
    return (MediumAccessController::maxControlPktSize -
            (authTagSize + panHeaderSize + ScheduleHeader::maxSize()))
        / ScheduleElement::maxSize();
#else
    return (MediumAccessController::maxControlPktSize - (panHeaderSize + ScheduleHeader::maxSize()))
        / ScheduleElement::maxSize();
#endif
}

} /* namespace mxnet */
