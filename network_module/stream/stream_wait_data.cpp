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

#include "stream_wait_data.h"

namespace mxnet {

StreamWakeupInfo::StreamWakeupInfo() : type(WakeupInfoType::EMPTY), id(StreamId()), wakeupTime(0), period(0) {}

StreamWakeupInfo::StreamWakeupInfo(WakeupInfoType t, StreamId sid, unsigned long long wt, unsigned long long p) : 
                                                   type(t), id(sid), wakeupTime(wt), period(p) {}

void StreamWakeupInfo::incrementWakeupTime() {
    // next time the stream relative to this StreamWakeupInfo
    // needs to be woken up, is the same wakeup time plus 
    // its period, converted to time (from an integer value)
    wakeupTime += period;
}

bool StreamWakeupInfo::operator<(const StreamWakeupInfo& other) const {
    
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

bool StreamWakeupInfo::operator==(const StreamWakeupInfo& other) const {
    return id == other.id &&
            type == other.type &&
            period == other.period &&
            wakeupTime == other.wakeupTime;
}

}  /* namespace mxnet */