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
#include "../scheduler/schedule_element.h"
#include "stream_wait_data.h"
#include "stream_wait_list.h"
#include "../downlink_phase/timesync/networktime.h"

#include <mutex>

#ifdef _MIOSIX
#include <thread>
#else
#include "threads/OmnetMultithreading.h"
#endif

namespace mxnet {

/**
 * This class is used to computed the time at which every stream
 * has to be woken up, according the required advance.
 * The execution runs in a separate thread (active object).
 */
class StreamWaitScheduler {

public:
    StreamWaitScheduler(const MACContext& ctx_, const NetworkConfiguration& netConfig_, StreamManager& streamMgr_): 
                            ctx(ctx_), netConfig(netConfig_), streamMgr(streamMgr_) {}

    /**
     * Set the data structures needed for the algorithm to work, i.e. two ordered lists
     * containing all the streams to be woken up and their realtive wakeup time (in a superframe).
     * @param currList streams to be woken up and transmitting during the current superframe
     * @param nextList streams to be woken up in the current superframe, but trasmitting
     *                 during the next one
     */
    void setStreamsWakeupLists(const std::vector<StreamWakeupInfo>& currList, 
                                const std::vector<StreamWakeupInfo>& nextList);

    /**
     * Set the tile number at which a newly received schedule will become active.
     * @param activationTile the tile number at which the new schedule will be activated
     */
    void setScheduleActivationTile(const unsigned int activationTile);

    /**
     * Start the active object's thread.
     */
    void start();

    /**
     * The method executed by the activa object.
     * Needed tobe public for usage with the simulator, executed by OmnetThread class.
     */
    void run();

private:
    /**
     * Replace streams lists relative to an older schedule
     * with the ones realtive to the new one.
     */
    void updateLists();

    /**
     * @return a StreamWakeupInfo object containing all the information 
     *         needed to know the next time instant to wake up.
     */
    StreamWakeupInfo getNextWakeupInfo();

    /**
     * Transform a stream's wkaeup offset w.r.t. to the superframe,
     * into an absolute time value.
     * @param currSuperframeStartTime start time of the current superframe
     * @param nextSuperframeStartTime start time of the next superframe
     * @param wt stream's wakeup time (w.r.t. the superframe)
     */
    unsigned long long nextAbsoluteWakeupTime(unsigned long long currSuperframeStartTime, 
                                                unsigned long long nextSuperframeStartTime, 
                                                unsigned int wt);

    /**
     * Wakeup a specific stream.
     * @param sinfo StreamWakeupInfo relative to the stream to be woken up.
     */
    void wakeupStream(StreamWakeupInfo sinfo);

    /**
     * @return boolean indicating if all the elements from both the
     *         current and the next lists of streams have been used
     *         (i.e. the algorithm iterated over both lists completely)
     */
    bool allListsElementsUsed();

#ifndef _MIOSIX
    void printStatus(unsigned int tile);
    void printLists();
#else
    void printStatus(unsigned int tile) {}
    void printLists() {}
#endif

    /**
     * StreamWaitScheduler state machine possible states.
     */
    enum class StreamWaitSchedulerStatus : unsigned char {
        IDLE,
        AWAITING_ACTIVATION,
        ACTIVE
    };

    StreamWaitSchedulerStatus status = StreamWaitSchedulerStatus::IDLE;

    unsigned int scheduleActivationTile = 0;

    bool isLastTileBeforeScheduleActivation = false;
    bool useNextSuperFrameStartTime = false;
    bool newScheduleAvailable = false;

    // streams that have to send during the current superframe
    StreamWaitList currWakeupList;
    // streams that have to send during next tile but need to be 
    // woken up during the current superframe
    StreamWaitList nextWakeupList;

    // same as above but referred to a new schedule
    StreamWaitList newCurrWakeupList;
    StreamWaitList newNextWakeupList;

#ifdef _MIOSIX
    std::thread* swthread = nullptr;
#else
    OmnetMultithreading threadsFactory;
    OmnetThread* swthread = nullptr;
#endif

    const MACContext& ctx;
    const NetworkConfiguration& netConfig;
    StreamManager& streamMgr;

    std::mutex mutex;
};

}  /* namespace mxnet */