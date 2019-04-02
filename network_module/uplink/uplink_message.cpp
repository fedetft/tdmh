/***************************************************************************
 *   Copyright (C)  2019 by Polidori Paolo, Federico Amedeo Izzo,          *
 *                                          Federico Terraneo              *
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

#include "uplink_message.h"

namespace mxnet {

//
// class SendUplinkMessage
//

SendUplinkMessage::SendUplinkMessage(const NetworkConfiguration& config,
                                     unsigned char hop, unsigned char assignee,
                                     const TopologyElement& myTopology,
                                     int availableTopologies, int availableSMEs) {
    computePacketAllocation(config, availableTopologies, availableSMEs);
    putPanHeader(config.getPanId());
    UplinkHeader header({hop, assignee, numTopologies, numSMEs});
    packet.put(&header, sizeof(UplinkHeader));
    myTopology.serialize(packet);
}

int SendUplinkMessage::serializeTopologiesAndSMEs(UpdatableQueue<TopologyElement>& topologies,
                                                  UpdatableQueue<StreamManagementElement>& smes) {
    int topologySize = TopologyElement::maxSize();
    int smeSize = StreamManagementElement::maxSize();
    int remainingBytes = packet.available();
    // Fit topologies in packets
    int packetTopologies = std::min(numTopologies, remainingBytes / topologySize);
    for(int i = 0; i < packetTopologies; i++) {
        auto topology = topologies.dequeue();
        topology.serialize(packet);
    }
    numTopologies -= packetTopologies;
    // NOTE: Do not put SMEs as long as there are topologies left
    if(numTopologies > 0)
        return;

    remainingBytes -= packetTopologies * topologySize;
    // Fit SMEs in packets
    int packetSMEs = std::min(numSMEs, remainingBytes / smeSize);
    for(int i = 0; i < packetSMEs; i++) {
        auto sme = smes.dequeue();
        sme.serialize(packet);
    }
    numSMEs -= packetSMEs;
}

void SendUplinkMessage::computePacketAllocation(const NetworkConfiguration& config,
                                                int availableTopologies, int availableSMEs) {
    int maxPackets = config.getNumUplinkPackets();
    int guaranteedTopologies = config.getGuaranteedTopologies();
    int topologySize = TopologyElement::maxSize();
    int smeSize = StreamManagementElement::maxSize();
    /* Calculate numTopologies and numSME without considering a topology or SME
       split between two packets
       Algorithm:
          - first fit guaranteed topologies
          - in the remaining bytes try to fit as many SME as we have available
          - if there is space remaining, try to fit other topologies
    */
    {
        int totAvailableBytes = getFirstUplinkPacketCapacity(config) +
            (maxPackets - 1) * getOtherUplinkPacketCapacity();
        numTopologies = std::min(guaranteedTopologies, availableTopologies);
        int remainingTopologies = availableTopologies - numTopologies;
        int topologyBytes = numTopologies * topologySize;
        int maxSMEs = (totAvailableBytes - topologyBytes) / smeSize;
        numSMEs = std::min(availableSMEs, maxSMEs);
        int unusedBytes = totAvailableBytes - topologyBytes -
            (numSMEs * smeSize);
        numTopologies += min(unusedBytes / topologySize, remainingTopologies);
    }

    /* Try to fit numTopologies and numSME in packet, to get the actual numbers,
       also considering SMEs and Topologies split between two packets */
    int remainingBytes = getFirstUplinkPacketCapacity(config);
    totPackets = 1;
    // Fit topologies in packets
    int remainingTopologies = numTopologies;
    for(;;) {
        // NOTE: Do the allocation also for the last packet
        int packetTopologies = std::min(remainingTopologies, remainingBytes / topologySize);
        remainingTopologies -= packetTopologies;
        remainingBytes -= packetTopologies * topologySize;
        if(remainingTopologies == 0 || totPackets >= maxPackets) break;
        totPackets++;
        remainingBytes = getOtherUplinkPacketCapacity();
    }
    /* NOTE: Handling the corner case in which after splitting in packets, we can't
       fit all the topologies */
    numTopologies -= remainingTopologies;

    // Fit SMEs in packets
    int remainingSMEs = numSMEs;
    for(;;) {
        // NOTE: Do the allocation also for the last packet
        int packetSMEs = std::min(remainingSMEs, remainingBytes / smeSize);
        remainingSMEs -= packetSMEs;
        remainingBytes -= packetSMEs * smeSize;
        if(remainingSMEs == 0 || totPackets >= maxPackets) break;
        totPackets++;
        remainingBytes = getOtherUplinkPacketCapacity();
    }
    /* NOTE: Handling the corner case in which after splitting in packets, we can't
       fit all the SMEs */
    numSMEs -= remainingSMEs;

    // Sanity checks
    assert(numTopologies >= 0);
    assert(numSMEs >= 0);
    assert(totPackets <= maxPackets);
}

