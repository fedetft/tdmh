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

#include "master_uplink_phase.h"
#include "topology/mesh_topology_context.h"
#include "../debug_settings.h"
#include <limits>

using namespace miosix;

namespace mxnet {

void MasterUplinkPhase::execute(long long slotStart) {
    auto address = currentNode();
    if (ENABLE_UPLINK_VERB_DBG)
        print_dbg("[U] N=%u T=%lld\n", address, slotStart);
    ctx.configureTransceiver(ctx.getTransceiverConfig());
    auto wuTime = slotStart - MediumAccessController::receivingNodeWakeupAdvance;
    auto timeout = slotStart + MediumAccessController::maxPropagationDelay +
            MediumAccessController::maxAdmittableResyncReceivingWindow + MediumAccessController::packetPreambleTime;
    auto now = getTime();
    if (now >= slotStart)
        print_dbg("[U] start late\n");
    if (now < wuTime)
        ctx.sleepUntil(wuTime);
    do {
        rcvResult = ctx.recv(packet.data(), MediumAccessController::maxPktSize, timeout);
        if (ENABLE_PKT_INFO_DBG) {
            if(rcvResult.size) {
                print_dbg("Received packet, error %d, size %d, timestampValid %d: ",
                        rcvResult.error, rcvResult.size, rcvResult.timestampValid);
                if (ENABLE_PKT_DUMP_DBG)
                    memDump(packet.data(), rcvResult.size);
            } else print_dbg("No packet received, timeout reached\n");
        }
    } while (rcvResult.error != miosix::RecvResult::TIMEOUT && rcvResult.error != miosix::RecvResult::OK);
    ctx.transceiverIdle();
    if (rcvResult.error == RecvResult::ErrorCode::OK) {
        auto data = std::vector<unsigned char>(packet.begin(), packet.begin() + rcvResult.size);
        auto msg = UplinkMessage::deserialize(data, ctx.getNetworkConfig());
        topology->receivedMessage(msg, address, rcvResult.rssi);
        auto smes = msg.getSMEs();
        streamManagement->receive(smes);
        if (ENABLE_UPLINK_INFO_DBG)
            print_dbg("[U]<-N=%u @%llu %hddBm\n", address, rcvResult.timestamp, rcvResult.rssi);
        if(ENABLE_TOPOLOGY_SHORT_SUMMARY)
            print_dbg("<-%d %ddBm\n",address,rcvResult.rssi);
    } else {
        topology->unreceivedMessage(address);
        if(ENABLE_TOPOLOGY_SHORT_SUMMARY)
            print_dbg("  %d\n",address);
    }
    if (ENABLE_TOPOLOGY_INFO_DBG && ctx.getNetworkConfig().getTopologyMode() == NetworkConfiguration::NEIGHBOR_COLLECTION && address == 1) {
        static_cast<MasterMeshTopologyContext*>(topology)->print();
    }
}

} /* namespace mxnet */
