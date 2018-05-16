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

#include "../debug_settings.h"
#include "dynamic_uplink_phase.h"
#include "../timesync/timesync_downlink.h"
#include "topology/topology_context.h"
#include <limits>

using namespace miosix;

namespace mxnet {

void DynamicUplinkPhase::receiveByNode(long long slotStart, unsigned char currentNode) {
    auto wakeUpTimeout = timesync->getWakeupAndTimeout(slotStart);
    auto now = getTime();
    if (now + timesync->getReceiverWindow() >= slotStart)
        print_dbg("[U] start late\n");
    if (now < wakeUpTimeout.first)
        ctx.sleepUntil(wakeUpTimeout.first);
    do {
        rcvResult = ctx.recv(packet.data(), packet.size(), wakeUpTimeout.second);
        if (ENABLE_PKT_INFO_DBG) {
            if(rcvResult.size) {
                print_dbg("Received packet, error %d, size %d, timestampValid %d: ", rcvResult.error, rcvResult.size, rcvResult.timestampValid);
                if (ENABLE_PKT_DUMP_DBG)
                    memDump(packet.data(), rcvResult.size);
            } else print_dbg("No packet received, timeout reached\n");
        }
    } while (rcvResult.error != miosix::RecvResult::TIMEOUT && rcvResult.error != miosix::RecvResult::OK);
    if (rcvResult.error == RecvResult::ErrorCode::OK) {
        auto msg = UplinkMessage::deserialize(packet.data(), rcvResult.size, ctx.getNetworkConfig());
        topology->receivedMessage(msg, currentNode, rcvResult.rssi);
        auto smes = msg.getSMEs();
        streamManagement->receive(smes);
        if (ENABLE_UPLINK_INFO_DBG)
            print_dbg("[U] <- N=%u @%llu\n", currentNode, rcvResult.timestamp);
    } else {
        topology->unreceivedMessage(currentNode);
    }
}

void DynamicUplinkPhase::sendMyUplink(long long slotStart) {
    auto* dTopology = static_cast<DynamicTopologyContext*>(topology);
    auto* dSMContext = static_cast<DynamicStreamManagementContext*>(streamManagement);
    if (!dTopology->hasPredecessor()) return;
    auto* tMsg = dTopology->getMyTopologyMessage();
    unsigned char maxSMEs = (MediumAccessController::maxPktSize - UplinkMessage::getSizeWithoutSMEs(tMsg)) / StreamManagementElement::maxSize();
    auto smes = dSMContext->dequeue(maxSMEs);
    UplinkMessage msg(ctx.getHop(), dTopology->getBestPredecessor(), tMsg, smes);
    msg.serialize(packet.begin());
    if (ENABLE_UPLINK_INFO_DBG)
        print_dbg("[U] N=%u -> @%llu\n", ctx.getNetworkId(), slotStart);
    auto wuTime = timesync->getSenderWakeup(slotStart);
    auto now = getTime();
    if (now >= slotStart)
        print_dbg("[U] start late\n");
    if (now < wuTime)
        ctx.sleepUntil(wuTime);
    ctx.sendAt(packet.data(), msg.size(), slotStart);
    tMsg->deleteForwarded();
    delete tMsg;
    for (auto it = smes.begin() ; it != smes.end(); ++it) {
        delete (*it);
    }
    smes.clear();
}

void DynamicUplinkPhase::execute(long long slotStart) {
    auto address = currentNode();
    if (ENABLE_UPLINK_VERB_DBG)
        print_dbg("[U] N=%u T=%lld\n", address, slotStart);
    ctx.configureTransceiver(ctx.getTransceiverConfig());
    if (address == ctx.getNetworkId()) sendMyUplink(slotStart);
    else receiveByNode(slotStart, address);
    ctx.transceiverIdle();
}

} /* namespace mxnet */
