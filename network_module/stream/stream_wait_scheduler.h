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
 * The execution runs in a separate thread.
 */
class StreamWaitScheduler {

public:
    StreamWaitScheduler(const MACContext& ctx_, const NetworkConfiguration& netConfig_, StreamManager& streamMgr_): 
                            ctx(ctx_), netConfig(netConfig_), streamMgr(streamMgr_) {}

    void setStreamsWakeupLists(const std::vector<StreamWakeupInfo>& currList, 
                                const std::vector<StreamWakeupInfo>& nextList);

    void setScheduleActivationTile(const unsigned int activationTile);

    void start();

    void run(); // needed public for usage with the simulator (executed by OmnetThread class)

private:
    void updateLists();

    StreamWakeupInfo getNextWakeupInfo();

    //StreamWakeupInfo getCurrListElement();

    void wakeupStream(StreamWakeupInfo sinfo);

    bool allListsElementsUsed();

#ifndef _MIOSIX
    void printStatus(unsigned int tile);
    void printLists();
#else
    void printStatus(unsigned int tile) {}
    void printLists() {}
#endif

    enum class StreamWaitSchedulerStatus : unsigned char {
        IDLE,
        AWAITING_ACTIVATION,
        ACTIVE
    };

    StreamWaitSchedulerStatus status = StreamWaitSchedulerStatus::IDLE;

    unsigned int scheduleActivationTile = 0;
    //unsigned int scheduleActivationTime = 0;

    bool isLastTileBeforeScheduleActivation = false;
    bool isLastTileInSuperframe             = false;

    bool useNextSuperFrameStartTime = false;

    //std::vector<ExplicitScheduleElement> explicitSchedule;
    //std::map<unsigned int, StreamWaitInfo> streamsTable;

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