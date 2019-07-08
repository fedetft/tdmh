/***************************************************************************
 *   Copyright (C)  2018-2019 by Polidori Paolo, TerraneoFederico,         *
 *   Federico Amedeo Izzo                                                  *
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
                                     int availableTopologies, int availableSMEs) :
    bitsetSize(config.getNeighborBitmaskSize()),
    topologySize(TopologyElement::maxSize(bitsetSize)),
    smeSize(StreamManagementElement::maxSize()),
    panId(config.getPanId())
{
    computePacketAllocation(config, availableTopologies, availableSMEs);
    packet.putPanHeader(panId);
    UplinkHeader header({hop, assignee, numTopologies, numSMEs});
    packet.put(&header, sizeof(UplinkHeader));
    auto neighbors = myTopology.getNeighbors();
    packet.put(neighbors.data(), neighbors.size());
}

void SendUplinkMessage::serializeTopologiesAndSMEs(UpdatableQueue<unsigned char,TopologyElement>& topologies,
                                                  UpdatableQueue<StreamId,StreamManagementElement>& smes) {
    int remainingBytes = packet.available();
    // Fit topologies in packets
    int packetTopologies = std::min<int>(numTopologies, remainingBytes / topologySize);
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
    int packetSMEs = std::min<int>(numSMEs, remainingBytes / smeSize);
    for(int i = 0; i < packetSMEs; i++) {
        auto sme = smes.dequeue();
        sme.serialize(packet);
    }
    numSMEs -= packetSMEs;
}

void SendUplinkMessage::computePacketAllocation(const NetworkConfiguration& config,
                                                int availableTopologies, int availableSMEs) {
    const int maxPackets = config.getNumUplinkPackets();
    const int guaranteedTopologies = config.getGuaranteedTopologies();
    /* Calculate numTopologies and numSME without considering a topology or SME
       split between two packets
       Algorithm:
          - first fit guaranteed topologies
          - in the remaining bytes try to fit as many SME as we have available
          - if there is space remaining, try to fit other topologies
    */
    {
        const int totAvailableBytes = getFirstUplinkPacketCapacity(config) +
            (maxPackets - 1) * getOtherUplinkPacketCapacity();
        numTopologies = std::min(guaranteedTopologies, availableTopologies);
        const int remainingTopologies = availableTopologies - numTopologies;
        const int topologyBytes = numTopologies * topologySize;
        const int maxSMEs = (totAvailableBytes - topologyBytes) / smeSize;
        numSMEs = std::min(availableSMEs, maxSMEs);
        const int unusedBytes = totAvailableBytes - topologyBytes -
            (numSMEs * smeSize);
        numTopologies += std::min<int>(unusedBytes / topologySize, remainingTopologies);
    }

    /* Try to fit numTopologies and numSME in packet, to get the actual numbers,
       also considering SMEs and Topologies split between two packets */
    int remainingBytes = getFirstUplinkPacketCapacity(config);
    totPackets = 1;
    // Fit topologies in packets
    int remainingTopologies = numTopologies;
    for(;;) {
        // NOTE: Do the allocation also for the last packet
        int packetTopologies = std::min<int>(remainingTopologies, remainingBytes / topologySize);
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
        int packetSMEs = std::min<int>(remainingSMEs, remainingBytes / smeSize);
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

//
// class ReceiveUplinkMessage
//

bool ReceiveUplinkMessage::recv(MACContext& ctx, long long tExpected) {
    auto rcvResult = packet.recv(ctx, tExpected);
    if(rcvResult.error != miosix::RecvResult::ErrorCode::OK)
        return false;

    auto& config = ctx.getNetworkConfig();
    // Validate first packet
    if(receivedPackets == 0) {
        if(checkFirstPacket(config) == false) return false;
    }
    // Validate other packets
    else {
        if(checkOtherPacket(config) == false) return false;
    }
    // Save rssi and timestamp of valid packet
    rssi = rcvResult.rssi;
    if(rcvResult.timestampValid)
        timestamp = rcvResult.timestamp;
    else
        timestamp = -1;
    receivedPackets++;
    return true;
}

void ReceiveUplinkMessage::deserializeTopologiesAndSMEs(UpdatableQueue<unsigned char,
                                                        TopologyElement>& topologies,
                                                        UpdatableQueue<StreamId,
                                                        StreamManagementElement>& smes) {
    for(int i = 0; i < getNumPacketTopologies(); i++) {
        auto topology = getForwardedTopology();
        unsigned char id = topology.getId();
        topologies.enqueue(id, std::move(topology));
    }

    for(int i = 0; i < getNumPacketSMEs(); i++) {
        if(packet.size() < 9)
            return;
        auto sme = getSME();
        smes.enqueue(sme.getStreamId(),sme);
    }
}

bool ReceiveUplinkMessage::checkFirstPacket(const NetworkConfiguration& config) {
    const unsigned int headerSize = Packet::maxSize() - getFirstUplinkPacketCapacity(config);
    if(packet.size() < headerSize) return false;
    if(packet.checkPanHeader(panId) == false) return false;
    packet.removePanHeader();
    UplinkHeader tempHeader;
    packet.get(&tempHeader, sizeof(UplinkHeader));
    if(tempHeader.hop == 0 || tempHeader.hop > config.getMaxHops()) return false;
    if(tempHeader.assignee > config.getMaxNodes()) return false;
    // Extract sender topology
    RuntimeBitset tempSenderTopology(maxNodes);
    packet.get(tempSenderTopology.data(), bitsetSize);

    // Check topologies and SME only if uplink packet has any of them
    if(tempHeader.numTopology != 0 || tempHeader.numSME != 0)
        if(checkTopologiesAndSMEs(config, headerSize, tempHeader) == false)
            return false;

    // Write temporary values to class fields
    header = tempHeader;
    topology = std::move(tempSenderTopology);
    return true;
}

bool ReceiveUplinkMessage::checkOtherPacket(const NetworkConfiguration& config) {
    const unsigned int headerSize = Packet::maxSize() - getOtherUplinkPacketCapacity();
    if(packet.size() < headerSize) return false;
    if(packet.checkPanHeader(panId) == false) return false;
    packet.removePanHeader();
    if(checkTopologiesAndSMEs(config, headerSize, header) == false) return false;
    return true;
}

bool ReceiveUplinkMessage::checkTopologiesAndSMEs(const NetworkConfiguration& config,
                                                  int headerSize, UplinkHeader tempHeader) {
    /* Validate numTopologies and numSME in UplinkHeader
       by trying to extract from an example packet the same number of topologies and SME */
    const int maxPackets = config.getNumUplinkPackets();
    int remainingBytes = packet.size();
    int numPackets = 1;
    // Validate topologies in packets
    int remainingTopologies = tempHeader.numTopology;
    unsigned int topologiesInPacket = 0;
    // If no topologies in uplinkpacket, skip check
    if(remainingTopologies != 0) {
        for(;;) {
            // NOTE: Do the allocation also for the last packet
            int topologies = std::min<int>(remainingTopologies, remainingBytes / topologySize);
            remainingTopologies -= topologies;
            remainingBytes -= topologies * topologySize;
            // Validate last received packet
            if(numPackets == receivedPackets + 1) {
                topologiesInPacket = topologies;
                // Check that TopologyElements in packet are valid
                for(unsigned int i = 0; i < topologiesInPacket; i++) {
                    // Calculate TopologyElement offset in packet
                    unsigned int offset = topologySize * i;
                    // Check that there is enough data in the packet
                    if(offset + topologySize > packet.size()) return false;
                    if(TopologyElement::validateInPacket(packet, offset + headerSize, maxNodes) == false)
                        return false;
                }
            }
            if(remainingTopologies == 0) break;
            if(++numPackets > maxPackets) return false;
            remainingBytes = getOtherUplinkPacketCapacity();
        } 
    }

    // Validate SMEs in packets
    int remainingSMEs = tempHeader.numSME;
    int SMEsInPacket = 0;
    // If no SMEs in uplinkpacket, skip check
    if(remainingSMEs != 0) {
        for(;;) {
            // NOTE: Do the allocation also for the last packet
            int smes = std::min<int>(remainingSMEs, remainingBytes / smeSize);
            remainingSMEs -= smes;
            remainingBytes -= smes * smeSize;
            // Validate last received packet
            if(numPackets == receivedPackets + 1) {
                SMEsInPacket = smes;
                // Check that SMEs in packet are valid
                for(int i = 0; i < SMEsInPacket; i++) {
                    // Calculate SME offset in packet
                    unsigned int offset = (topologySize * topologiesInPacket) + (smeSize * i);
                    // Check that there is enough data in the packet
                    if(offset + smeSize > packet.size()) return false;
                    if(StreamManagementElement::validateInPacket(packet, offset + headerSize) == false)
                        return false;
                }
            }
            if(remainingSMEs == 0) break;
            if(++numPackets > maxPackets) return false;
            remainingBytes = getOtherUplinkPacketCapacity();
        }
    }
    // Check size of data in packet
    if((topologiesInPacket * topologySize + SMEsInPacket * smeSize) != packet.size())
        return false;

    // Write temporary values to class fields
    // Write totPackets only after checking first packet
    if(totPackets == 0)
        totPackets = numPackets;
    else {
        // numPackets depends only on header (first packet), so should remain constant
        assert(totPackets == numPackets);
    }
    packetTopologies = topologiesInPacket;
    packetSMEs = SMEsInPacket;
    return true;
}

} /* namespace mxnet */
