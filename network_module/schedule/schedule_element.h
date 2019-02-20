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

#pragma once
#include "../serializable_message.h"
#include "../uplink/stream_management/stream_management_element.h"
#include <cstring>
#include <iostream>

namespace mxnet {

/* Possible actions to do in a dataphase slot */
enum class Action
{
    SLEEP      =0,    // Sleep to save energy
    SENDSTREAM =1,    // Send packet of a stream opened from this node
    RECVSTREAM =2,    // Receive packet of a stream opened to this node
    SENDBUFFER =3,    // Send a saved packet from a multihop stream
    RECVBUFFER =4     // Receive and save packet from a multihop stream
};

enum class InfoType
{
    ACK_LISTEN   =0, // Signals that the master have received the Listen request
    NACK_CONNECT =1  // Signals that the connect has failed for lack of a listen 
};

struct ScheduleHeaderPkt {
    unsigned int totalPacket:16;
    unsigned int currentPacket:16;
    unsigned int scheduleID:32;
    unsigned int activationTile:32;
    unsigned int scheduleTiles:16;
    unsigned int repetition:2;
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
    ScheduleHeader() {
        std::memset(&header, 0, sizeof(ScheduleHeaderPkt));
    }

    ScheduleHeader(unsigned int totalPacket, unsigned int currentPacket,
                   unsigned long scheduleID=0, unsigned long activationTile=0,
                   unsigned int scheduleTiles=0, unsigned char repetition=1)
    {
        header.totalPacket = totalPacket;
        header.currentPacket = currentPacket;
        header.scheduleID = scheduleID;
        header.activationTile = activationTile;
        header.scheduleTiles = scheduleTiles;
        header.repetition = repetition;
    }

    void serialize(Packet& pkt) const override;
    static ScheduleHeader deserialize(Packet& pkt);
    std::size_t size() const override { return sizeof(ScheduleHeaderPkt); }
    unsigned int getTotalPacket() const { return header.totalPacket; }
    unsigned int getCurrentPacket() const { return header.currentPacket; }
    // NOTE: schedule with ScheduleID=0 are not sent in MasterScheduleDistribution
    unsigned long getScheduleID() const { return header.scheduleID; }
    unsigned long getActivationTile() const { return header.activationTile; }
    unsigned int getScheduleTiles() const { return header.scheduleTiles; }
    unsigned char getRepetition() const { return header.repetition; }
    void incrementPacketCounter() { header.currentPacket++; }
    void incrementRepetition() {
        // Prevent overflow (2 bit number)
        if(header.repetition == 3)
            header.repetition = 1;
        else
            header.repetition++;
    }
    void resetPacketCounter() { header.currentPacket = 0; }
    void resetRepetition() { header.repetition = 1; }
    void setActivationTile(unsigned long tilenum) { header.activationTile = tilenum; }

private:
    ScheduleHeaderPkt header;
};

class ScheduleElement : public SerializableMessage {
public:
    ScheduleElement() {
        std::memset(&content, 0, sizeof(ScheduleElementPkt));
    }

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
        content.period = static_cast<unsigned int>(period);
        content.offset = off;
    }

    // Constructor copying data from StreamInfo
    ScheduleElement(StreamInfo stream, unsigned int off=0) {
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
    StreamId getStreamId() const {
        return StreamId(content.src, content.dst, content.srcPort, content.dstPort);
    }
    //TODO: Save payloadSize, Direction, Redundancy in ScheduleElement
    StreamInfo getStreamInfo() const {
        return StreamInfo(content.src, content.dst, content.srcPort,
                          content.dstPort, static_cast<Period>(content.period),
                          0, Direction::TX, Redundancy::NONE, StreamStatus::ESTABLISHED);
    }
    unsigned char getSrc() const { return content.src; }
    unsigned char getDst() const { return content.dst; }
    unsigned char getSrcPort() const { return content.srcPort; }
    unsigned char getDstPort() const { return content.dstPort; }
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

class InfoElement : public SerializableMessage {
public:
    InfoElement() {
        std::memset(&content, 0, sizeof(ScheduleElementPkt));
    }
    // Constructor copying data from ScheduleElement
    // Used when parsing SchedulePacket that contains Info elements
    InfoElement(ScheduleElement s) {
        content.src = s.getSrc();
        content.dst = s.getDst();
        content.srcPort = s.getSrcPort();
        content.dstPort = s.getDstPort();
        // This is an info element and is characterized by TX=RX=0
        content.tx = 0;
        content.rx = 0;
        content.period = 0;
        // The message of the info element is saved in the offset field
        content.offset = s.getOffset();
    }
    // Constructor copying data from StreamId
    InfoElement(StreamId id, InfoType type) {
        content.src = id.src;
        content.dst = id.dst;
        content.srcPort = id.srcPort;
        content.dstPort = id.dstPort;
        // This is an info element and is characterized by TX=RX=0
        content.tx = 0;
        content.rx = 0;
        content.period = 0;
        // The message of the info element is saved in the offset field
        content.offset = static_cast<unsigned int>(type);
    };

    void serialize(Packet& pkt) const override;
    static InfoElement deserialize(Packet& pkt);
    std::size_t size() const override { return sizeof(ScheduleElementPkt); }
    StreamId getStreamId() const {
        return StreamId(content.src, content.dst, content.srcPort, content.dstPort);
    }
    unsigned char getSrc() const { return content.src; }
    unsigned char getDst() const { return content.dst; }
    unsigned char getSrcPort() const { return content.srcPort; }
    unsigned char getDstPort() const { return content.dstPort; }
    unsigned char getTx() const { return content.tx; }
    unsigned char getRx() const { return content.rx; }
    InfoType getType() const { return static_cast<InfoType>(content.offset); }

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
    ScheduleHeader getHeader() const { return header; }
    std::vector<ScheduleElement> getElements() const { return elements; }
private:
    ScheduleHeader header;
    std::vector<ScheduleElement> elements;
};

class ExplicitScheduleElement {
public:
    ExplicitScheduleElement() {
        action = Action::SLEEP;
        stream = StreamInfo();
    }
    ExplicitScheduleElement(Action action, StreamInfo stream) :
        action(action), stream(stream) {}

    Action getAction() const { return action; }
    StreamId getStreamId() const { return stream.getStreamId(); }
    StreamInfo getStreamInfo() const { return stream; }
private:
    Action action;
    StreamInfo stream;
};


} /* namespace mxnet */
