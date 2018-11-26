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

#include "../uplink/stream_management/stream_management_element.h"

namespace mxnet {

class ScheduleElement {
public:
    ScheduleElement() {}
    
    ScheduleElement(unsigned int key, unsigned char src, unsigned char dst,
                    unsigned char srcPort, unsigned char dstPort,
                    Redundancy redundancy, Period period,
                    unsigned short payloadSize, unsigned int off=0)
    {
        id = key;
        offset = off;
        content.src = src;
        content.dst = dst;
        content.srcPort = srcPort;
        content.dstPort = dstPort;
        content.period=static_cast<unsigned int>(period);
        content.payloadSize=payloadSize;
        content.redundancy=static_cast<unsigned int>(redundancy);
    }

    // Constructor copying data from StreamManagementElement
    ScheduleElement(StreamManagementElement stream, unsigned int off=0) {
        id = stream.getKey();
        content.src = stream.getSrc();
        content.dst = stream.getDst();
        content.srcPort = stream.getSrcPort();
        content.dstPort = stream.getDstPort();
        content.redundancy = static_cast<unsigned int>(stream.getRedundancy());
        content.period = static_cast<unsigned int>(stream.getPeriod());
        content.payloadSize = stream.getPayloadSize();
        offset = off;
    };

    unsigned int getKey() const { return id; }
    unsigned char getSrc() const { return content.src; }
    unsigned char getDst() const { return content.dst; }
    Period getPeriod() const { return static_cast<Period>(content.period); }
    unsigned int getOffset() const { return offset; }
    void setOffset(unsigned int off) { offset = off; }

    inline int toInt(Period x)
    {
        return static_cast<int>(x);
    }

private:
    StreamManagementElementPkt content;
    unsigned int id;
    unsigned int offset;
};

} /* namespace mxnet */