void SendUplinkMessage::putPanHeader(unsigned short panId) {
    unsigned char panHeader[] = {
        0x46, //frame type 0b110 (reserved), intra pan
        0x08, //no source addressing, short destination addressing
        0xff, //seq no is reused as packet type (0xff=uplink), or glossy hop count
        static_cast<unsigned char>(panId>>8),
        static_cast<unsigned char>(panId & 0xff), //destination pan ID
    };
    packet.put(&panHeader, sizeof(panHeader));
}

//
// class ReceiveUplinkMessage
//

bool ReceiveUplinkMessage::recv(MACContext& ctx, long long tExpected) {
    auto rcvResult = packet.recv(ctx, tExpected);
    if(rcvResult.error != RecvResult::ErrorCode::OK)
        return false;

    auto& config = ctx.getNetworkConfiguration();
    // Validate first packet
    if(firstPacket) {
        if(checkFirstPacket(config) == false) return false;
        firstPacket = false;
    }
    // Validate other packets
    else {
        if(checkOtherPacket() == false) return false;
    }
    // Save rssi and timestamp of valid packet
    rssi = rcvResult.getRssi();
    if(rcvResult.timestampValid())
        timestamp = rcvResult.getTimestamp();
    else
        timestamp = -1;
    return true;
}

void ReceiveUplinkMessage::deserializeTopologiesAndSMEs(UpdatableQueue<unsigned char,
                                                        TopologyElement>& topologies,
                                                        UpdatableQueue<StreamId,
                                                        StreamManagementElement>& smes) {
    for(int i = 0; i < getPacketTopologies(); i++) {
        auto topology = getForwardedTopology();
        topologies.enqueue(topology.getId(),std::move(topology));
    }

    for(int i = 0; i < getPacketSMEs(); i++) {
        auto sme = getSME();
        smes.enqueue(sme.getStreamId(),sme);
    }
}

bool ReceiveUplinkMessage::checkPanHeader(unsigned short panId) {
    // Check panHeader
    if(packet[0] == 0x46 &&
       packet[1] == 0x08 &&
       packet[2] == 0xff &&
       packet[3] == static_cast<unsigned char>(panId >> 8) &&
       packet[4] == static_cast<unsigned char>(panId & 0xff)) return false;

    // Remove panHeader from packet
    packet.discard(panHeaderSize);
    return true;
}

bool ReceiveUplinkMessage::checkFirstPacket(const NetworkConfiguration& config) {
    if(packet.size() < Packet::maxSize() - getFirstPacketCapacity(config)) return false;
    unsigned short panId = config.getPanId();
    if(checkPanHeader(panId) == false) return false;
    UplinkHeader temp;
    packet.get(&temp, sizeof(UplinkHeader));
    if(temp.hop == 0 || temp.hop > config.getMaxHops()) return false;
    if(temp.assignee > config.getMaxNodes()) return false;
    // Extract sender topology
    auto senderTopology = RuntimeBitset::deserialize(packet);

    /* Validate numTopologies and numSME in UplinkHeader
       by trying to extract from an example packet the same number of topologies and SME */
    int maxPackets = config.getNumUplinkPackets();
    int remainingBytes = packet.size();
    int numPackets = 1;
    // Validate topologies in packets
    int remainingTopologies = temp.numTopology;
    for(;;) {
        // NOTE: Do the allocation also for the last packet
        int packetTopologies = std::min(remainingTopologies, remainingBytes / topologySize);
        remainingTopologies -= packetTopologies;
        remainingBytes -= packetTopologies * topologySize;
        // Validate first packet
        if(numPackets == 1) {
            
        }
        if(remainingTopologies == 0) break;
        if(++numPackets > maxPackets) return false;
        remainingBytes = getOtherUplinkPacketCapacity();
    }

    // Validate SMEs in packets
    int remainingSMEs = temp.numSME;
    for(;;) {
        // NOTE: Do the allocation also for the last packet
        int packetSMEs = std::min(remainingSMEs, remainingBytes / smeSize);
        remainingSMEs -= packetSMEs;
        remainingBytes -= packetSMEs * smeSize;
        if(remainingSMEs == 0) break;
        if(++numPackets > maxPackets) return false;
        remainingBytes = getOtherUplinkPacketCapacity();
    }




    // Write temporary values to class fields
    totPackets = numPackets;
    header = temp;
    topology = std::move(senderTopology);
    return true;
}

} /* namespace mxnet */
