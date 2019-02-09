/***************************************************************************
 *   Copyright (C)  2018 by Polidori Paolo                                 *
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


#include "master_mac_context.h"
#include "data/dataphase.h"

namespace mxnet {
MasterMACContext::MasterMACContext(const MediumAccessController& mac, miosix::Transceiver& transceiver, const NetworkConfiguration& config) :
    MACContext(mac, transceiver, config) {
    timesync = new MasterTimesyncDownlink(*this);
    if (config.getTopologyMode() == NetworkConfiguration::NEIGHBOR_COLLECTION) {
        auto* topology_ctx = new MasterMeshTopologyContext(*this);
        scheduleComputation = new ScheduleComputation(*this, *topology_ctx);
        topologyContext = topology_ctx;
        uplink = new MasterUplinkPhase(*this, topology_ctx, *scheduleComputation);
    } else {
        auto* topology_ctx = new MasterTreeTopologyContext(*this);
        // A scheduler for Tree topology is not yet implemented
        topologyContext = topology_ctx;
        uplink = new MasterUplinkPhase(*this, topology_ctx, *scheduleComputation);
    }
    scheduleDistribution = new MasterScheduleDownlinkPhase(*this, *scheduleComputation);
    stream = new MasterStreamManager(*this);
    data = new DataPhase(*this, *stream);
    /* Stream list hardcoding */
    /* parameters:
       StreamManagementElement(unsigned char src, unsigned char dst, unsigned char srcPort,
       unsigned char dstPort, Period period, unsigned char payloadSize,
       Redundancy redundancy=Redundancy::NONE)*/
    // Streams for Line4 test
    /*scheduleComputation->open(StreamManagementElement(0, 1, 0, 0, Period::P2, 0));
    scheduleComputation->open(StreamManagementElement(3, 2, 0, 0, Period::P5, 0));
    scheduleComputation->open(StreamManagementElement(0, 2, 0, 0, Period::P1, 0));*/
    // Streams to replicate RTSS experiment
    scheduleComputation->open(StreamManagementElement(3, 0, 0, 0, Period::P1, 0, Redundancy::NONE));
    scheduleComputation->open(StreamManagementElement(4, 0, 0, 0, Period::P2, 0, Redundancy::DOUBLE_SPATIAL));
    scheduleComputation->open(StreamManagementElement(6, 0, 0, 0, Period::P2, 0, Redundancy::NONE));
};

} /* namespace mxnet */
