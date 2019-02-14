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

#include "../../serializable_message.h"
#include <string.h>
#include <vector>

namespace mxnet {

enum class Period
{
    P0dot1,        //  0.1*tileDuration (currently unsupported)
    P0dot2,        //  0.2*tileDuration (currently unsupported)
    P0dot5,        //  0.5*tileDuration (currently unsupported)
    P1     =1,     //    1*tileDuration
    P2     =2,     //    2*tileDuration
    P5     =5,     //    5*tileDuration
    P10    =10,    //   10*tileDuration
    P20    =20,    //   20*tileDuration
    P50    =50,    //   50*tileDuration
    P100   =100,   //  100*tileDuration
    P200   =200,   //  200*tileDuration (currently unsupported)
    P500   =500,   //  500*tileDuration (currently unsupported)
    P1000  =1000,  // 1000*tileDuration (currently unsupported)
    P2000  =2000,  // 2000*tileDuration (currently unsupported)
    P5000  =5000,  // 5000*tileDuration (currently unsupported)
    P10000 =10000, //10000*tileDuration (currently unsupported)
};

inline int toInt(Period x)
{
    return static_cast<int>(x);
}

enum class Redundancy
{
    NONE,            //No redundancy
    DOUBLE,          //Double redundancy, packets follow the same path
    DOUBLE_SPATIAL,  //Double redundancy, packets follow more than one path
    TRIPLE,          //Triple redundancy, packets follow the same path
    TRIPLE_SPATIAL   //Triple redundancy, packets follow more than one path
};

enum class StreamStatus
{
    NEW,             // TODO: remove after StreamManagementContext refactoring
    CLOSED,          // No stream opened, or stream closed
    LISTEN_REQ,      // Listen request sent by server
    LISTEN,          // Listen acknowledged by the master
    CONNECT_REQ,     // Connect request sent by client
    CONNECT,         // Connect acknowledged by the master
    ACCEPTED,        // Listen + Connect matched for the same Stream
    ESTABLISHED,     // Accepted stream successfully routed and scheduled
    REJECTED,        // Connect without Listen or routing/scheduling failed
};

enum class Direction
{
    TX,              // Only the server transmits data
    RX,              // Only the client transmits data
    TX_RX,           // The server and client transmit and receive data
};

class StreamId {
public:
    StreamId() {};
    StreamId(unsigned char src, unsigned char dst, unsigned char srcPort,
             unsigned char dstPort) : src(src), dst(dst), srcPort(srcPort),
                                      dstPort(dstPort) {}
    /**
     * @return a unique key for each stream
     */
    unsigned int getKey() const {
        return src | dst<<8 | srcPort<<16 | dstPort<<20;
    }
    /**
     * @return the comparison between the keys of the two StreamId
     */
    bool operator <(const StreamId& other) const {
        return getKey() < other.getKey();
    }
    bool operator ==(const StreamId& other) const {
        return getKey() == other.getKey();
    }

    unsigned int src:8;
    unsigned int dst:8;
    unsigned int srcPort:4;
    unsigned int dstPort:4;
} __attribute__((packed));

struct StreamParameters {
    unsigned int redundancy:3;
    unsigned int period:4;
    unsigned int payloadSize:7;
    unsigned int direction:2;
} __attribute__((packed));

struct SMEType {
    unsigned int type:4;
} __attribute__((packed));

/**
 *  StreamInfo is used to save the status of a Stream internally to TDMH
 */
class StreamInfo {
public:
    StreamInfo() {}
    
    StreamInfo(unsigned char src, unsigned char dst, unsigned char srcPort,
               unsigned char dstPort, Period period, unsigned char payloadSize,
               Direction direction, Redundancy redundancy=Redundancy::NONE,
               StreamStatus st=StreamStatus::CLOSED)
    {
        id.src=src;
        id.dst=dst;
        id.srcPort=srcPort;
        id.dstPort=dstPort;
        parameters.redundancy=static_cast<unsigned int>(redundancy);
        parameters.period=static_cast<unsigned int>(period);
        parameters.payloadSize=payloadSize;
        parameters.direction=static_cast<unsigned int>(direction);
        status=st;
    }

