/***************************************************************************
 *   Copyright (C)  2019 by Federico Amedeo Izzo                           *
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

namespace mxnet {

enum class Redundancy
{
    NONE,            //No redundancy
    DOUBLE,          //Double redundancy, packets follow the same path
    DOUBLE_SPATIAL,  //Double redundancy, packets follow more than one path
    TRIPLE,          //Triple redundancy, packets follow the same path
    TRIPLE_SPATIAL   //Triple redundancy, packets follow more than one path
};

enum class Period
{
//  P0dot1,        //  0.1*tileDuration (currently unsupported)
//  P0dot2,        //  0.2*tileDuration (currently unsupported)
//  P0dot5,        //  0.5*tileDuration (currently unsupported)
    P1     =1,     //    1*tileDuration
    P2     =2,     //    2*tileDuration
    P5     =3,     //    5*tileDuration
    P10    =4,    //   10*tileDuration
    P20    =5,    //   20*tileDuration
    P50    =6,    //   50*tileDuration
    P100   =7,   //  100*tileDuration
    P200   =8,   //  200*tileDuration (currently unsupported)
    P500   =9,   //  500*tileDuration (currently unsupported)
    P1000  =10,  // 1000*tileDuration (currently unsupported)
    P2000  =11,  // 2000*tileDuration (currently unsupported)
    P5000  =12,  // 5000*tileDuration (currently unsupported)
    P10000 =13, //10000*tileDuration (currently unsupported)
};

/* Convert Period from 4-bit value to real value */
inline int toInt(Period x)
{
    int value = 0;
    switch(x){
    case Period::P1:
        value = 1;
        break;
    case Period::P2:
        value = 2;
        break;
    case Period::P5:
        value = 5;
        break;
    case Period::P10:
        value = 10;
        break;
    case Period::P20:
        value = 20;
        break;
    case Period::P50:
        value = 50;
        break;
    case Period::P100:
        value = 100;
        break;
    case Period::P200:
        value = 200;
        break;
    case Period::P500:
        value = 500;
        break;
    case Period::P1000:
        value = 1000;
        break;
    case Period::P2000:
        value = 2000;
        break;
    case Period::P5000:
        value = 5000;
        break;
    case Period::P10000:
        value = 10000;
        break;
    }
    return value;
}

enum class Direction
{
    TX,              // Only the server transmits data
    RX,              // Only the client transmits data
    TX_RX,           // The server and client transmit and receive data
};

/* This class contains all the parameters that distinguish a Stream.
   NOTE: use the getters to get the parameters in enum format (e.g Period)
   while you can use the public field directly to access the unsigned int format
   e.g (parameters.redundancy), to compare two parameters without double conversion */
class StreamParameters {
public:
    StreamParameters() : redundancy(0), period(0), payloadSize(0), direction(0) {}
    StreamParameters(Redundancy red, Period per,
                     unsigned short size, Direction dir) {
        redundancy=static_cast<unsigned int>(red);
        period=static_cast<unsigned int>(per);
        payloadSize=size;
        direction=static_cast<unsigned int>(dir);
    }
    /* Constructor used to create a StreamParameters from already packed data */
    StreamParameters(unsigned int redundancy,
                     unsigned int period,
                     unsigned int payloadSize,
                     unsigned int direction) : redundancy(redundancy),
                                               period(period),
                                               payloadSize(payloadSize),
                                               direction(direction) {};

    Redundancy getRedundancy() const { return static_cast<Redundancy>(redundancy); }
    Period getPeriod() const { return static_cast<Period>(period); }
    unsigned short getPayloadSize() const { return payloadSize; }
    Direction getDirection() const { return static_cast<Direction>(direction); }

    unsigned int redundancy:3;
    unsigned int period:4;
    unsigned int payloadSize:7;
    unsigned int direction:2;
} __attribute__((packed));

class StreamId {
public:
    StreamId() : src(0),dst(0),srcPort(0),dstPort(0) {}
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
    /**
     * @return the calculated StreamId of the server
     */
    StreamId getServerId() const {
        return StreamId(dst, dst, 0, dstPort);
    }
    /**
     * @return true if the StreamId belongs to a server
     */
    bool isServer() const {
        if(src == dst && srcPort == 0)
            return true;
        else
            return false;
    }
    /**
     * @return true if the StreamId belongs to a stream
     */
    bool isStream() const {
        return !isServer();
    }
    static StreamId fromBytes(unsigned char bytes[3]) {
        return StreamId(bytes[0], bytes[1], bytes[2]&0xf, bytes[2]>>4);
    }

