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

namespace mxnet {

/**
 * Structure used to build a table with streams offsets
 * and wakeup advance information.
 */
struct StreamOffsetInfo {
    StreamId id;
    int period;
    unsigned int offset;
    unsigned int wakeupAdvance;

    StreamOffsetInfo() : id(StreamId()), period(1), wakeupAdvance(0) {}
    StreamOffsetInfo(StreamId sid, int p, unsigned int o, unsigned int wa) : 
                            id(sid), period(p), offset(o), wakeupAdvance(wa) {}
};

/**
 * Structure used to indicate if the wakeup info struct
 * is referred to a stream, to a downlink slot
 * or none of those (i.e. just and empty structure).
 */
enum class WakeupInfoType : uint8_t {
    STREAM   = 0,
    DOWNLINK = 1,
    EMPTY    = 2
};

/**
 * Structure used to build an ordered list of wakeup times for each stream.
 */
struct StreamWakeupInfo {
    WakeupInfoType type;
    StreamId id;
    unsigned long long wakeupTime;
    int period;

    StreamWakeupInfo() : type(WakeupInfoType::EMPTY), id(StreamId()), wakeupTime(0), period(0) {} 
    StreamWakeupInfo(WakeupInfoType t, StreamId sid, long long wt, int p) : 
                                                   type(t), id(sid), wakeupTime(wt), period(p) {}

    bool operator<(const StreamWakeupInfo& other) const {
        //
        // check if "this" < "other"
        // i.e. check if "this" priority > "other" priority
        // => check if "this" will be ordered before "other"
        //    in streams wakeup list
        //
        // Ordering: 
        //  - elements with lower wakeup time, before elements with higher wakeup time
        //  - elements with lower period, before elements with higher period
        //  - streams before downlink elements
        //
        // TODO explain why this ordering is needed
        //
        
        if (wakeupTime < other.wakeupTime) {
            return true;
        }

        if (wakeupTime == other.wakeupTime) {
            if (period < other.period) {
                return true;
            }
        }

        if (wakeupTime == other.wakeupTime) {
            if (period == other.period) {
                if (type != other.type) {
                    if (type == WakeupInfoType::STREAM && other.type == WakeupInfoType::DOWNLINK)
                        return true;
                }
            }
        }

        return false;
        
    }

    bool operator>(const StreamWakeupInfo& other) const {
        return wakeupTime > other.wakeupTime;
    }
};

}  /* namespace mxnet */