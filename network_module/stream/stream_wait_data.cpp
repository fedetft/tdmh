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
#include "../downlink_phase/timesync/networktime.h"

namespace mxnet {

StreamWakeupInfo::StreamWakeupInfo() : type(WakeupInfoType::NONE), id(StreamId()), wakeupTime(0) {}

StreamWakeupInfo::StreamWakeupInfo(WakeupInfoType t, StreamId sid, unsigned long long wt) : type(t), id(sid), wakeupTime(wt) {}

void StreamWakeupInfo::print() const {
    print_dbg("%-10lu %-13llu", id.getKey(), NetworkTime::fromLocalTime(wakeupTime).get());
    switch(type) {
        case WakeupInfoType::WAKEUP_STREAM:
            print_dbg(" STREAM \n"); 
            break;
        case WakeupInfoType::WAKEUP_DOWNLINK:
            print_dbg(" DOWNLINK \n"); 
            break;
        default:
            print_dbg(" ----- \n"); 
            break;
    }
}

bool StreamWakeupInfo::operator<(const StreamWakeupInfo& other) const {
    if (wakeupTime < other.wakeupTime) {
        return true;
    }
    else if (wakeupTime > other.wakeupTime) {
        return false;
    }
    // Streams have higher priority than downlinks:
    // if they have same wakeup time, streams must be ordered before
    else if (type == WakeupInfoType::WAKEUP_STREAM && other.type == WakeupInfoType::WAKEUP_DOWNLINK) {
        if (wakeupTime == other.wakeupTime) {
            return true;
        }
        // else {
        //     return false;
        // }
    }
    // else if (type == WakeupInfoType::WAKEUP_DOWNLINK && other.type == WakeupInfoType::WAKEUP_STREAM) {
    //     if (wakeupTime == other.wakeupTime) {
    //         return false;
    //     }
    //     else {
    //         return true;
    //     }
    // }

    return false;
}

bool StreamWakeupInfo::operator>(const StreamWakeupInfo& other) const {
    return wakeupTime > other.wakeupTime;
}

bool StreamWakeupInfo::operator==(const StreamWakeupInfo& other) const {
    return id == other.id &&
            type == other.type &&
            wakeupTime == other.wakeupTime;
}

}  /* namespace mxnet */