/***************************************************************************
 *   Copyright (C)  2017 by Federico Amedeo Izzo                           *
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

#include "dynamic_schedule_distribution.h"
#include "../tdmh.h"
#include "../packet.h"
#include "../debug_settings.h"

using namespace miosix;

namespace mxnet {

void DynamicScheduleDownlinkPhase::execute(long long slotStart) {
    Packet pkt;
    // Receive the schedule packet
    //TODO: check that the calculated arrivaltime is correct
    auto arrivalTime = slotStart + (ctx.getHop() - 1) * rebroadcastInterval;
    ctx.configureTransceiver(ctx.getTransceiverConfig());
    auto rcvResult = pkt.recv(ctx, arrivalTime);
    ctx.transceiverIdle(); //Save power waiting for rebroadcast time
    
    if (rcvResult.error == miosix::RecvResult::TIMEOUT) return;

    // Rebroadcast the schedule packet
    if(ctx.getHop() >= ctx.getNetworkConfig().getMaxHops()) return;
    ctx.configureTransceiver(ctx.getTransceiverConfig());
    pkt.send(ctx, rcvResult.timestamp + rebroadcastInterval);
    ctx.transceiverIdle();

    // Parse the schedule packet
    schedule = SchedulePacket::deserialize(pkt);

    print_dbg("[D] node %d, hop %d, received schedule %u/%u/%lu/%d/%d\n",
              ctx.getNetworkId(),
              ctx.getHop(),
              schedule.getHeader().getTotalPacket(),
              schedule.getHeader().getPacketCounter(),
              schedule.getHeader().getScheduleID(),
              schedule.getHeader().getRepetition(),
              schedule.getHeader().getCountdown());

}

}

