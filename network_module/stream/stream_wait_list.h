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

/**
 * Wrapper of a lists of StreamWakeupInfo elements,
 * which provides some utility methods.
 */
class StreamWaitList {

public:
    StreamWaitList();
    StreamWaitList(const StreamWaitList& other);

    ~StreamWaitList();

    /**
     * @return current list index
     */
    unsigned int getIndex() const;

    /**
     * @return current list element
     */
    StreamWakeupInfo getElement();

    /**
     * @return the entire list 
     */
    const std::vector<StreamWakeupInfo>& get() const;

    /**
     * Set the streams list to the specified one.
     * @param l the new streams list 
     */
    void set(const std::vector<StreamWakeupInfo>& l);

    /**
     * Set the streams list to the specified one.
     * @param other the new streams list 
     */
    void set(const StreamWaitList& other);

    /**
     * Reset current list index to zero (first element).
     */
    void reset();
    
    /**
     * Increment the current list index in a circular way.
     */
    void incrementIndex();

    /**
     * @return boolean indicating if the current list is empty
     *         (i.e. does not contain any element).
     */
    bool isEmpty() const;

    /**
     * @return boolean indicating if the list index reached the last position.
     */
    bool isListEnd() const;

    /**
     * @param l1 first of the lists to be considered
     * @param l2 second of the lists to be considered
     * @return the StreamWakeupInfo element that has the minimum wakeup time
     * among the first elements of the two lists passed as parameters. 
     */
    static StreamWakeupInfo getMinWakeupInfo(StreamWaitList& l1, StreamWaitList& l2);

    /**
     * @param l1 first of the lists to be considered
     * @param l2 second of the lists to be considered
     * @return boolean indicating if all the elements of both lists have been used
     *         (i.e. the list index reached the last element in both lists)
     */
    static bool allListsElementsUsed(const StreamWaitList& l1, const StreamWaitList& l2);
    
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