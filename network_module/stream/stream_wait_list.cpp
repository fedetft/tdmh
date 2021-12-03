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

#include "stream_wait_list.h"

namespace mxnet {

StreamWaitList::StreamWaitList() {}

StreamWaitList::StreamWaitList(const StreamWaitList& other) {
    set(other.get());
}

StreamWaitList::~StreamWaitList() {}

unsigned int StreamWaitList::getIndex() const {
    return listIndex;
}

const std::vector<StreamWakeupInfo>& StreamWaitList::get() const { 
    return list; 
}

StreamWakeupInfo StreamWaitList::getElement() {
    //std::unique_lock<std::mutex> lck(mutex);
    StreamWakeupInfo swi;

    if (isEmpty()) {
        listEnd = true;
        return swi;
    }

    if (!isEmpty()) {
        swi = list[listIndex];
    }
    
    return swi;
}

void StreamWaitList::set(const std::vector<StreamWakeupInfo>& l) {
    //std::unique_lock<std::mutex> lck(mutex);
    list = l;
    listSize = list.size();
    listIndex = 0;
}

void StreamWaitList::set(const StreamWaitList& other) {
    //std::unique_lock<std::mutex> lck(mutex);
    set(other.get());
    listIndex = other.getIndex();
}

void StreamWaitList::reset() {
    //std::unique_lock<std::mutex> lck(mutex);
    listEnd = false;
    listIndex = 0;
}

void StreamWaitList::incrementIndex() {
    //std::unique_lock<std::mutex> lck(mutex);

    if (listEnd) {
        listEnd = false;
    }

    listIndex = (listIndex + 1) % listSize;
    if (listIndex == 0) {
        listEnd = true;
    }
}

bool StreamWaitList::isEmpty() const { return listSize == 0; }

bool StreamWaitList::isListEnd() const { return listEnd; }

/*bool StreamWaitList::isLastElement() const { 
    if (isEmpty())
        return true;
    else
        return listIndex == listSize - 1; 
}*/

#ifndef _MIOSIX
void StreamWaitList::print() {
    //std::unique_lock<std::mutex> lck(mutex);
    print_dbg("ID \t\tWakeupTime \tType \n");
    for(auto e : list)
        print_dbg("%lu \t%u \t%d\n", e.id.getKey(), e.wakeupTime, e.type);
}
#endif

StreamWakeupInfo StreamWaitList::getMinWakeupInfo(StreamWaitList& currList,
                                                    StreamWaitList& nextList) {
    
    //print_dbg("curr list end = %d - next list end = %d\n", currList.isListEnd(), nextList.isListEnd());

    // both lists empty, return empty struct
    if (currList.isEmpty() && nextList.isEmpty()) {
        if (ENABLE_STREAM_WAKEUP_SCHEDULER_INFO_DBG)
            print_dbg("[S] Both curr and next lists empty\n");
        return StreamWakeupInfo{};
    }

    if (currList.isListEnd() && nextList.isListEnd()) {
        currList.reset();
        nextList.reset();
    }
    
    StreamWakeupInfo currSwi{};
    StreamWakeupInfo nextSwi{};

    // get first element of both lists (if not empty)
    if (!currList.isEmpty()) {
        currSwi = currList.getElement();
    }
    if (!nextList.isEmpty()) {
        nextSwi = nextList.getElement();
    }

    // if one of the two lists is empty, return the element from the other list
    if (currList.isEmpty()) {
        if (ENABLE_STREAM_WAKEUP_SCHEDULER_INFO_DBG)
            print_dbg("[S] Curr list empty : get element from next list\n");
        nextList.incrementIndex();
        return nextSwi;
    }
    else if (nextList.isEmpty()) {
        if (ENABLE_STREAM_WAKEUP_SCHEDULER_INFO_DBG)
            print_dbg("[S] Next list empty : get element from curr list\n");
        currList.incrementIndex();
        return currSwi;
    }

    // if already cycled over the entire curr list
    // return element from the other list (next list)
    if (currList.isListEnd()) {
        if (ENABLE_STREAM_WAKEUP_SCHEDULER_INFO_DBG)
            print_dbg("[S] Curr list is ended : get element from next list\n");
        nextList.incrementIndex();
        return nextSwi;
    }

    // otherwise, return the minimum among the two lists elements
    if (currSwi.wakeupTime <= nextSwi.wakeupTime) {
        if (ENABLE_STREAM_WAKEUP_SCHEDULER_INFO_DBG)
            print_dbg("[S] Get element from curr list\n");
        currList.incrementIndex();
        return currSwi;
    }
    else {
        if (ENABLE_STREAM_WAKEUP_SCHEDULER_INFO_DBG)
            print_dbg("[S] Get element from next list\n");
        nextList.incrementIndex();
        return nextSwi;
    }

    return currSwi;
}

bool StreamWaitList::allListsElementsUsed(const StreamWaitList& currList,
                                            const StreamWaitList& nextList) {
    // if used all elements from both lists
    //              OR
    // one of the two lists is empty,
    // then return true
    if (currList.isListEnd() && nextList.isListEnd())
        return true;

    if (currList.isListEnd() && nextList.isEmpty())
        return true;

    if (currList.isEmpty() && nextList.isListEnd())
        return true;

    // not used all the elements in both lists
    return false;
}

} /* namespace mxnet */