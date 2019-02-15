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
#include "../timesync/networktime.h"
#include "topology/topology_context.h"
#include "../packet.h"
#include <limits>
#include <cassert>

using namespace miosix;

namespace mxnet {

void DynamicUplinkPhase::receiveByNode(long long slotStart, unsigned char currentNode) {
    Packet pkt;
    RecvResult rcvResult = pkt.recv(ctx, slotStart);
    if (rcvResult.error == RecvResult::ErrorCode::OK) {
        auto msg = UplinkMessage::deserialize(pkt, ctx.getNetworkConfig());
        topology->receivedMessage(msg, currentNode, rcvResult.rssi);
        if(msg.getAssignee() == ctx.getNetworkId())
        {
            auto smes = msg.getSMEs();
            streamManagement->receive(smes);
        }
        if (ENABLE_UPLINK_INFO_DBG)
            print_dbg("[U]<-N=%u @%llu %hddBm\n", currentNode, rcvResult.timestamp, rcvResult.rssi);
        if(ENABLE_TOPOLOGY_SHORT_SUMMARY)
            print_dbg("<-%d %ddBm\n",currentNode,rcvResult.rssi);
    } else {
        topology->unreceivedMessage(currentNode);
        if(ENABLE_TOPOLOGY_SHORT_SUMMARY)
            print_dbg("  %d\n",currentNode);
    }
}

void DynamicUplinkPhase::sendMyUplink(long long slotStart) {
    auto* dTopology = static_cast<DynamicTopologyContext*>(topology);
    auto& config = ctx.getNetworkConfig();
    // Calculate max number of SME that leaves spaces for the guaranteed topologies
    unsigned char SMEPart = MediumAccessController::maxPktSize -
        (UplinkMessage::getMinSize() + NeighborMessage::guaranteedSize(config));
    unsigned char maxSMEs = SMEPart / StreamManagementElement::maxSize();
    unsigned char availableSMEs = streamMgr->getNumSME();
    unsigned char numSMEs = std::min(maxSMEs, availableSMEs);
    unsigned char freeBytes = (maxSMEs - numSMEs) * StreamManagementElement::maxSize();
    unsigned char extraTopologies = freeBytes / ForwardedNeighborMessage::staticSize(config);
    // Prepare the UplinkMessage
    auto smes = streamMgr->getSMEs(numSMEs);
    auto* tMsg = dTopology->getMyTopologyMessage(extraTopologies);
    UplinkMessage msg(ctx.getHop(), dTopology->getBestPredecessor(), tMsg, smes);
    Packet pkt;
    msg.serialize(pkt);
    if (ENABLE_UPLINK_INFO_DBG)
        print_dbg("[U] N=%u -> @%llu\n", ctx.getNetworkId(), slotStart);
    pkt.send(ctx, slotStart);
    tMsg->deleteForwarded();
    delete tMsg;
    smes.clear();
    if(ENABLE_TOPOLOGY_SHORT_SUMMARY)
        print_dbg("->%d\n",ctx.getNetworkId());
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
