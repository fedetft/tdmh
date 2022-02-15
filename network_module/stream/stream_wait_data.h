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
 * Structure used to build an ordered list of streams wakeup times.
 */
struct StreamWakeupInfo {
    WakeupInfoType type;
    StreamId id;
    unsigned long long wakeupTime;
    unsigned long long period; // stream period converted to time (nanoseconds)

    StreamWakeupInfo(); 
    StreamWakeupInfo(WakeupInfoType t, StreamId sid, unsigned long long wt, unsigned long long p);

    /**
     * Update the current wakeup time. 
     * The result is the next wakeup time for the stream
     * associated to this object.
     */
    void incrementWakeupTime();

    /**
     * Used to build an ordered list of StreamWakeupInfo objects.
     * This method checks if "this" < "other", i.e. it checks if
     * priority of "this" > priority of "other".
     * If "this" < "other", it will be ordered before "other" in streams wakeup lists.
    
     * Ordering: 
     * - Objects with lower wakeup time, before objects with higher wakeup time
     * - Objects with lower period, before objects with higher period
     * - Streams before downlink objects
     * 
     * NOTE: this custom ordering is fundamental for the streams wakeup algorithm
     *       to work, in particular for handling streams that have different periods
     *       in the same node.
     * 
     * \param other object to be compared with this object
     * \return true if "this" < "other", false otherwise
     */
    bool operator<(const StreamWakeupInfo& other) const;

    /**
     * \param other object to be compared with this object
     * \return true if all members of "this" are equal to those of "other" 
     */
    bool operator==(const StreamWakeupInfo& other) const;
};

}  /* namespace mxnet */