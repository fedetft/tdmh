/***************************************************************************
 *   Copyright (C) 2018-2019 by Federico Amedeo Izzo                       *
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
#include "../util/serializable_message.h"
#include "../stream/stream_management_element.h"
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
    SERVER_OPENED =0, // Signals that the master has accepted the new Server
    SERVER_CLOSED =1, // Signals that the master has rejected the new Server
    STREAM_REJECT =2  // Signals that the master has rejected the new Stream
};

struct ScheduleHeaderPkt {
    unsigned int totalPacket:16;
    unsigned int currentPacket:16;
    unsigned int scheduleID:32;
    unsigned int activationTile:32;
    unsigned int scheduleTiles:16;
    unsigned int repetition:8;
} __attribute__((packed));

struct ScheduleElementPkt {
    unsigned int tx:8;
    unsigned int rx:8;
    unsigned int offset:20;
} __attribute__((packed));

class ScheduleHeader : public SerializableMessage {
public:
    ScheduleHeader() {
        std::memset(&header, 0, sizeof(ScheduleHeaderPkt));
    }

    ScheduleHeader(unsigned int totalPacket, unsigned int currentPacket,
                   unsigned long scheduleID=0, unsigned long activationTile=0,
                   unsigned int scheduleTiles=0, unsigned char repetition=0)
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
    std::size_t size() const override { return maxSize(); }
    static std::size_t maxSize() { return sizeof(ScheduleHeaderPkt); }
    unsigned int getTotalPacket() const { return header.totalPacket; }
    unsigned int getCurrentPacket() const { return header.currentPacket; }
    // NOTE: schedule with ScheduleID=0 are not sent in MasterScheduleDistribution
    unsigned long getScheduleID() const { return header.scheduleID; }
    unsigned long getActivationTile() const { return header.activationTile; }
    unsigned int getScheduleTiles() const { return header.scheduleTiles; }
    unsigned char getRepetition() const { return header.repetition; }
    void incrementPacketCounter() { header.currentPacket++; }
    void incrementRepetition() {
            header.repetition++;
    }
    void resetPacketCounter() { header.currentPacket = 0; }
    void resetRepetition() { header.repetition = 0; }
    void setActivationTile(unsigned long tilenum) { header.activationTile = tilenum; }

private:
    ScheduleHeaderPkt header;
};

class ScheduleElement : public SerializableMessage {
public:
    ScheduleElement() {
        std::memset(&id, 0, sizeof(StreamId));
        std::memset(&params, 0, sizeof(StreamParameters));
        std::memset(&content, 0, sizeof(ScheduleElementPkt));
    }

    // Constructor for single-hop stream
    ScheduleElement(MasterStreamInfo stream, unsigned int off=0) {
        id = stream.getStreamId();
        params = stream.getParams();
        // If a ScheduleElement is created from a stream, then tx=src, rx=dst
        // because it is a single-hop transmission
        content.tx = id.src;
        content.rx = id.dst;
        content.offset = off;
    }

    // Constructor for multi-hop stream
    ScheduleElement(MasterStreamInfo stream,
                    unsigned char tx,
                    unsigned char rx,
                    unsigned int off=0) {
        id = stream.getStreamId();
        params = stream.getParams();
        content.tx = tx;
        content.rx = rx;
        content.offset = off;
    }

    void serialize(Packet& pkt) const override;
    static ScheduleElement deserialize(Packet& pkt);
    std::size_t size() const override { return maxSize(); }
    static std::size_t maxSize() {
        return (sizeof(StreamId) +
                sizeof(StreamParameters) +
                sizeof(ScheduleElementPkt)); }
    StreamId getStreamId() const { return id; }
    StreamParameters getParams() const { return params; }
    StreamInfo getStreamInfo() const {
        return StreamInfo(id, params, StreamStatus::ESTABLISHED);
    }
    unsigned char getSrc() const { return id.src; }
    unsigned char getDst() const { return id.dst; }
    unsigned char getSrcPort() const { return id.srcPort; }
    unsigned char getDstPort() const { return id.dstPort; }
    Period getPeriod() const { return params.getPeriod(); }
    unsigned char getTx() const { return content.tx; }
    unsigned char getRx() const { return content.rx; }
    unsigned int getOffset() const { return content.offset; }
    void setOffset(unsigned int off) { content.offset = off; }
    // return an unique key for each stream
    unsigned int getKey() const { return id.getKey(); }
protected:
    StreamId id;
    StreamParameters params;
    ScheduleElementPkt content;
};

class InfoElement : public ScheduleElement {
public:
    InfoElement() : ScheduleElement() {};
    // Constructor copying data from ScheduleElement
    // Used when parsing SchedulePacket that contains Info elements
    InfoElement(ScheduleElement s) {
        id = s.getStreamId();
        // Info elements do not carry parameter information
        std::memset(&params, 0, sizeof(StreamParameters));
        // This is an info element and is characterized by TX=RX=0
        content.tx = 0;
        content.rx = 0;
        // The message of the info element is saved in the offset field
        content.offset = s.getOffset();
    }
    // Constructor copying data from StreamId
    InfoElement(StreamId streamId, InfoType type) {
        id = streamId;
        // Info elements do not carry parameter information
        std::memset(&params, 0, sizeof(StreamParameters));
        // This is an info element and is characterized by TX=RX=0
        content.tx = 0;
        content.rx = 0;
        // The message of the info element is saved in the offset field
        content.offset = static_cast<unsigned int>(type);
    };
    static InfoElement deserialize(Packet& pkt);
    InfoType getType() const { return static_cast<InfoType>(content.offset); }
    // NOTE: do not use outside ScheduleElement(Infoelement i),
    // Info elements are not meant to carry offset information
    unsigned int getOffset() const { return content.offset; }
};

class SchedulePacket : public SerializableMessage {
public:
    SchedulePacket() {}

    SchedulePacket(ScheduleHeader hdr, std::vector<ScheduleElement> elms) :
        header(hdr), elements(elms) {}

    void serialize(Packet& pkt, unsigned short panId) const;
    static SchedulePacket deserialize(Packet& pkt);
    std::size_t size() const override {
        return panHeaderSize +
            header.size() +
            elements.size();
    }
    static unsigned int getPacketCapacity() {
        return (MediumAccessController::maxControlPktSize - (panHeaderSize + ScheduleHeader::maxSize()))
            / ScheduleElement::maxSize();
    }
    ScheduleHeader getHeader() const { return header; }
    std::vector<ScheduleElement> getElements() const { return elements; }
    void popElements(int n) {
        for(int i = 0; i < n; i++)
            elements.pop_back();
    }
    void setHeader(ScheduleHeader& newHeader) { header = newHeader; }
    void putElement(ScheduleElement& el) { elements.push_back(el); }
    void putInfoElement(InfoElement& el) { elements.push_back(static_cast<ScheduleElement>(el)); }

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
