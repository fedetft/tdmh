/***************************************************************************
 *   Copyright (C) 2018-2019 by Federico Amedeo Izzo and Federico Terraneo *
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

#include "schedule_distribution.h"

namespace mxnet {

class DynamicScheduleDownlinkPhase : public ScheduleDownlinkPhase
{
public:
    DynamicScheduleDownlinkPhase(MACContext& ctx) : ScheduleDownlinkPhase(ctx),
                                                    myId(ctx.getNetworkId()) {}
    DynamicScheduleDownlinkPhase() = delete;
    DynamicScheduleDownlinkPhase(const DynamicScheduleDownlinkPhase& orig) = delete;
    
    void execute(long long slotStart) override;
    
    void advance(long long slotStart) override;

    void desync() override;

private:
    /**
     * @return true if actions of rekeying or activation were taken in this tile, meaning
     * that we must not listen for packets in this tile.
     */
    bool handleActivationAndRekeying(long long slotStart);
    
    bool recvPkt(long long slotStart, Packet& pkt);
    
    void applyInfoElements(SchedulePacket& spkt);
    
    void initSchedule(SchedulePacket& spkt);
    
    void appendToSchedule(SchedulePacket& spkt, bool beginResend = false);
    
    bool isScheduleComplete();

    void setEmptySchedule(long long slotStart);

    void applyEmptySchedule(long long slotStart);
    
    void resetAndDisableSchedule(long long slotStart);
    
    void handleIncompleteSchedule();
    
    std::string scheduleStatusAsString() const;
    
    void printHeader(ScheduleHeader& header) const;

    /* NetworkId of this node */
    unsigned char myId;

    // Number of times each schedule packet has been received
    std::vector<unsigned char> received;
    
    int incompleteScheduleCounter = 0;

    ScheduleHeader newHeader;

    // ScheduleID of last schedule I received complete
    unsigned int lastScheduleID = 0;
    // ScheduleID of the schedule being received
    unsigned int currentScheduleID = 0;

    unsigned int nextActivationTile;
};

}
