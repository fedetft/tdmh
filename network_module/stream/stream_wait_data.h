/***************************************************************************
 *   Copyright (C) 2021 by Luca Conterio                                   *
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

#include "stream.h"

#include <list>

namespace mxnet {

enum class WakeupInfoType : uint8_t {
    STREAM   = 0,
    DOWNLINK = 1,
    EMPTY    = 2
};

/**
 * Structure used to build a table with streams information.
 */
struct StreamWaitInfo {
    StreamId id;
    int period;
    int redundancy;
    std::list<unsigned int> offsets;
    unsigned int wakeupAdvance;

    StreamWaitInfo() : id(StreamId()), period(1), redundancy(1), wakeupAdvance(0) {}
    StreamWaitInfo(StreamId sid, int p, int r, std::list<unsigned int> o, unsigned int wa) : 
                            id(sid), period(p), redundancy(r), offsets(o), wakeupAdvance(wa) {}
};

/**
 * Structure used to build an ordered list of wakeup times for each stream.
 */
struct StreamWakeupInfo {
    WakeupInfoType type;
    StreamId id;
    unsigned int wakeupTime;

    StreamWakeupInfo() : type(WakeupInfoType::EMPTY), id(StreamId()), wakeupTime(0) {} 
    StreamWakeupInfo(WakeupInfoType t, StreamId sid, unsigned int wa) : type(t), id(sid), wakeupTime(wa) {}

    bool operator<(const StreamWakeupInfo& other) const {
        return wakeupTime < other.wakeupTime;
    }
};

}  /* namespace mxnet */