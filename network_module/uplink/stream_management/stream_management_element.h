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
        friend class StreamManagementElement;
        SMEId(unsigned char src, unsigned char dst) : src(src), dst(dst) {};
        unsigned char getSrc() const { return src; }
        unsigned char getDst() const { return dst; }
        bool operator <(const SMEId& other) const {
            return src < other.src || (src == other.src && dst < other.dst);
        }
        bool operator >(const SMEId& other) const {
            return !(*this < other);
        }
        bool operator ==(const SMEId& other) const {
            return src == other.src && dst == other.dst;
        }
        bool operator !=(const SMEId& other) const {
            return !(*this == other);
        }
    protected:
        SMEId() {};
    private:
        unsigned char src;
        unsigned char dst;
    };
    StreamManagementElement(unsigned char src, unsigned char dst, unsigned char dataRate) :
        content({src, dst, dataRate}), id(src, dst) {};
    virtual ~StreamManagementElement() {};
    static unsigned short maxSize() { return sizeof(StreamManagementElementPkt); }
    unsigned char getSrc() const { return content.src; }
    unsigned char getDst() const { return content.dst; }
    unsigned char getDataRate() const { return content.dataRate; }
    SMEId getId() const { return id; }
    bool operator ==(const StreamManagementElement& other) const {
        return content.src == other.content.src && content.dst == other.content.dst && content.dataRate == other.content.dataRate;
    }
    bool operator !=(const StreamManagementElement& other) const {
        return !(*this == other);
    }

    void serialize(unsigned char* pkt) const override;
    static std::vector<StreamManagementElement*> deserialize(std::vector<unsigned char>& pkt);
    static std::vector<StreamManagementElement*> deserialize(unsigned char* pkt, std::size_t size);
    std::size_t size() const override { return maxSize(); }
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
