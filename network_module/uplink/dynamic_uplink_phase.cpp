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
#include <limits>

using namespace miosix;

namespace mxnet {

void DynamicTopologyDiscoveryPhase::receiveByNode(long long slotStart) {
    auto wakeUpTimeout = syncStatus.getWakeupAndTimeout(slotStart);
    RecvResult result;
    auto now = getTime();
    if (now >= timestampFrom - status.receiverWindow)
        print_dbg("[U] start late\n");
    if (now < wakeUpTimeout.first)
        pm.deepSleepUntil(wakeUpTimeout.first);
    while (result.error != miosix::RecvResult::TIMEOUT && result.error != miosix::RecvResult::OK) {
        try {
            result = transceiver.recv(packet, ctx.maxPacketSize, wakeUpTimeout.second);
        } catch(std::exception& e) {
            if (ENABLE_RADIO_EXCEPTION_DBG)
                print_dbg("%s\n", e.what());
        }
        if (ENABLE_PKT_INFO_DBG) {
            if(result.size) {
                print_dbg("Received packet, error %d, size %d, timestampValid %d: ", result.error, result.size, result.timestampValid);
                if (ENABLE_PKT_DUMP_DBG)
                    memDump(packet, result.size);
            } else print_dbg("No packet received, timeout reached\n");
        }
    }
    if (result.error == RecvResult::ErrorCode::OK) {
        auto msg = UplinkMessage::fromPkt(std::vector<unsigned char>(packet, packet + result.size), config);
        topology.receivedMessage(msg, currentNode, result.rssi);
        streamManagement.receive(msg.getSMES());
        if (ENABLE_UPLINK_INFO_DBG)
            print_dbg("[U] <- N=%u @%llu\n", currentNode, result.timestamp);
    } else {
        topology.unreceivedMessage(currentNode);
    }
}

void DynamicTopologyDiscoveryPhase::sendMyTopology(long long slotStart) {
    auto dTopology = dynamic_cast<DynamicTopologyContext&>(topology);
    auto dSMContext = dynamic_cast<DynamicStreamManagementContext&>(streamManagement);
    if (!dTopology.hasPredecessor()) return;
    auto* tMsg = dTopology.getMyTopologyMessage();
    unsigned char maxSMEs = (MediumAccessController::maxPktSize - UplinkMessage::getSizeWithoutSMEs(tMsg)) / StreamManagementElement::getSize();
    auto smes = dSMContext.dequeue(maxSMEs);
    UplinkMessage msg(ctx.getHop(), dTopology.getBestPredecessor(), tMsg, smes);
    std::vector<unsigned char> pkt = msg.getPkt();
    if (ENABLE_UPLINK_INFO_DBG)
        print_dbg("[U] N=%u -> @%llu\n", ctx.getNetworkId(), slotStart);
    auto wuTime = slotStart - MediumAccessController::receivingNodeWakeupAdvance;
    auto now = getTime();
    if (now >= slotStart)
        print_dbg("[U] start late\n");
    if (now < wuTime)
        pm.deepSleepUntil(wuTime);
    transceiver.sendAt(pkt.data(), pkt.size(), slotStart);
    tMsg->deleteForwarded();
    delete tMsg;
    smes.clear();
}

void DynamicTopologyDiscoveryPhase::execute(long long slotStart) {
    print_dbg("[T] T=%lld\n", slotStart);
    transceiver.configure(ctx.getTransceiverConfig());
    ledOn();
    transceiver.turnOn();
    if (currentNode == ctx.getNetworkId()) sendMyTopology(slotStart);
    else receiveByNode(slotStart);
    transceiver.turnOff();
    ledOff();
}

} /* namespace mxnet */
