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

UplinkMessage::UplinkMessage(unsigned int numPackets, unsigned char hop,
                             unsigned char assignee, const TopologyElement& myTopology) :
    numPackets(numPackets), header({hop, assignee, 0, 0}), myTopology(myTopology) {
    packet = new Packet[numPackets];
    /* Add panHeader, UplinkHeader and myTopology to first packet */
    putHeader();
    putMyTopology();
}

UplinkMessage::UplinkMessage(unsigned int numPackets) :
    numPackets(numPackets), header({0,0,0,0}) {
    packet = new Packet[numPackets];
}

UplinkMessage::~UplinkMessage() {
    delete[] packet;
}

void UplinkMessage::putHeader() {
    if(packet.size() != 0) throw std::runtime_error("UplinkMessage: can't put header on non empty Packet");
    auto panId = networkConfig.getPanId();
    unsigned char panHeader[] = {
                                 0x46, //frame type 0b110 (reserved), intra pan
                                 0x08, //no source addressing, short destination addressing
                                 0xff, //seq no reused as glossy hop count, 0=root node, it has to contain the source hop
                                 static_cast<unsigned char>(panId>>8),
                                 static_cast<unsigned char>(panId & 0xff), //destination pan ID
    };
    // Put IEEE 802.15.4 compliant header
    packet.put(&panHeader, sizeof(panHeader));
    // Put UplinkHeader
    packet.put(&header, sizeof(UplinkHeader));
}

void UplinkMessage::putMyTopology() {
    if(packet.size() == 0) throw std::runtime_error("UplinkMessage: can't put mytopology Packet without header");
    // Put myTopology in Packet
    myTopology.serialize(packet);
}

void UplinkMessage::send(MACContext& ctx, long long sendTime) {
    packet[currentPacket].send(ctx, sendTime);
    if(currentPacket < numPackets) currentPacket++;
}

bool recv(MACContext& ctx, long long tExpected) {
    packet[currentPacket].recv(ctx, tExpected);
    valid = checkHeader() && checkPacket();
    return valid;
}

bool UplinkMessage::checkHeader() {
    auto panId = networkConfig.getPanId();
    // Check panHeader
    if((packet[0] == 0x46 &&
        packet[1] == 0x08 &&
        packet[2] == 0xff &&
        packet[3] == static_cast<unsigned char>(panId >> 8) &&
        packet[4] == static_cast<unsigned char>(panId & 0xff)) return false;
    return true;
}

bool UplinkMessage::checkPacket() {
    // check if UplinkHeader matches the current packet
}

void UplinkMessage::putTopologiesAndSMEs(UpdatableQueue<TopologyElement>& topologies,
                                         UpdatableQueue<StreamManagementElement>& smes) {
    if(!extracted) throw std::runtime_error("UplinkMessage: can't add Topologies and SMES without header data");
    packet.put(&content, sizeof(UplinkMessagePkt));
    topology->serialize(packet);
    for (StreamManagementElement sme : smes) {
        sme.serialize(packet);
    }
}

void UplinkMessage::extract() const {
    if(!isUplinkPacket()) throw std::runtime_error("UplinkMessage: not an uplink packet");
    if(packet.size() < getMinSize()) throw std::runtime_error("UplinkMessage: packet has not enough data to extract");

    // Remove panHeader from the packet
    packet.discard(panHeaderSize);
    // Extract UplinkHeader from the packet
    packet.get(&header, sizeof(UplinkHeader));
    // Extract myTopology from the packet
    packet.get(&myTopology, sizeof(TopologyElement));
}

TopologyElement UplinkMessage::getForwardedTopology() {
    if(header.numSME < 1) throw std::runtime_error("UplinkMessage: no more forwarded topologies to extract");
    if(packet.size() < TopologyElement.size()) throw std::runtime_error("UplinkMessage: no more forwarded topologies to extract");
    TopologyElement result;
    packet.get(&result, sizeof(TopologyElement));
}

StreamManagementElement UplinkMessage::getSME() {
    if(packet.size() < TopologyElement.size()) throw std::runtime_error("UplinkMessage: no more forwarded topologies to extract");
    TopologyElement result;
    packet.get(&result, sizeof(TopologyElement));
}

} /* namespace mxnet */
