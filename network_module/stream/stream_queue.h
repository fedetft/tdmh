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
#include <queue>
#include <mutex>

namespace mxnet {

using stream_queue_data_t      = StreamWakeupInfo;
using stream_queue_container_t = std::vector<stream_queue_data_t>;
using stream_queue_compare_t   = std::greater<stream_queue_data_t>;

using stream_queue_t = std::priority_queue<stream_queue_data_t, 
                                           stream_queue_container_t, 
                                           stream_queue_compare_t>;

/**
 * Priority queue holding streams wakeup times.
 */
class StreamQueue : public stream_queue_t {

public:
    StreamQueue();
    StreamQueue(const StreamQueue& other);
    StreamQueue(const stream_queue_container_t& container);

    /**
     * @return the front priority queue element. If the queue
     *         is empty, an empty StreamWakeupInfo is returned
     */
    StreamWakeupInfo getElement() const;

    /**
     * Print the priority queue container elements.
     */
    void print();

    /**
     * Compare the two passed stream queues and return a pointer
     * to the one that contains the StreamWakeupInfo element with 
     * lower wakeup time. If a queue is empty, the other one is
     * returned. In case both queues are empty, the first one is 
     * always returned. 
     * @return pointer to the stream queue containing the lower
     *         wakeup time among the two
     */
    static StreamQueue* compare(StreamQueue& q1, StreamQueue& q2);
};

} /* namespace mxnet */