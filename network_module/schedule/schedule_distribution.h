/***************************************************************************
 *   Copyright (C)  2017 by Terraneo Federico, Polidori Paolo,             *
 *                          Federico Amedeo Izzo                           *
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

#include "../mac_phase.h"
#include "../mac_context.h"

/**
 * Represents the phase in which the schedule is distributed to the entire network,
 * in order to reach all the nodes so they can playback it when it becomes active.
 */

namespace mxnet {

    /* Possible actions to do in a dataphase slot */
    enum class Action
        {
         SLEEP      =0,    // Sleep to save energy 
         SENDSTREAM =1,    // Send packet of a stream opened from this node
         RECVSTREAM =2,    // Receive packet of a stream opened to this node
         SENDBUFFER =3,    // Send a saved packet from a multihop stream
         RECVBUFFER =4,    // Receive and save packet from a multihop stream
        };

    struct ExplicitScheduleElement {
        unsigned int action:3;
        unsigned int port:4;
    }

class ScheduleDownlinkPhase : public MACPhase {
public:
    ScheduleDownlinkPhase() = delete;
    ScheduleDownlinkPhase(const ScheduleDownlinkPhase& orig) = delete;
    ScheduleDownlinkPhase(MACContext& ctx) :
        MACPhase(ctx) {}
    virtual ~ScheduleDownlinkPhase() {};

    static unsigned long long getDuration(unsigned short hops) {
        return phaseStartupTime + hops * rebroadcastInterval;
    }

    void advance(long long slotStart) override {} //TODO

    static const int phaseStartupTime = 450000;
    static const int rebroadcastInterval = 5000000; //32us per-byte + 600us total delta
    unsigned long getScheduleID() {
        return header.getScheduleID();
    }
    std::vector<ScheduleElement> getSchedule() {
        return currentSchedule;
    }


private:
    // Schedule header with information on schedule distribution
    ScheduleHeader header;
    // Copy of last computed/received schedule
    std::vector<ScheduleElement> schedule;

    std::vector<ExplicitScheduleElement> explicitSchedule;
};

}

