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

#include "stream_queue.h"
#include "../downlink_phase/timesync/networktime.h"

namespace mxnet
{

StreamQueue::StreamQueue() {}

StreamQueue::StreamQueue(const StreamQueue& other) : queue(other.get()) {}

StreamQueue::StreamQueue(const std::vector<StreamWakeupInfo>& container) : queue(container) {}

void StreamQueue::set(const std::vector<StreamWakeupInfo>& container) {
    queue = container;
    reset();
}

const std::vector<StreamWakeupInfo>& StreamQueue::get() const {
    return queue;
}

StreamWakeupInfo StreamQueue::getElement() const {
    if (queue.empty()) {
        return StreamWakeupInfo{};
    }
    else {
        return queue[index];
    }
}

StreamWakeupInfo StreamQueue::getNextElement() const {
    if (queue.empty()) {
        return StreamWakeupInfo{};
    }
    else {
        unsigned int nextIndex = getNextIndex();
        return queue[nextIndex];
    }
}

unsigned int StreamQueue::getIndex() const {
    return index;
}

unsigned int StreamQueue::getNextIndex() const {
    auto nextIndex = index + 1;

    if (nextIndex == queueSize)
        nextIndex = 0;
        
    return nextIndex;
}

void StreamQueue::updateElement() {
    // update front element's wakeup time, only if 
    // that element exists (i.e. the queue is not empty)
    // if (queue[index].type != WakeupInfoType::EMPTY) {
    if (!queue.empty()) {
        queue[index].incrementWakeupTime();

        // move index only if next element has a
        // wakeup time < than the current one
        if (getNextElement().wakeupTime < queue[index].wakeupTime) {
            updateIndex();
        }
    }
}

void StreamQueue::applyTimeIncrement(unsigned long long t) {
    if (!queue.empty()) {
        for(unsigned int i = 0; i < queueSize; i++) {
            queue[i].wakeupTime += t;
        }
    }
}

bool StreamQueue::contains(const StreamWakeupInfo& sinfo) const {
    for(auto e : queue) {
        if (e == sinfo)
            return true;
    }

    return false;
}

bool StreamQueue::empty() {
    return queueSize == 0;
}

void StreamQueue::print() const {
#ifndef _MIOSIX
    char header[4][11] = {"ID", "WakeupTime", "Period", "Type"};
    char emptyChar = '-';
    print_dbg("%-10s %-13s %-10s %-10s\n", header[0], header[1], header[2], header[3]);
    if (queue.empty()) {
        print_dbg("%-10c %-13c %-10c %-10c\n", emptyChar, emptyChar, emptyChar, emptyChar);
    }
    else {
        for(auto e : queue) {
            print_dbg("%-10lu %-13llu %-10d", e.id.getKey(), NetworkTime::fromLocalTime(e.wakeupTime).get(), e.period);
            switch(e.type) {
                case WakeupInfoType::STREAM:
                    print_dbg(" STREAM \n"); 
                    break;
                case WakeupInfoType::DOWNLINK:
                    print_dbg(" DOWNLINK \n"); 
                    break;
                default:
                    print_dbg(" ----- \n"); 
                    break;
            }
        }
    }
#endif
}

void StreamQueue::operator=(const StreamQueue& other) {
    set(other.get());
    // keep same index as "other"'s one
    index = other.getIndex();
}

StreamQueue* StreamQueue::compare(StreamQueue& q1, StreamQueue& q2) {
    
    // if both lists empty, just return one of the two lists
    if (q1.empty() && q2.empty()) {
        // if (ENABLE_STREAM_WAKEUP_SCHEDULER_INFO_DBG)
        //     print_dbg("[S] Both curr and next lists empty\n");
        return &q1;
    }

    // if one of the two lists is empty, return the element from the other list
    if (q1.empty()) {
        // if (ENABLE_STREAM_WAKEUP_SCHEDULER_INFO_DBG)
        //     print_dbg("[S] First list empty : get element from second list\n");
        return &q2;
    }
    else if (q2.empty()) {
        // if (ENABLE_STREAM_WAKEUP_SCHEDULER_INFO_DBG)
        //     print_dbg("[S] Second list empty : get element from first list\n");
        return &q1;
    }

    StreamWakeupInfo swi1{};
    StreamWakeupInfo swi2{};

    // get first element of both lists (if not empty)
    if (!q1.empty()) {
        swi1 = q1.getElement();
    }
    if (!q2.empty()) {
        swi2 = q2.getElement();
    }

    // return the minimum among the two lists elements
    if (swi1.wakeupTime <= swi2.wakeupTime) {
        // if (ENABLE_STREAM_WAKEUP_SCHEDULER_INFO_DBG)
        //     print_dbg("[S] Get element from first list\n");
        return &q1;
    }
    else {
        // if (swi1.wakeupTime == swi2.wakeupTime) {
        //     print_dbg("Two equal wakeup times...\n");
        // }

        // if (ENABLE_STREAM_WAKEUP_SCHEDULER_INFO_DBG)
        //     print_dbg("[S] Get element from second list\n");
        return &q2;
    }

    return &q1;
}

void StreamQueue::updateIndex() {
    if (!queue.empty()) {
        index = getNextIndex();
    }
}

void StreamQueue::reset() {
    index = 0;
    queueSize = queue.size();
}
    
} // namespace mxnet
