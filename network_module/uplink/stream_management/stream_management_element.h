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
#include <vector>

namespace mxnet {

class StreamManagementElement : public SerializableMessage {
public:
    StreamManagementElement(unsigned char src, unsigned char dst, unsigned char srcPort,
                            unsigned char dstPort, Redundancy redundancy,
                            unsigned char period, unsigned char payloadSize) :
        src(src), dst(dst), srcPort(srcPort), dstPort(dstPort),
        redundancy(redundancy), period(period), payloadSize(payloadSize) {};
    virtual ~StreamManagementElement() {};

    void serialize(unsigned char* pkt) const override;
    static std::vector<StreamManagementElement*> deserialize(std::vector<unsigned char>& pkt);
    static std::vector<StreamManagementElement*> deserialize(unsigned char* pkt, std::size_t size);
    unsigned char getSrc() const { return src; }
    unsigned char getSrcPort() const { return srcPort; }
    unsigned char getDst() const { return dst; }
    unsigned char getDstPort() const { return dstPort; }
    Redundancy getRedundancy() const { return redundancy; }
    unsigned char getPeriod() const { return period; }
    unsigned short getPayloadSize() const { return payloadSize; }

    bool operator ==(const StreamManagementElement& other) const {
        return content.src == other.content.src && content.dst == other.content.dst && content.dataRate == other.content.dataRate;
    }
    bool operator !=(const StreamManagementElement& other) const {
        return !(*this == other);
    }

protected:
    StreamManagementElement() {};
    unsigned int src:8;
    unsigned int dst:8;
    unsigned int srcPort:4;
    unsigned int dstPort:4;
    unsigned int redundancy:3;
    unsigned int period:4;
    unsigned int payloadSize:9;
};


} /* namespace mxnet */
