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
 * Priority queue holding streams wakeup times.
 */
class StreamQueue {

public:
    StreamQueue();
    StreamQueue(const StreamQueue& other);
    StreamQueue(const std::vector<StreamWakeupInfo>& container);

    /**
     * Assign a StreamQueue to this one. The underlying container
     * and is size are assigned. This object's index is set to zero
     * (i.e. to the first element of the container).
     */
    void set(const std::vector<StreamWakeupInfo>& container);

    /**
     * @return reference to the underlying container
     */
    const std::vector<StreamWakeupInfo>& get() const;

    /**
     * @return the front queue element (i.e. the one at current index in the queue).
     *         If the queue is empty, an empty StreamWakeupInfo is returned
     */
    StreamWakeupInfo getElement() const;

    /**
     * @return the queue element following the front one.
     */
    StreamWakeupInfo getNextElement() const;

    /**
     * @return current queue index.
     */
    unsigned int getIndex() const;

    /**
     * @return index following the current queue index.
     */
    unsigned int getNextIndex() const;
    
    /**
     * Update current from element's wakeup time, 
     * according to its period, and move index, if needed.
     * The index is moved only if the element following the
     * current one has a wakeup time that is lower than
     * the current element's one.
     */
    void updateElement();

    /**
     * Increment wakeup time of all elements in the queue by the 
     * given amount of time.
     * @param t increment to be applied to queue elements
     */
    void applyTimeIncrement(unsigned long long t);

    /**
     * Check if a StreamWakeupInfo object exists in the queue.
     * @param sinfo the object whose existence has to be checked
     * @return true if an element equal to @param{sinfo} exists
     *          in the queue, false otherwise
     */
    bool contains(const StreamWakeupInfo& sinfo) const;

    /**
     * @return true if queue is empty, false otherwise
     */
    bool empty();

    /**
     * Print the queue elements.
     */
    void print() const;

    /**
     * Assign a StreamQueue to this one. The underlying container
     * and is size are assigned. The current index of @param{other}
     * is maintained and assigned too to the index of this object.
     * @param other the object to be assigned to this one
     */
    void operator=(const StreamQueue& other);

    /**
     * Compare the two passed stream queues and return a pointer
     * to the one that contains the StreamWakeupInfo element with 
     * lower wakeup time. If a queue is empty, the other one is
     * returned. In case both queues are empty, the first one is 
     * always returned. 
     * @param q1 the first of the queues to be compared
     * @param q2 the second of the queues to be compared
     * @return pointer to the stream queue containing the lower
     *         wakeup time among the two
     */
    static StreamQueue* compare(StreamQueue& q1, StreamQueue& q2);

private:
    /**
     * Update the index used to iterate over the queue,
     * go to the next index.
     */
    void updateIndex();
    
    /**
     * Reset current index to zero and queue size to
     * the actual size of the underlying container.
     */
    void reset();

    // Index used to iterate over the queue
    unsigned int index = 0;

    // Number of elements contained in the queue
    size_t queueSize = 0;

    std::vector<StreamWakeupInfo> queue;
};

} /* namespace mxnet */