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

#pragma once

#include "../util/serializable_message.h"
#include "stream_parameters.h"
#include <string.h>
#include <vector>
#ifdef _MIOSIX
#include <interfaces/atomic_ops.h>
#else //_MIOSIX
#include <bits/atomic_word.h>
#include <bits/atomic_lockfree_defines.h>
#endif //_MIOSIX

/**
 * Enable adding a sequence number to each SME to detect SME losses in the uplink
 */
#define WITH_SME_SEQNO

namespace mxnet {

enum class SMEType : unsigned char
{
    CONNECT =0,          // Request to open a new stream
    LISTEN =1,           // Request to open a new server
    CLOSED =2,           // Request to close the stream or server
    RESEND_SCHEDULE = 3  // Request to resend the schedule
};

inline const char *smeTypeToString(SMEType s)
{
    switch(s)
    {
        case SMEType::CONNECT:         return "CONNECT";
        case SMEType::LISTEN:          return "LISTEN";
        case SMEType::CLOSED:          return "CLOSED";
        case SMEType::RESEND_SCHEDULE: return "RESEND_SCHEDULE";
        default:                       return "UNKNOWN";
    }
}

/**
 * Perform the grouping of SMETypes into SME classes
 * \param s SMEType
 * \return SME class
 */
inline unsigned char smeTypeToClass(SMEType s)
{
    switch(s)
    {
        case SMEType::CONNECT:         return 0; // Class 0, stream/server control
        case SMEType::LISTEN:          return 0;
        case SMEType::CLOSED:          return 0;
        case SMEType::RESEND_SCHEDULE: return 1; // Class 1, schedule control
        default:                       return 255;
    }
}

/**
 * The SME key is used to put SMEs in UpdatableQueues. The main property of the
 * UpdatableQueue is that if a new element with the same key is inserted in the
 * queue, it overwrites the old element. It also isn't put at the end of the
 * queue, but takes the place of the old element.
 * The key of a SME is the StreamId and the SME class. The StreamId is
 * is self-evident, SMEs for different streams shouldn't overwrite each other,
 * the SME class is used to allow grouping SME types in classes, where SMEs
 * with the same class (and the same StreamId) can overwrite each other.
 * For example, if a stream sends a CONNECT, followed by a CLOSED (due to
 * timeout) and a new CONNECT, all with the same StreamId, those SMES overwrite
 * each other. While on the other hand, a RESEND_SCHEDULE does not overwrite
 * any of those.
 */
struct SMEKey {
    StreamId id;
    unsigned char smeClass;

    SMEKey(StreamId id, SMEType type) : id(id), smeClass(smeTypeToClass(type)) {}

    bool operator <(const SMEKey& other) const {
        return getKey() < other.getKey();
    }

    unsigned int getKey() const {
        return id.getKey() | static_cast<unsigned int>(smeClass)<<24;
    }
};



/**
 *  StreamManagementElement is the message that is sent on the network
 *  to manage the streams
 */
class StreamManagementElement : public SerializableMessage {
public:
    StreamManagementElement() {}

    StreamManagementElement(StreamInfo info, SMEType t)
        : id(info.getStreamId()), parameters(info.getParams()), type(t)
#ifdef WITH_SME_SEQNO
#ifdef _MIOSIX
        , seqNo(miosix::atomicAddExchange(&seqCounter,1))
#else //_MIOSIX
        , seqNo(__atomic_add_fetch(&seqCounter,1,__ATOMIC_ACQ_REL))
#endif //_MIOSIX
#endif //WITH_SME_SEQNO
    {}
    
    static StreamManagementElement makeResendSME(unsigned char nodeId)
    {
        StreamManagementElement result;
        result.type = SMEType::RESEND_SCHEDULE;
        result.id = StreamId(nodeId,0,0,0);
#ifdef WITH_SME_SEQNO
#ifdef _MIOSIX
        result.seqNo = miosix::atomicAddExchange(&seqCounter,1);
#else //_MIOSIX
        result.seqNo = __atomic_add_fetch(&seqCounter,1,__ATOMIC_ACQ_REL);
#endif //_MIOSIX
#endif //WITH_SME_SEQNO
        return result;
    }

    void serialize(Packet& pkt) const override;
    void deserialize(Packet& pkt) override;
    StreamId getStreamId() const { return id; }
    StreamParameters getParams() const { return parameters; }
    unsigned char getSrc() const { return id.src; }
    unsigned char getSrcPort() const { return id.srcPort; }
    unsigned char getDst() const { return id.dst; }
    unsigned char getDstPort() const { return id.dstPort; }
    Redundancy getRedundancy() const { return static_cast<Redundancy>(parameters.redundancy); }
    Period getPeriod() const { return static_cast<Period>(parameters.period); }
    unsigned short getPayloadSize() const { return parameters.payloadSize; }
    SMEType getType() const { return type; }

    bool operator ==(const StreamManagementElement& other) const {
        return (id == other.getStreamId() && 
               memcmp(&parameters,&other.parameters,sizeof(StreamParameters))==0 &&
               type == other.type);
    }
    bool operator !=(const StreamManagementElement& other) const {
        return !(*this == other);
    }
    /**
     * @return a unique key for each stream
     */
    SMEKey getKey() const { return SMEKey(id, type); }

    static unsigned short maxSize() {
        return sizeof(StreamId)
             + sizeof(StreamParameters)
             + sizeof(SMEType)
#ifdef WITH_SME_SEQNO
             + sizeof(seqNo)
#endif //WITH_SME_SEQNO
             ;
    }

    std::size_t size() const override { return maxSize(); }

    static bool validateInPacket(Packet& packet, unsigned int offset, unsigned short maxNodes);
    
#ifdef WITH_SME_SEQNO
    unsigned short getSeqNo() const { return seqNo; }
#endif //WITH_SME_SEQNO

private:
    StreamId id;
    StreamParameters parameters;
    SMEType type;
#ifdef WITH_SME_SEQNO
    unsigned short seqNo;
    static int seqCounter;
#endif //WITH_SME_SEQNO
};


} /* namespace mxnet */