    unsigned int src:8;
    unsigned int dst:8;
    unsigned int srcPort:4;
    unsigned int dstPort:4;
} __attribute__((packed));

/* This class contains the stream statuses as seen by the dynamic node */
enum class StreamStatus : uint8_t
{
     CONNECTING,      // Connect request sent by client
     CONNECT_FAILED,  // Connect request rejected by master
     ACCEPT_WAIT,     // Server-side Stream opened after receiving a schedule
     ESTABLISHED,     // Stream successfully accepted, routed and scheduled
     REMOTELY_CLOSED, // Stream closed after receiving a schedule
     REOPENED,        // Received a schedule containing a closed stream
     CLOSE_WAIT,      // Stream closed by user, waiting for schedule without it
     LISTEN_WAIT,     // Listen request sent by server
     LISTEN_FAILED,   // Listen request rejected by master
     LISTEN,          // Listen acknowledged by master
     UNINITIALIZED
};

/* This class contains the stream statuses as seen by the master node */
enum class MasterStreamStatus : uint8_t
    {
     ACCEPTED,        // Stream received by master and related server present
     ESTABLISHED,     // Stream received and scheduled by master
     REJECTED,        // Stream received by master and related server absent
     LISTEN,          // Listen received by master
    };


/**
 *  StreamInfo is used to save the status of a Stream internally to TDMH
 */
class StreamInfo {
public:
    StreamInfo() : status(StreamStatus::UNINITIALIZED) {}
    StreamInfo(StreamInfo info, StreamStatus st) : id(info.id),
                                                   parameters(info.parameters),
                                                   status(st) {}

    StreamInfo(StreamId id, StreamParameters params, StreamStatus status) : id(id),
                                                                            parameters(params),
                                                                            status(status) {}

    StreamId getStreamId() const { return id; }
    StreamParameters getParams() const { return parameters; }
    unsigned char getSrc() const { return id.src; }
    unsigned char getSrcPort() const { return id.srcPort; }
    unsigned char getDst() const { return id.dst; }
    unsigned char getDstPort() const { return id.dstPort; }
    Redundancy getRedundancy() const { return parameters.getRedundancy(); }
    Period getPeriod() const { return parameters.getPeriod(); }
    unsigned short getPayloadSize() const { return parameters.getPayloadSize(); }
    Direction getDirection() const { return parameters.getDirection(); }
    StreamStatus getStatus() const { return status; }
    unsigned int getKey() const { return id.getKey(); }
    void setStatus(StreamStatus s) { status=s; }
    void setParams(StreamParameters newParameters) { parameters=newParameters; }
    void setRedundancy(Redundancy r) { parameters.redundancy=static_cast<unsigned int>(r); }
    void setPeriod(Period p) { parameters.period=static_cast<unsigned int>(p); }

protected:
    StreamId id;
    StreamParameters parameters;
    StreamStatus status;
};

/**
 *  MasterStreamInfo is used to save the status of a Stream internally to TDMH
 */
class MasterStreamInfo {
public:
    MasterStreamInfo() {}
    MasterStreamInfo(MasterStreamInfo info, MasterStreamStatus st) : id(info.id),
                                                   parameters(info.parameters),
                                                   status(st) {}

    MasterStreamInfo(StreamId id, StreamParameters params, MasterStreamStatus status) : id(id),
                                                                            parameters(params),
                                                                            status(status) {}

    StreamId getStreamId() const { return id; }
    StreamParameters getParams() const { return parameters; }
    unsigned char getSrc() const { return id.src; }
    unsigned char getSrcPort() const { return id.srcPort; }
    unsigned char getDst() const { return id.dst; }
    unsigned char getDstPort() const { return id.dstPort; }
    Redundancy getRedundancy() const { return parameters.getRedundancy(); }
    Period getPeriod() const { return parameters.getPeriod(); }
    unsigned short getPayloadSize() const { return parameters.getPayloadSize(); }
    Direction getDirection() const { return parameters.getDirection(); }
    MasterStreamStatus getStatus() const { return status; }
    unsigned int getKey() const { return id.getKey(); }
    void setStatus(MasterStreamStatus s) { status=s; }
    void setRedundancy(Redundancy r) { parameters.redundancy=static_cast<unsigned int>(r); }
    void setPeriod(Period p) { parameters.period=static_cast<unsigned int>(p); }

protected:
    StreamId id;
    StreamParameters parameters;
    MasterStreamStatus status;
};


} /* namespace mxnet */
