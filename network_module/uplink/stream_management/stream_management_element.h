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
    class SMEId {
    public:
        SMEId() = delete;
        SMEId(unsigned char src, unsigned char dst) : src(src), dst(dst) {};
        unsigned char getSrc() { return src; }
        unsigned char getDst() { return dst; }
        bool operator ==(const SMEId& other) const {
            return src == other.src && dst == other.dst;
        }
        bool operator !=(const SMEId& other) const {
            return !(*this == other);
        }
    private:
        unsigned char src;
        unsigned char dst;
    };
    StreamManagementElement(unsigned char src, unsigned char dst, unsigned char dataRate) :
        content({src, dst, dataRate}), id(src, dst) {};
    virtual ~StreamManagementElement() {};
    static unsigned short getMaxSize() { return sizeof(StreamManagementElementPkt); }
    unsigned char getSrc() { return content.src; }
    unsigned char getDst() { return content.dst; }
    unsigned char getDataRate() { return content.dataRate; }
    SMEId getId() { return id; }
    bool operator ==(const StreamManagementElement& other) const {
        return content.src == other.content.src && content.dst == other.content.dst && content.dataRate == other.content.dataRate;
    }
    bool operator !=(const StreamManagementElement& other) const {
        return !(*this == other);
    }

    void serialize(const unsigned char* pkt) override;
    static std::vector<StreamManagementElement> deserialize(std::vector<unsigned char>& pkt);
    static std::vector<StreamManagementElement> deserialize(std::vector<unsigned char>::iterator pkt, std::size_t size);
    std::size_t getSize() override { return getMaxSize(); }
protected:
    StreamManagementElement() {};
    struct StreamManagementElementPkt {
        unsigned char src;
        unsigned char dst;
        unsigned char dataRate;
    } __attribute__((packed));
    StreamManagementElementPkt content;
    SMEId id;
};


} /* namespace mxnet */
