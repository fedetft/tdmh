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
#include "stream_wakeup_data.h"
#include "stream_queue.h"
#include "../downlink_phase/timesync/networktime.h"

#include <mutex>

#ifdef _MIOSIX
#include <thread>
#else
#include "threads/OmnetMultithreading.h"
#endif

namespace mxnet {

/**
 * Structure used to hold information and streams lists
 * relative to a specific schedule.
 */
struct ScheduleWakeupData {

    // schedule activation tile and time instant
    unsigned int activationTile         = 0;
    unsigned long long activationTime   = 0;
    unsigned long long scheduleDuration = 0;
    unsigned int scheduleTiles          = 0;
    // streams that have to send during the current superframe
    StreamQueue currWakeupQueue;
    // streams that have to send during next tile but need to be
    // woken up during the current superframe
    StreamQueue nextWakeupQueue;

    /**
     * Set the two StreamQueue to the two given ones.
     * @param currQueue queue to be copied to currWakeupQueue
     * @param nextQueue queue to be copied to nextWakeupQueue
     */
    void set(const StreamQueue& currQueue, const StreamQueue& nextQueue) {
        currWakeupQueue = currQueue;
        nextWakeupQueue = nextQueue;
    }

    /**
     * Incremenet the wakeup times of all the elements in the two queues.
     * @param t increment to be applied to the queue elements.
     */
    void applyTimeIncrement(unsigned long long t) {
        currWakeupQueue.applyTimeIncrement(t);
        nextWakeupQueue.applyTimeIncrement(t);
    }

    /**
     * Copy members of the given objects to the ones of this object.
     * @param other the object from which members have to be copied
     */
    void operator=(const ScheduleWakeupData& other) {
        activationTile   = other.activationTile;
        activationTime   = other.activationTime;
        scheduleDuration = other.scheduleDuration;
        scheduleTiles    = other.scheduleTiles;
        currWakeupQueue  = other.currWakeupQueue;
        nextWakeupQueue  = other.nextWakeupQueue;
    }
};

/**
 * This class is used to computed the time at which every stream
 * has to be woken up, according the required advance.
 * The execution runs in a separate thread (active object).
 */
class StreamWakeupScheduler {

public:

    StreamWakeupScheduler(const MACContext& ctx_, const NetworkConfiguration& netConfig_, StreamManager& streamMgr_);

    /**
     * Set the data structures needed for the algorithm to work, i.e. two StreamQueues
     * containing all the streams to be woken up and their realtive wakeup time.
     * @param currList streams to be woken up and transmitting during the current superframe
     * @param nextList streams to be woken up in the current superframe, but trasmitting
     *                 during the next one
     */
    void setStreamsWakeupLists(const std::vector<StreamWakeupInfo>& currList, 
                                const std::vector<StreamWakeupInfo>& nextList);

    /**
     * Set the tile number at which a newly received schedule will become active.
     * @param header
     */
    void setScheduleHeader(const ScheduleHeader& header);

    /**
     * Start the active object's thread.
     */
    void start();

    /**
     * The method executed by the active object.
     * Needed tobe public for usage with the simulator, executed by OmnetThread class.
     */
    void run();

private:
    /**
     * Executed in the IDLE state.
     */
    void idle();

    /**
     * Executed in the AWAITING_ACTIVATION state.
     */
    void awaitingActivation(const StreamWakeupInfo& wakeupInfo);

    /**
     * Executed in the ACTIVE state.
     */
    void active(const StreamWakeupInfo& wakeupInfo);

    /**
     * Replace streams lists relative to an older schedule
     * with the ones realtive to the new one.
     */
    void applyNewSchedule();

    /**
     * Get the streams queue from which extracting
     * the next StreamWakeupInfo element.
     * @return pointer to the stream queue to be used
     */
    StreamQueue* getNextWakeupQueue();

    /**
     * @return return StreamWakeupInfo to be used at current iteration
     */
    StreamWakeupInfo getNextWakeupInfo();

    /**
     * Update the front element of the streams queue from
     * which the last element was extracted.
     */
    void updateLastUsedQueue();

    /**
     * Wakeup a specific stream.
     * @param sinfo StreamWakeupInfo relative to the stream to be woken up.
     */
    void wakeupStream(const StreamWakeupInfo& sinfo);

    /**
     * Starts the active object's thread.
     */
    static void threadLauncher(void *arg) {
        reinterpret_cast<StreamWakeupScheduler*>(arg)->run();
    }

    /**
     * Print state machine state.
     */
    void printState(unsigned int tile) const;

    /**
     * Print the given StreamQueues.
     * @param scheduleData the struct containing the stream queues to be printed
     */
    void printQueues(const ScheduleWakeupData& sData);

    /**
     * Print the StreamWakeupScheduler thread stack usage and size.
     */
    void printStack() const;

    /**
     * StreamWakeupScheduler state machine states.
     */
    enum class StreamWakeupSchedulerState : unsigned char {
        IDLE,                   // Waiting to receive the first schedule
        AWAITING_ACTIVATION,    // Schedule received but waiting for it to be activated
        ACTIVE                  // Schedule active
    };

    // State machine current state
    StreamWakeupSchedulerState state = StreamWakeupSchedulerState::IDLE;

    unsigned int currentTile             = 0; // Current tile index
    unsigned long long currTileStartTime = 0; // Time at which current tile started
    unsigned long long nextTileStartTime = 0; // Time at which next tile will start
    unsigned int superframeSize          = 0; // Number of tiles in control superframe

    // Boolean indicating if we are currently inside the last
    // tile before a new schedule activation
    bool isLastSuperframeBeforeScheduleActivation = false;

    // Boolean indicating if a new schedule has been received
    bool newScheduleAvailable = false;
    
    // Current schedule's relative data (activation time and streams queues)
    ScheduleWakeupData scheduleData;
    // Next schedule's relative data (activation time and streams queues)
    ScheduleWakeupData newScheduleData;

    // StreamQueue from which the last StreamWakeupInfo element was extracted
    StreamQueue* lastUsedQueue = nullptr;

    // Active object's thread
#ifdef _MIOSIX
    miosix::Thread* swthread = nullptr;
    unsigned int const THREAD_STACK = 4096;
#else
    OmnetMultithreading threadsFactory;
    OmnetThread* swthread = nullptr;
#endif

    const MACContext& ctx;
    const NetworkConfiguration& netConfig;
    StreamManager& streamMgr;

    // Mutex used to protect access to the StreamQueue objects
    std::mutex mutex;
};

}  /* namespace mxnet */