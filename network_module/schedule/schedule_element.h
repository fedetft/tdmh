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
    
    ScheduleElement(unsigned int id, unsigned char src, unsigned char dst,
                    unsigned char srcPort, unsigned char dstPort,
                    Redundancy redundancy, Period period,
                    unsigned short payloadSize, int offset=0) : id(id), src(src), dst(dst),
                                                 srcPort(srcPort), dstPort(dstPort),
                                                 redundancy(redundancy), period(period),
                                                 payloadSize(payloadSize), offset(offset) {};

    // Constructor copying data from StreamManagementElement
    ScheduleElement(StreamManagementElement stream, int off=0) {
        id = stream.getKey();
        src = stream.getSrc();
        dst = stream.getDst();
        srcPort = stream.getSrcPort();
        dstPort = stream.getDstPort();
        redundancy = stream.getRedundancy();
        period = stream.getPeriod();
        payloadSize = stream.getPayloadSize();
        offset = off;
    };

    unsigned int getKey() const { return id; }
    unsigned char getSrc() const { return src; }
    unsigned char getDst() const { return dst; }
    Period getPeriod() const { return period; }
    int getOffset() const { return offset; }

    void setOffset(unsigned int off) { offset = off; }

    inline int toInt(Period x)
    {
        return static_cast<int>(x);
    }

private:
    unsigned int id;
    unsigned char src;
    unsigned char dst;
    unsigned char srcPort;
    unsigned char dstPort;
    Redundancy redundancy;
    Period period;
    unsigned short payloadSize;
    unsigned int offset;
};


} /* namespace mxnet */
