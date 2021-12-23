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

    newCurrWakeupList.set(currList);
    newNextWakeupList.set(nextList);

    newScheduleAvailable = true;

    if(ENABLE_STREAM_WAKEUP_SCHEDULER_INFO_DBG) {
        print_dbg("\n[S] Node : %d - set new lists - NT=%llu\n\n", netConfig.getStaticNetworkId(), NetworkTime::now().get());
        printLists();
    }
}  

void StreamWaitScheduler::setScheduleActivationTile(const unsigned int activationTile) {
    std::unique_lock<std::mutex> lck(mutex);

    scheduleActivationTile = activationTile;
    
    if (ENABLE_STREAM_WAKEUP_SCHEDULER_INFO_DBG) {
        print_dbg("[S] Next activation tile : %u\n", scheduleActivationTile);
    }
}

void StreamWaitScheduler::start() {
    if (swthread == nullptr) {
#ifdef _MIOSIX
        swthread = new std::thread(&StreamWaitScheduler::run, this);
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
    unsigned int superframeSize = netConfig.getControlSuperframeStructure().size();
    unsigned long long superframeDuration = superframeSize * netConfig.getTileDuration();

    while(true) {

        unsigned int currentTile = ctx.getCurrentTileAndSlot(NetworkTime::now()).first;

        unsigned long long currTileStartTime = currentTile * netConfig.getTileDuration();
        unsigned long long nextTileStartTime = currTileStartTime + netConfig.getTileDuration();

        // "index" of the current control superframe (current tile over number of tile in superframe)
        unsigned int currSuperframeIndex = currentTile / superframeSize;
        unsigned long long currSuperframeStartTime = currSuperframeIndex * superframeDuration;
        unsigned long long nextSuperframeStartTime = currSuperframeStartTime + superframeDuration;
        
        // if current tile is the last before new schedule activation,
        // go to AWAITING_ACTIVATION state and use the new schedule's wakeup lists
        isLastTileBeforeScheduleActivation = (currentTile >= scheduleActivationTile - 1);

        switch(status) {
    
            case StreamWaitSchedulerStatus::IDLE:
                // check if new schedule available and if needed change state
                if (newScheduleAvailable) {
                    print_dbg("\nNode : %d - NEW SCHEDULE AVAILABLE - NT=%llu\n\n", netConfig.getStaticNetworkId(), NetworkTime::now().get());
                    newScheduleAvailable = false;
                    status = StreamWaitSchedulerStatus::AWAITING_ACTIVATION;
                }
                else {
                    // do nothing, just wait until a schedule is received
                    ctx.sleepUntil(nextTileStartTime);
                }
                break;

            case StreamWaitSchedulerStatus::AWAITING_ACTIVATION:
            {
                if (ENABLE_STREAM_WAKEUP_SCHEDULER_INFO_DBG) {
                    printStatus(currentTile);
                }

                StreamWakeupInfo wakeupInfo;

                if (isLastTileBeforeScheduleActivation) {
                    wakeupInfo = getNextWakeupInfo();
                    if (ENABLE_STREAM_WAKEUP_SCHEDULER_INFO_DBG) {
                        print_dbg("[S] Use next streams wakeup list\n");
                        print_dbg("[S] Stream wakeup info : %u - %u\n", wakeupInfo.id.getKey(), wakeupInfo.wakeupTime);
                    }
                }
                else if (currentTile >= scheduleActivationTile) {
                    status = StreamWaitSchedulerStatus::ACTIVE;
                    newScheduleAvailable = false;
                    updateLists();
                    wakeupInfo = getNextWakeupInfo();
                    if (ENABLE_STREAM_WAKEUP_SCHEDULER_INFO_DBG) {
                        print_dbg("[S] Stream wakeup info : %u - %u\n", wakeupInfo.id.getKey(), wakeupInfo.wakeupTime);
                        //print_dbg("NT=%llu - Curr superframe start=%llu - Curr tile start=%llu\n\n", NetworkTime::now().get(), currSuperframeStartTime, currTileStartTime);
                        printLists();
                    }
                }

                unsigned long long when = nextAbsoluteWakeupTime(currSuperframeStartTime, nextSuperframeStartTime, wakeupInfo.wakeupTime);
                
                switch (wakeupInfo.type) {
                    case WakeupInfoType::STREAM:
                        if (ENABLE_STREAM_WAKEUP_SCHEDULER_INFO_DBG) {
                            print_dbg("[S] Node : %d - NT=%llu - Curr SleepUntil=%llu\n\n", netConfig.getStaticNetworkId(), NetworkTime::now().get(), currTileStartTime + wakeupInfo.wakeupTime);
                        }
                        ctx.sleepUntil(when);
                        wakeupStream(wakeupInfo);
                        break;
                    case WakeupInfoType::EMPTY:
                        ctx.sleepUntil(nextTileStartTime);
                        break;
                    default:
                        Thread::nanoSleep(ctx.getDataSlotDuration());
                        break;
                }

                break;
            }

            case StreamWaitSchedulerStatus::ACTIVE:
            {
                StreamWakeupInfo wakeupInfo = getNextWakeupInfo();

                if (ENABLE_STREAM_WAKEUP_SCHEDULER_INFO_DBG) {
                    printStatus(currentTile);
                    print_dbg("[S] Stream wakeup info : %u - %u\n", wakeupInfo.id.getKey(), wakeupInfo.wakeupTime);
                }

                unsigned long long when = nextAbsoluteWakeupTime(currSuperframeStartTime, nextSuperframeStartTime, wakeupInfo.wakeupTime);
                    
                if (ENABLE_STREAM_WAKEUP_SCHEDULER_INFO_DBG) {
                    print_dbg("[S] NT=%llu - Curr tile start=%llu\n", NetworkTime::now().get(), currTileStartTime);
                    print_dbg("[S] Curr superframe start=%llu - Next superframe start=%llu\n", currSuperframeStartTime, nextSuperframeStartTime);                    
                    print_dbg("[S] Node : %d - NT=%llu - SleepUntil=%llu\n\n", netConfig.getStaticNetworkId(), NetworkTime::now().get(), when);
                }

                switch(wakeupInfo.type) {
                    case WakeupInfoType::STREAM:
                        ctx.sleepUntil(when);
                        wakeupStream(wakeupInfo);
                        break;
                    case WakeupInfoType::EMPTY:
                        ctx.sleepUntil(nextTileStartTime);
                        break;
                    case WakeupInfoType::DOWNLINK:
                        // wait until the end of the downlink slot
                        ctx.sleepUntil(when);   
                        //print_dbg("\nNode : %d - DOWNLINK SLOT - NT=%llu\n\n", netConfig.getStaticNetworkId(), NetworkTime::now().get());
                        // check if new schedule available and if needed change state
                        if (newScheduleAvailable) {
                            if (ENABLE_STREAM_WAKEUP_SCHEDULER_INFO_DBG) {
                                print_dbg("\nNode : %d - NEW SCHEDULE AVAILABLE - NT=%llu\n\n", netConfig.getStaticNetworkId(), NetworkTime::now().get());
                            }
                            newScheduleAvailable = false;
                            status = StreamWaitSchedulerStatus::AWAITING_ACTIVATION;
                        }
                        break;
                    default:
                        Thread::nanoSleep(ctx.getDataSlotDuration());
                        break;
                }

                break;
            }
            
            default:
                ctx.sleepUntil(nextTileStartTime);
                break;
        }

        // if all the elements from curr and next list have been used,
        // use the next superframe start time as a "base time"
        // for computing wakeup times
        if (StreamWaitList::allListsElementsUsed(currWakeupList, nextWakeupList)) {
            useNextSuperFrameStartTime = true;
        }
    }
}

void StreamWaitScheduler::updateLists() {
    std::unique_lock<std::mutex> lck(mutex);
    
    currWakeupList.set(newCurrWakeupList.get());
    nextWakeupList.set(newNextWakeupList.get());
}

StreamWakeupInfo StreamWaitScheduler::getNextWakeupInfo() {
    
    std::unique_lock<std::mutex> lck(mutex);

    StreamWakeupInfo swi;

    switch(status) {
        case StreamWaitSchedulerStatus::AWAITING_ACTIVATION:
            if (isLastTileBeforeScheduleActivation) {
                // use the curr list and the next list
                // relative to the newly received schedule
                swi = StreamWaitList::getMinWakeupInfo(currWakeupList, newNextWakeupList);
            }
            else {
                // use the curr list and the next list
                // relative to the currently active schedule
                swi = StreamWaitList::getMinWakeupInfo(currWakeupList, nextWakeupList);
            }
            break;
        case StreamWaitSchedulerStatus::ACTIVE:
            // compare both the curr and next lists and 
            // take the element with minimum wakeup time
            swi = StreamWaitList::getMinWakeupInfo(currWakeupList, nextWakeupList);
            break;
        default:
            break;
    }

    return swi;
}

unsigned long long StreamWaitScheduler::nextAbsoluteWakeupTime(unsigned long long currSuperframeStartTime, 
                                                                unsigned long long nextSuperframeStartTime, 
                                                                unsigned int wt) {
    unsigned long long when = 0;

    if (useNextSuperFrameStartTime) {
        when = nextSuperframeStartTime + wt;
        useNextSuperFrameStartTime = false;
    }
    else {
        when = currSuperframeStartTime + wt;
    }

    return when;
}

void StreamWaitScheduler::wakeupStream(StreamWakeupInfo sinfo) {
    if (sinfo.id.getKey() != 0) {
        bool res = streamMgr.wakeup(sinfo.id);
        if (ENABLE_STREAM_WAKEUP_SCHEDULER_INFO_DBG) {
            if (!res) {
                print_dbg("[S] Invalid stream ID to be woken up (ID = %u)\n", sinfo.id.getKey());
            }
        }
    }
}

#ifndef _MIOSIX
void StreamWaitScheduler::printStatus(unsigned int tile) {
    switch(status) {
        case StreamWaitSchedulerStatus::IDLE:
            print_dbg("[S] Node : %d - State = IDLE - Tile = %u\n", netConfig.getStaticNetworkId(), tile); 
            break;
        case StreamWaitSchedulerStatus::AWAITING_ACTIVATION:
            print_dbg("[S] Node : %d - State = AWAITING_ACTIVATION - Tile = %u\n", netConfig.getStaticNetworkId(), tile); 
            break;
        case StreamWaitSchedulerStatus::ACTIVE:
            print_dbg("[S] Node : %d - State = ACTIVE - Tile = %u\n", netConfig.getStaticNetworkId(), tile); 
            break;
        default:
            break;
    }
}

void StreamWaitScheduler::printLists() {
    print_dbg("[S] Node : %d - Wakeup lists \n", netConfig.getStaticNetworkId());
    print_dbg("---------------------------------------\n");
    currWakeupList.print();
    print_dbg("---------------------------------------\n");
    nextWakeupList.print();
}
#endif
} /* namespace mxnet */
