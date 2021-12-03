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

#include "stream_wait_data.h"

#include <vector>
#include <mutex>

namespace mxnet {

class StreamWaitList {

public:
    StreamWaitList();
    StreamWaitList(const StreamWaitList& other);

    ~StreamWaitList();

    unsigned int getIndex() const;
    StreamWakeupInfo getElement();
    const std::vector<StreamWakeupInfo>& get() const;

    //StreamWakeupInfo getMinWakeupInfo();

    void set(const std::vector<StreamWakeupInfo>& l);
    void set(const StreamWaitList& other);

    void reset();
    
    void incrementIndex();

    bool isEmpty() const;

    bool isListEnd() const;

    //bool isLastElement() const;

    static StreamWakeupInfo getMinWakeupInfo(StreamWaitList& currList,
                                                StreamWaitList& nextList);

    static bool allListsElementsUsed(const StreamWaitList& currList,
                                        const StreamWaitList& nextList);
    
#ifndef _MIOSIX
    void print();
#endif

private:

    std::vector<StreamWakeupInfo> list;
    size_t listSize = 0;
    unsigned int listIndex = 0;
    bool listEnd = false;

    std::mutex mutex;
};

} /* namespace mxnet */