    StreamId getStreamId() const { return id; }
    StreamParameters getStreamParameters() const { return parameters; }
    unsigned char getSrc() const { return id.src; }
    unsigned char getSrcPort() const { return id.srcPort; }
    unsigned char getDst() const { return id.dst; }
    unsigned char getDstPort() const { return id.dstPort; }
    Redundancy getRedundancy() const { return static_cast<Redundancy>(parameters.redundancy); }
    Period getPeriod() const { return static_cast<Period>(parameters.period); }
    unsigned short getPayloadSize() const { return parameters.payloadSize; }
    Direction getDirection() const { return static_cast<Direction>(parameters.direction); }
    StreamStatus getStatus() const { return status; }
    void setStatus(StreamStatus s) { status=s; }
    unsigned int getKey() const { return id.getKey(); }

protected:
    StreamId id;
    StreamParameters parameters;
    StreamStatus status;
};

/**
 *  StreamManagementElement is the message that is sent on the network
 *  to manage the streams
 */
class StreamManagementElement : public SerializableMessage {
public:
    StreamManagementElement() {}

    StreamManagementElement(StreamInfo info, StreamStatus t)
    {
        id=info.getStreamId();
        parameters=info.getStreamParameters();
        type.type=static_cast<unsigned int>(t);
    }
   
    StreamManagementElement(unsigned char src, unsigned char dst,
                            unsigned char srcPort, unsigned char dstPort,
                            Period period, unsigned char payloadSize,
                            Direction direction, Redundancy redundancy,
                            StreamStatus st)
    {
        id.src=src;
        id.dst=dst;
        id.srcPort=srcPort;
        id.dstPort=dstPort;
        parameters.redundancy=static_cast<unsigned int>(redundancy);
        parameters.period=static_cast<unsigned int>(period);
        parameters.payloadSize=payloadSize;
        parameters.direction=static_cast<unsigned int>(direction);
        type.type=static_cast<unsigned int>(st);
    }

    virtual ~StreamManagementElement() {};

    void serialize(Packet& pkt) const override;
    static std::vector<StreamManagementElement> deserialize(Packet& pkt, std::size_t size);
    StreamId getStreamId() const { return id; }
    unsigned char getSrc() const { return id.src; }
    unsigned char getSrcPort() const { return id.srcPort; }
    unsigned char getDst() const { return id.dst; }
    unsigned char getDstPort() const { return id.dstPort; }
    Redundancy getRedundancy() const { return static_cast<Redundancy>(parameters.redundancy); }
    Period getPeriod() const { return static_cast<Period>(parameters.period); }
    unsigned short getPayloadSize() const { return parameters.payloadSize; }
    // TODO: remove once ScheduleComputation is using StreamInfo instead of StreamManagementElement
    StreamStatus getStatus() const { return getType(); }
    // TODO: remove once ScheduleComputation is using StreamInfo instead of StreamManagementElement
    void setStatus(StreamStatus s) { type.type = static_cast<unsigned int>(s); }
    StreamStatus getType() const { return static_cast<StreamStatus>(type.type); }

    bool operator ==(const StreamManagementElement& other) const {
        return (id == other.getStreamId() && 
               (memcmp(&parameters,&other.parameters,sizeof(StreamParameters))==0) &&
               (memcmp(&type,&other.type,sizeof(SMEType))==0));
    }
    bool operator !=(const StreamManagementElement& other) const {
        return !(*this == other);
    }
    /**
     * @return a unique key for each stream
     */
    unsigned int getKey() const { return id.getKey(); }

    static unsigned short maxSize() {
        return sizeof(StreamId) +
               sizeof(StreamParameters) +
               sizeof(SMEType);
    }

    std::size_t size() const override { return maxSize(); }

protected:
    StreamId id;
    StreamParameters parameters;
    SMEType type;
};


} /* namespace mxnet */
