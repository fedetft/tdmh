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

#pragma once

#include "../serializable_message.h"
#include "../packet.h"
#include "stream_management/stream_management_element.h"
#include "../updatable_queue.h"

namespace mxnet {

const int panHeaderSize = 5; //panHeader size is 5 bytes

/* Header used in the first packet of the UplinkMessage */
struct UplinkHeader {
    unsigned char hop;
    unsigned char assignee;
    unsigned char numTopology;
    unsigned char numSME;
} __attribute__((packed));

/**
 * @return the capacity of the first packet of an UplinkMessage, which is composed of
 * panHeader, UplinkHeader and myTopology
 */
inline int getFirstUplinkPacketCapacity(const NetworkConfiguration& config) {
    int runtimeBitsetSize = ((config.getMaxNumNodes() + 7) / 8);
    return Packet::maxSize() - (panHeaderSize +
                                sizeof(UplinkHeader) +
                                runtimeBitsetSize);
}

/**
 * @return the capacity of the second and following packets of an UplinkMessage,
 * which are composed of panHeader and SmallHeader
 */
inline int getOtherUplinkPacketCapacity() {
    return Packet::maxSize() - panHeaderSize;
}

/** The UplinkMessage class represents the message used to send topologies and SMEs
 *  to the other nodes of the network.
 *
 *  The UplinkMessages can be of two types in case TDMH is configured for multiple
 *  uplink slots, the first UplinkMessage sent by a node is complete, while the
 *  subsequent UplinkMessages lack the hop and assignee fields in the header,
 *  and lack the topology of the sender, containing only forwarded Topologies and SMEs.
 *  This differentiation is handled with a parameter specified to the deserialize
 *  function, this approach works because the TDMH network is deterministic and
 *  it is possible to know if we are deserializing the first packet or not.
 *
 * NOTE: UplinkMessage does not derive from SerializableMessage,
 * because unlike SerializableMessage, it contains directly a Packet class,
 * and serialization/deserialization are handled differently than SerializableMessage,
 * for example the `extract` method, analogous to `deserialize` extracts
 * only part of the data to avoid unnecessary copy of Topologies and SMEs
 */

class SendUplinkMessage {
public:
    SendUplinkMessage(const NetworkConfiguration& config,
                      unsigned char hop, unsigned char assignee,
                      const TopologyElement& myTopology,
                      int availableTopologies, int availableSMEs);

    SendUplinkMessage(const SendUplinkMessage&) = delete;
    SendUplinkMessage& operator=(const SendUplinkMessage&) = delete;

    /**
     * This function serializes topologies and SMEs in the packet.
     * NOTE: Call this function before every send
     */
    void serializeTopologiesAndSMEs(UpdatableQueue<unsigned char, TopologyElement>& topologies,
                                    UpdatableQueue<StreamId, StreamManagementElement>& smes);

    /**
     * @return the number of packet to send with this UplinkMessage
     */
    int getNumPackets() const { return totPackets; }

    /**
     * This function sends the current packet of the UplinkMessage over the radio
     * and prepares the next packet
     */
    void send(MACContext& ctx, long long sendTime) {
        packet.send(ctx, sendTime);
        // Prepare the next packet
        packet.clear();
        putPanHeader();
    }

private:

    void computePacketAllocation(const NetworkConfiguration& config,
                                 int availableTopologies, int availableSMEs);
    /**
     * Insert IEEE 802.15.4 header Packet
     */
    void putPanHeader();

    /* One of the UplinkMessage packets */
    Packet packet;
    /* Number of packets used */
    int totPackets;
    /* Number of topologies sent in UplinkMessage.
       Initialized by the constructor and decremented
       every time a topology is serialized */
    int numTopologies;
    /* Number of SMEs sent in UplinkMessage.
       Initialized by the constructor and decremented
       every time a new SME is serialized */
    int numSMEs;
};

class ReceiveUplinkMessage {
public:
    ReceiveUplinkMessage(const NetworkConfiguration& config);

    ReceiveUplinkMessage(const ReceiveUplinkMessage&) = delete;
    ReceiveUplinkMessage& operator=(const ReceiveUplinkMessage&) = delete;

    /**
     * Get TopologyElements and SMEs from the UplinkMessage packet
     */
    void deserializeTopologiesAndSMEs(UpdatableQueue<unsigned char, TopologyElement>& topologies,
                                      UpdatableQueue<StreamId, StreamManagementElement>& smes);

    /**
     * This function listens on the radio for the next Packet of the UplinkMessage
     * @return if a valid packet is received.
     */
    bool recv(MACContext& ctx, long long tExpected);

    /**
     * @return the number of packets
     */
    int getNumPackets() const { return otherPackets; }

    /**
     * @return the hop of the message sender
     */
    unsigned char getHop() const { return header.hop; }

    /**
     * @return the recipient node of the UplinkMessage
     */
    unsigned char getAssignee() const { return header.assignee; }

    /**
     * @return the recipient node of the UplinkMessage
     */
    unsigned char getNumTopologies() const { return header.assignee; }

    /**
     * @return the recipient node of the UplinkMessage
     */
    unsigned char getNumSMEs() const { return header.assignee; }

    /**
     * @return the TopologyElement containing the neighbors of the sender
     */
    RuntimeBitset getSenderTopology() const { return topology; }

    /**
     * @return the TopologyElement containing the neighbors of the sender
     */
    TopologyElement getForwardedTopology();

    /**
     * @return the next StreamManagementElement, extracting it from the Packet
     */
    StreamManagementElement getSME();

private:
    /**
     * @return the capacity of the first packet of an UplinkMessage, which is composed of
     * panHeader, UplinkHeader and myTopology
     */
    int getFirstPacketCapacity() {
        return Packet::maxSize() - (panHeaderSize +
                                    sizeof(UplinkHeader) +
                                    runtimeBitsetSize);
    }

    /**
     * @return the capacity of the second and following packets of an UplinkMessage,
     * which are composed of panHeader and SmallHeader
     */
    int getOtherPacketCapacity() {
        return Packet::maxSize() - (panHeaderSize +
                                    sizeof(SmallHeader));
    }

    /**
     * Checks the IEEE 802.15.4 header of the current packet.
     * @return true if current packet is an UplinkPacket, false otherwise
     */
    bool checkHeader();

    /**
     * Checks the UplinkHeader of the current packet.
     * @return true if data in UplinkHeader is valid, false otherwise
     * firstPacket is used to check againt the correct packet type
     */
    bool checkPacket();

    /**
     * Insert IEEE 802.15.4 header Packet
     */
    void putPanHeader();

    /**
     * Insert UplinkHeader in Packet
     */
    void putUplinkHeader();

    /**
     * Insert UplinkHeader in Packet
     */
    void putSmallHeader();

    /**
     * Insert myTopology in Packet
     */
    void putMyTopology();

    /**
     * Extract header and myTopology from the packet.
     */
    void extract();
    /* Configuration values from NetworkConfiguration */
    const int maxPackets;
    const int guaranteedTopologies;

    /* Bool flag to store if last received packet is valid */
    bool valid = false;
    /* Packet we are handling, first packet = 0 */
    int currPacket = 0;
    /* Number of packets used */
    int totPackets = 0;
    /* Number of packets, first excluded */
    int otherPackets = 0;
    /* Array of Packets composing the UplinkMessage */
    Packet *packet;
    /* Header containing information on this packet */
    UplinkHeader header;
    /* TopologyElement containing topology of node sending the packet */
    TopologyElement myTopology;
};

} /* namespace mxnet */
