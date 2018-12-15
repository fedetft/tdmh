/***************************************************************************
 *   Copyright (C)  2018 by Federico Amedeo Izzo                                 *
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
#include "../uplink/stream_management/stream_management_element.h"

namespace mxnet {

struct ScheduleHeaderPkt {
    unsigned int totalPacket:16;
    unsigned int currentPacket:16;
    unsigned int scheduleID:32;
    unsigned int repetition:2;
    unsigned int countdown:6;
};

struct ScheduleElementPkt {
    unsigned int src:8;
    unsigned int dst:8;
    unsigned int srcPort:4;
    unsigned int dstPort:4;
    unsigned int tx:8;
    unsigned int rx:8;
    unsigned int period:4;
    unsigned int offset:20;
};

class ScheduleHeader : public SerializableMessage {
public:
    ScheduleHeader() {}

    ScheduleHeader(unsigned int totalPacket, unsigned int currentPacket,
                   unsigned long scheduleID=0, unsigned char repetition=1,
                   unsigned char countdown=63)
    {
        header.totalPacket = totalPacket;
        header.currentPacket = currentPacket;
        header.scheduleID = scheduleID;
        header.repetition=repetition;
        header.countdown=countdown;
    }

    void serialize(Packet& pkt) const override;
    static ScheduleHeader deserialize(Packet& pkt);
    std::size_t size() const override { return sizeof(ScheduleHeaderPkt); }
    unsigned int getTotalPacket() const { return header.totalPacket; }
    unsigned int getPacketCounter() const { return header.currentPacket; }
    unsigned long getScheduleID() const { return header.scheduleID; }
    unsigned char getRepetition() const { return header.repetition; }
    unsigned char getCountdown() const { return header.countdown; }
    void incrementPacketCounter() { header.currentPacket++; }
    void incrementRepetition() { header.repetition++; }
    void resetPacketCounter() { header.currentPacket = 0; }
    void resetRepetition() { header.repetition = 1; }

private:
    ScheduleHeaderPkt header;
};

class ScheduleElement : public SerializableMessage {
public:
    ScheduleElement() {}

    ScheduleElement(unsigned char src, unsigned char dst, unsigned char srcPort,
                    unsigned char dstPort, unsigned char tx, unsigned char rx,
                    Period period, unsigned int off=0)
    {
        content.src = src;
        content.dst = dst;
        content.srcPort = srcPort;
        content.dstPort = dstPort;
        content.tx = tx;
        content.rx = rx;
        content.period=static_cast<unsigned int>(period);
        content.offset = off;
    }

    // Constructor copying data from StreamManagementElement
    ScheduleElement(StreamManagementElement stream, unsigned int off=0) {
        content.src = stream.getSrc();
        content.dst = stream.getDst();
        content.srcPort = stream.getSrcPort();
        content.dstPort = stream.getDstPort();
        // If a ScheduleElement is created from a stream, then tx=src, rx=dst
        // because it is a single-hop transmission
        content.tx = stream.getSrc();
        content.rx = stream.getDst();
        content.period = static_cast<unsigned int>(stream.getPeriod());
        content.offset = off;
    };

    void serialize(Packet& pkt) const override;
    static ScheduleElement deserialize(Packet& pkt);
    std::size_t size() const override { return sizeof(ScheduleElementPkt); }
    unsigned char getSrc() const { return content.src; }
    unsigned char getDst() const { return content.dst; }
    unsigned char getTx() const { return content.tx; }
    unsigned char getRx() const { return content.rx; }
    Period getPeriod() const { return static_cast<Period>(content.period); }
    unsigned int getOffset() const { return content.offset; }
    void setOffset(unsigned int off) { content.offset = off; }

    /**
     * \return an unique key for each stream
     */
    unsigned int getKey() const
    {
        return content.src | content.dst<<8 | content.srcPort<<16 | content.dstPort<<20;
    }

private:
    ScheduleElementPkt content;
};

class SchedulePacket : public SerializableMessage {

public:
    SchedulePacket() {}

    SchedulePacket(ScheduleHeader hdr, std::vector<ScheduleElement> elms) :
        header(hdr), elements(elms) {}

    void serialize(Packet& pkt) const override;
    static SchedulePacket deserialize(Packet& pkt);
    std::size_t size() const override { return header.size() + elements.size(); }

private:
    ScheduleHeader header;
    std::vector<ScheduleElement> elements;
};

} /* namespace mxnet */
