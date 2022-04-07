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
    WAKEUP_STREAM   = 0,
    WAKEUP_DOWNLINK = 1,
    NONE            = 2
};

/**
 * Structure used to build an ordered list of streams wakeup times.
 */
struct StreamWakeupInfo {
    WakeupInfoType type;
    StreamId id;
    unsigned long long wakeupTime;

    StreamWakeupInfo(); 
    StreamWakeupInfo(WakeupInfoType t, StreamId sid, unsigned long long wt);

    /**
     * 
     */
    void print() const;

    /**
     * \param other object to be compared with this object
     * \return true if this object has a lower wakeup time than "other"
     */
    bool operator<(const StreamWakeupInfo& other) const;

    /**
     * \param other object to be compared with this object
     * \return true if this object has a higher wakeup time than "other"
     */
    bool operator>(const StreamWakeupInfo& other) const;

    /**
     * \param other object to be compared with this object
     * \return true if all members of "this" are equal to those of "other" 
     */
    bool operator==(const StreamWakeupInfo& other) const;
};

}  /* namespace mxnet */