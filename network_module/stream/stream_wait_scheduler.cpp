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

#include "stream_wait_scheduler.h"
#include "../downlink_phase/timesync/networktime.h"
#include "../mac_context.h"

using namespace miosix;

namespace mxnet {

void StreamWaitScheduler::setStreamsWakeupLists(const std::vector<StreamWakeupInfo>& currList,
                                                    const std::vector<StreamWakeupInfo>& nextList) {
    std::unique_lock<std::mutex> lck(mutex);

    newScheduleData.currWakeupQueue = StreamQueue(currList);
    newScheduleData.nextWakeupQueue = StreamQueue(nextList);

    newScheduleAvailable = true;

    //if(ENABLE_STREAM_WAKEUP_SCHEDULER_INFO_DBG) {
        print_dbg("\n[S] N=%d, Set new lists, NT=%llu\n\n", 
                        netConfig.getStaticNetworkId(), NetworkTime::now().get());
    //}
}  

void StreamWaitScheduler::setScheduleActivationTile(const unsigned int activationTile) {
    std::unique_lock<std::mutex> lck(mutex);

    newScheduleData.activationTile = activationTile;
    newScheduleData.activationTime = activationTile * netConfig.getTileDuration();

    // convert activation time from network time to local time
    newScheduleData.activationTime = NetworkTime::fromNetworkTime(newScheduleData.activationTime).toLocalTime();

    //if(ENABLE_STREAM_WAKEUP_SCHEDULER_INFO_DBG) {
        print_dbg("[S] NT=%llu, Last activation time=%llu, Next activation time=%llu\n", 
                    NetworkTime::now().get(), NetworkTime::fromLocalTime(scheduleData.activationTime).get(), 
                    NetworkTime::fromLocalTime(newScheduleData.activationTime).get());
    //}
}

void StreamWaitScheduler::start() {
    if (swthread == nullptr) {
#ifdef _MIOSIX
        swthread = miosix::Thread::create(&StreamWaitScheduler::threadLauncher, THREAD_STACK, 
                                        miosix::MAIN_PRIORITY, this);
        printStackRange("StreamWaitScheduler");
#else
        swthread = threadsFactory.create(std::bind(&StreamWaitScheduler::run, this));
        swthread->start();

        if (ENABLE_STREAM_WAKEUP_SCHEDULER_INFO_DBG) {
            if (swthread->isStarted()) {
                print_dbg("[S] Thread %d started\n", swthread->getId());
            }
            else {
                print_dbg("[S] Error: thread %d not started\n", swthread->getId());
            }
        }
#endif
    }
}

void StreamWaitScheduler::run() {

    while(true) {
        
        // get current tile index, start time and next tile start time
        currentTile = ctx.getCurrentTileAndSlot(NetworkTime::now()).first;
        currTileStartTime = NetworkTime::fromNetworkTime(currentTile * netConfig.getTileDuration()).toLocalTime();
        nextTileStartTime = currTileStartTime + netConfig.getTileDuration();

        switch(state) {
            
            // Waiting to receive the first schedule
            case StreamWaitSchedulerState::IDLE:
            {
                if (ENABLE_STREAM_WAKEUP_SCHEDULER_INFO_DBG) {
                    printState(currentTile);
                }

                idle();
                break;
            }
            // Schedule received but waiting for it to be activated
            case StreamWaitSchedulerState::AWAITING_ACTIVATION:
            {   
                // True if current tile is the last one before the new schedule activation tile
                isLastTileBeforeScheduleActivation = (currentTile == newScheduleData.activationTile - 1);

                if (ENABLE_STREAM_WAKEUP_SCHEDULER_INFO_DBG) {
                    printState(currentTile);       
                    if (isLastTileBeforeScheduleActivation)
                        print_dbg("N=%d - last tile before activation - NT=%llu\n", netConfig.getStaticNetworkId(), NetworkTime::now().get());

                }

                // New schedule's activation time reached,
                // moved to ACTIVE state
                if (currentTile >= newScheduleData.activationTile) {
                    print_dbg("[S] N=%d, activation tile reached, move to ACTIVE state\n", netConfig.getStaticNetworkId());
                    state = StreamWaitSchedulerState::ACTIVE;
                    newScheduleAvailable = false;
                    updateScheduleData();
                    if (ENABLE_STREAM_WAKEUP_SCHEDULER_INFO_DBG) {
                        printQueues(scheduleData.currWakeupQueue, scheduleData.nextWakeupQueue);
                        printQueues(newScheduleData.currWakeupQueue, newScheduleData.nextWakeupQueue);
                    }
                }
                else {
                    StreamWakeupInfo wakeupInfo;
                    // get here the next wakeup time, so if needed to move to active state
                    // the streams lists are later updated here below
                    wakeupInfo = getNextWakeupInfo();

                    awaitingActivation(wakeupInfo);
                }

                break;
            }
            // Schedule active, regime state
            case StreamWaitSchedulerState::ACTIVE:
            {
                StreamWakeupInfo wakeupInfo = getNextWakeupInfo();
                // update wakeup info and push it to the correct queue
                // (i.e. the one from which it was removed)
                pushWakeupInfo(wakeupInfo);
                
                active(wakeupInfo);
                break;
            }
            
            default:
                ctx.sleepUntil(nextTileStartTime);
                break;
        }
    }
}

void StreamWaitScheduler::idle() {

    // If new schedule available change state to AWAITING_ACTIVATION
    if (newScheduleAvailable) {
        print_dbg("[S] N=%d, new schedule available, NT=%llu\n", 
                    netConfig.getStaticNetworkId(), NetworkTime::now().get());

        newScheduleAvailable = false;
        state = StreamWaitSchedulerState::AWAITING_ACTIVATION;
        
        printStack();
        if (ENABLE_STREAM_WAKEUP_SCHEDULER_INFO_DBG) {
            printQueues(scheduleData.currWakeupQueue, scheduleData.nextWakeupQueue);
            printQueues(newScheduleData.currWakeupQueue, newScheduleData.nextWakeupQueue);
        }
    }
    else {
        // do nothing, just wait until next tile
        ctx.sleepUntil(nextTileStartTime); 
    }
}

void StreamWaitScheduler::awaitingActivation(StreamWakeupInfo wakeupInfo) {
    
    auto when = wakeupInfo.wakeupTime;

    // If the next sleep should wait until a time instant that is greater than the
    // next schedule activation time, just wait until that activation time.
    //  At the next iteration the current tile will be grater than the schedule's 
    // activation tile and  the algorithm will update the streams lists and move to 
    // the ACTIVE state. Moreover if when > activation time, we don't have streams 
    // in the next list or we already "consumed" and handled them for this tile.
    if (when > newScheduleData.activationTime && state == StreamWaitSchedulerState::AWAITING_ACTIVATION) {
        if (ENABLE_STREAM_WAKEUP_SCHEDULER_INFO_DBG) {
            print_dbg("N=%d - when=%llu > next activation time, wait until next tile %llu \n", 
                        netConfig.getStaticNetworkId(), NetworkTime::fromLocalTime(when).get(), newScheduleData.activationTime);
        }
        
        ctx.sleepUntil(newScheduleData.activationTime);
    }
    else {
        // push updated wakeup info to correct queue
        pushWakeupInfo(wakeupInfo);

        switch (wakeupInfo.type) {
            case WakeupInfoType::STREAM:
                if (ENABLE_STREAM_WAKEUP_SCHEDULER_INFO_DBG) {
                    print_dbg("[S] N=%d, sleep until %llu (Stream %d), NT=%llu, \n", 
                                    netConfig.getStaticNetworkId(), NetworkTime::fromLocalTime(when).get(), 
                                    wakeupInfo.id.getKey(), NetworkTime::now().get());
                }
                // sleep until next stream's wakeup time
                ctx.sleepUntil(when);
                wakeupStream(wakeupInfo);
                break;
            case WakeupInfoType::DOWNLINK:
                // do nothing, we're still waiting for a schedule to be activated,
                // no other schedule can be received here, directly go to next iteration
                break;
            case WakeupInfoType::EMPTY:
                if (ENABLE_STREAM_WAKEUP_SCHEDULER_INFO_DBG) {
                    print_dbg("[S] N=%d, sleep until next tile %llu (Empty), NT=%llu, \n", 
                                    netConfig.getStaticNetworkId(), NetworkTime::fromLocalTime(nextTileStartTime).get()), 
                                    NetworkTime::now().get();
                }
                ctx.sleepUntil(nextTileStartTime);
                break;
            default:
                Thread::nanoSleep(ctx.getDataSlotDuration());
                break;
        }
    }
}

void StreamWaitScheduler::active(StreamWakeupInfo wakeupInfo) {

    if (ENABLE_STREAM_WAKEUP_SCHEDULER_INFO_DBG) {
        printState(currentTile);
        printQueues(scheduleData.currWakeupQueue, scheduleData.nextWakeupQueue);
        printQueues(newScheduleData.currWakeupQueue, newScheduleData.nextWakeupQueue);
        print_dbg("[S] Stream wakeup info : %u - %llu\n", wakeupInfo.id.getKey(), wakeupInfo.wakeupTime);
    }

    auto when = wakeupInfo.wakeupTime;

    switch(wakeupInfo.type) {
        case WakeupInfoType::STREAM:
            if (ENABLE_STREAM_WAKEUP_SCHEDULER_INFO_DBG) {
                print_dbg("[S] N=%d, sleep until %llu (Stream %d), NT=%llu, \n", 
                                netConfig.getStaticNetworkId(), NetworkTime::fromLocalTime(when).get(), 
                                wakeupInfo.id.getKey(), NetworkTime::now().get());
            }
            ctx.sleepUntil(when);
            wakeupStream(wakeupInfo);
            break;
        case WakeupInfoType::DOWNLINK:
            if (ENABLE_STREAM_WAKEUP_SCHEDULER_INFO_DBG) {
                print_dbg("[S] N=%d, sleep until %llu (Downlink), NT=%llu, \n", 
                                netConfig.getStaticNetworkId(), NetworkTime::fromLocalTime(when).get(), 
                                NetworkTime::now().get());
            }
            // wait until the end of the downlink slot
            ctx.sleepUntil(when); 
            // check if new schedule available and if needed change state
            if (newScheduleAvailable) {
                print_dbg("[S] N=%d, new schedule available, NT=%llu\n", 
                                netConfig.getStaticNetworkId(), NetworkTime::now().get());
                newScheduleAvailable = false;
                state = StreamWaitSchedulerState::AWAITING_ACTIVATION;
                printStack();
            }
            break;
        case WakeupInfoType::EMPTY:
            if (ENABLE_STREAM_WAKEUP_SCHEDULER_INFO_DBG) {
                print_dbg("[S] N=%d, sleep until next tile %llu (Empty), NT=%llu, \n", 
                                netConfig.getStaticNetworkId(), NetworkTime::fromLocalTime(nextTileStartTime).get()), 
                                NetworkTime::now().get();
            }
            ctx.sleepUntil(nextTileStartTime);
            break;
        default:
            Thread::nanoSleep(ctx.getDataSlotDuration());
            break;
    }
}

void StreamWaitScheduler::updateScheduleData() {
    std::unique_lock<std::mutex> lck(mutex);
    scheduleData.set(newScheduleData);

    if (ENABLE_STREAM_WAKEUP_SCHEDULER_INFO_DBG) {
        printQueues(scheduleData.currWakeupQueue, scheduleData.nextWakeupQueue);
        printQueues(newScheduleData.currWakeupQueue, newScheduleData.nextWakeupQueue);
    }

}

StreamQueue* StreamWaitScheduler::getNextWakeupQueue() {

    switch(state) {
        case StreamWaitSchedulerState::AWAITING_ACTIVATION:
            if (isLastTileBeforeScheduleActivation) {
                // use the curr list and the next list
                // relative to the newly received schedule
                return StreamQueue::compare(scheduleData.currWakeupQueue, newScheduleData.nextWakeupQueue);
            }
            else {
                // use the curr list and the next list
                // relative to the currently active schedule
                return StreamQueue::compare(scheduleData.currWakeupQueue, scheduleData.nextWakeupQueue);
            }
            break;
        case StreamWaitSchedulerState::ACTIVE:
            // compare both the curr and next lists and 
            // take the element with minimum wakeup time
            return StreamQueue::compare(scheduleData.currWakeupQueue, scheduleData.nextWakeupQueue);
            break;
        default:
            return StreamQueue::compare(scheduleData.currWakeupQueue, scheduleData.nextWakeupQueue);
            break;
    }

    return StreamQueue::compare(scheduleData.currWakeupQueue, scheduleData.nextWakeupQueue);
}

StreamWakeupInfo StreamWaitScheduler::getNextWakeupInfo() {
    std::unique_lock<std::mutex> lck(mutex);
    // get queue from which getting next element
    lastUsedQueue = getNextWakeupQueue();
    // get element from the correct queue
    StreamWakeupInfo sinfo = lastUsedQueue->getElement();
    
    return sinfo;
}

void StreamWaitScheduler::pushWakeupInfo(const StreamWakeupInfo& sinfo) {
    std::unique_lock<std::mutex> lck(mutex);

    if (sinfo.type != WakeupInfoType::EMPTY) {
        // remove element from the correct queue
        lastUsedQueue->getElement();
        // update element's wakeup time and add it to the queue again
        StreamWakeupInfo newInfo = updateWakeupTime(sinfo);
        lastUsedQueue->pushElement(newInfo);
    }
}

StreamWakeupInfo StreamWaitScheduler::updateWakeupTime(const StreamWakeupInfo& sinfo) const {
    StreamWakeupInfo updatedInfo = sinfo;

    // next time the stream relative to this StreamWakeupInfo
    // needs to be woken up, is the same wakeup time plus 
    // its period, converted to time (from an integer value)
    auto increment = sinfo.period * netConfig.getTileDuration();
    updatedInfo.wakeupTime = updatedInfo.wakeupTime + increment;

    return updatedInfo;
}

void StreamWaitScheduler::wakeupStream(const StreamWakeupInfo& sinfo) {
    if (sinfo.id.getKey() != 0) {
        bool res = streamMgr.wakeup(sinfo.id);
        if (ENABLE_STREAM_WAKEUP_SCHEDULER_INFO_DBG) {
            if (!res) {
                print_dbg("[S] Invalid stream ID to be woken up (ID = %u)\n", sinfo.id.getKey());
            }
        }
    }
}

void StreamWaitScheduler::printState(unsigned int tile) {
#ifndef _MIOSIX
    switch(state) {
        case StreamWaitSchedulerState::IDLE:
            print_dbg("[S] N=%d - State = IDLE - Tile = %u\n", netConfig.getStaticNetworkId(), tile);
            break;
        case StreamWaitSchedulerState::AWAITING_ACTIVATION:
            print_dbg("[S] N=%d - State = AWAITING_ACTIVATION - Tile = %u\n", netConfig.getStaticNetworkId(), tile);
            break;
        case StreamWaitSchedulerState::ACTIVE:
            print_dbg("[S] N=%d - State = ACTIVE - Tile = %u\n", netConfig.getStaticNetworkId(), tile);
            break;
        default:
            break;
    }
#endif
}

void StreamWaitScheduler::printQueues(StreamQueue& q1, StreamQueue& q2) {
#ifndef _MIOSIX
    std::unique_lock<std::mutex> lck(mutex);
    print_dbg("\n-----------------------------------------------\n");
    print_dbg("[S] N=%d - Wakeup lists\n", netConfig.getStaticNetworkId());
    q1.print();
    q2.print();
    print_dbg("-----------------------------------------------\n");
    print_dbg("\n");
#endif
}

void StreamWaitScheduler::printStack() {
#ifdef _MIOSIX
    unsigned int stackSize = miosix::MemoryProfiling::getStackSize();
    unsigned int absFreeStack = miosix::MemoryProfiling::getAbsoluteFreeStack();
    if (ENABLE_STACK_STATS_DBG)
        print_dbg("[H] StreamWaitScheduler stack %d/%d\n", stackSize - absFreeStack, stackSize);
#endif
}

} /* namespace mxnet */
