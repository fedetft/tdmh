/***************************************************************************
 *   Copyright (C) 2019 by Polidori Paolo, Terraneo Federico,              *
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

#pragma once

#include "../mac_context.h"
#include "../util/debug_settings.h"
#include "../util/serializable_message.h"
#include "../util/packet.h"
#include "../util/updatable_queue.h"
#include "../util/runtime_bitset.h"
#include "../stream/stream_management_element.h"
#include "topology/topology_element.h"

namespace mxnet {

/* Header used in the first packet of the UplinkMessage */
struct UplinkHeader {
    unsigned char hop;
    unsigned char assignee;
    unsigned char numTopology;
    unsigned char numSME;

    bool getHop() { return hop & 0x7F; };

    bool getBadAssignee() const {
        if(hop & 0x80) return true;
        else return false;
    }
} __attribute__((packed));

/**
 * @return the capacity of the first packet of an UplinkMessage, which is composed of
 * panHeader, UplinkHeader and myTopology
 */
inline int getFirstUplinkPacketCapacity(const NetworkConfiguration& config) {
    unsigned int capacity;
    if(config.getUseWeakTopologies()) {
        capacity = Packet::maxSize() - (panHeaderSize +
                                        sizeof(UplinkHeader) +
                                        2*config.getNeighborBitmaskSize());
    } else {
        capacity =  Packet::maxSize() - (panHeaderSize +
                                        sizeof(UplinkHeader) +
                                        config.getNeighborBitmaskSize());
    }
#ifdef CRYPTO
    if(config.getAuthenticateControlMessages()) capacity -= tagSize;
#endif
    
    return capacity;
}

/**
 * @return the capacity of the second and following packets of an UplinkMessage,
 * which are composed of panHeader and SmallHeader
 */
inline int getOtherUplinkPacketCapacity(const NetworkConfiguration& config) {
#ifdef CRYPTO
    if(config.getAuthenticateControlMessages()) {
        return Packet::maxSize() - panHeaderSize - tagSize;
    } else {
        return Packet::maxSize() - panHeaderSize;
    }
#else
    return Packet::maxSize() - panHeaderSize;
#endif
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
#ifdef CRYPTO
    SendUplinkMessage(const NetworkConfiguration& config, unsigned char hop,
                      bool badFlag, unsigned char assignee,
                      const TopologyElement& myTopology,
                      int availableTopologies, int availableSMEs,
                      AesGcm& gcm);
#else
    SendUplinkMessage(const NetworkConfiguration& config, unsigned char hop,
                      bool badFlag, unsigned char assignee,
                      const TopologyElement& myTopology,
                      int availableTopologies, int availableSMEs);
#endif

    SendUplinkMessage(const SendUplinkMessage&) = delete;
    SendUplinkMessage& operator=(const SendUplinkMessage&) = delete;

    /**
     * This function serializes topologies and SMEs in the packet.
     * NOTE: Call this function before every send
     */
    void serializeTopologiesAndSMEs(UpdatableQueue<unsigned char,TopologyElement>& topologies,
                                    UpdatableQueue<SMEKey,StreamManagementElement>& smes);

    /**
     * @return the number of packet to send with this UplinkMessage
     */
    int getNumPackets() const { return totPackets; }

    /**
     * This function sends the current packet of the UplinkMessage over the radio
     * and prepares the next packet
     */
    void send(MACContext& ctx, long long sendTime) {
#ifdef CRYPTO
        /**
         * NOTE: it is important that setIV is called before calling send
         */
        if(encrypt) packet.encryptAndPutTag(gcm);
        else if(authenticate) packet.putTag(gcm);
#endif
        packet.send(ctx, sendTime);
        // Prepare the next packet
        packet.clear();
#ifdef CRYPTO
        if(authenticate) packet.reserveTag();
#endif
        packet.putPanHeader(panId);
    }

    void printHeader();

    /**
     * @return the hop of the message sender
     * We use the most significant bit of the hop field as badAssignee flag
     */
    unsigned char getHop() const { return header.hop & 0x7F; }

    /**
     * @return the badAssignee flag
     * We use the most significant bit of the hop field as badAssignee flag
     */
    bool getBadAssignee() const {
        if(header.hop & 0x80) return true;
        else return false;
    }

#ifdef CRYPTO
    /**
     * Wrapper to set IV on the uplink gcm. Needed because decryption needs to be
     * performed before semantic checks on uplink message.
     */
    void setIV(unsigned int tileNo, unsigned long long seqNo, unsigned int masterIndex) {
        if(ENABLE_CRYPTO_UPLINK_DBG)
            print_dbg("[U] Authenticating uplink: tile=%d, seqNo=%d, mI=%d\n",
                      tileNo, seqNo, masterIndex);
        gcm.setIV(tileNo, seqNo, masterIndex);
    }
#endif

private:

    void computePacketAllocation(const NetworkConfiguration& config,
                                 int availableTopologies, int availableSMEs);

    /* Constant values used in the methods */
    bool weakTop;
    const unsigned int topologySize;
    const unsigned int smeSize;
    const unsigned short panId;
    UplinkHeader header;


    /* One of the UplinkMessage packets */
    Packet packet;
    /* Number of packets used */
    int totPackets;
    /* Number of topologies sent in UplinkMessage.
       Initialized by the constructor and decremented
       every time a topology is serialized */
    unsigned char numTopologies;
    /* Number of SMEs sent in UplinkMessage.
       Initialized by the constructor and decremented
       every time a new SME is serialized */
    unsigned char numSMEs;

#ifdef CRYPTO
    AesGcm& gcm;
    bool authenticate;
    bool encrypt;
#endif
};

class ReceiveUplinkMessage {
public:
#ifdef CRYPTO
    ReceiveUplinkMessage(const NetworkConfiguration& config, AesGcm& gcm) :
        bitsetSize(config.getNeighborBitmaskSize()),
        maxNodes(config.getMaxNodes()),
        weakTop(config.getUseWeakTopologies()),
        topologySize(TopologyElement::maxSize(bitsetSize, weakTop)),
        smeSize(StreamManagementElement::maxSize()),
        panId(config.getPanId()),
        topology(RuntimeBitset(maxNodes)),
        weakTopology(RuntimeBitset(maxNodes)),
        gcm(gcm),
        authenticate(config.getAuthenticateControlMessages()),
        encrypt(config.getEncryptControlMessages()) {}
#else
    ReceiveUplinkMessage(const NetworkConfiguration& config) :
        bitsetSize(config.getNeighborBitmaskSize()),
        maxNodes(config.getMaxNodes()),
        weakTop(config.getUseWeakTopologies()),
        topologySize(TopologyElement::maxSize(bitsetSize, weakTop)),
        smeSize(StreamManagementElement::maxSize()),
        panId(config.getPanId()),
        topology(RuntimeBitset(maxNodes)),
        weakTopology(RuntimeBitset(maxNodes)) {}
#endif
    ReceiveUplinkMessage(const ReceiveUplinkMessage&) = delete;
    ReceiveUplinkMessage& operator=(const ReceiveUplinkMessage&) = delete;

    /**
     * Get TopologyElements and SMEs from the UplinkMessage packet
     */
    void deserializeTopologiesAndSMEs(UpdatableQueue<unsigned char, TopologyElement>& topologies,
                                      UpdatableQueue<SMEKey, StreamManagementElement>& smes);

    /**
     * This function listens on the radio for the next Packet of the UplinkMessage
     * @return true if a valid packet is received.
     */
    bool recv(MACContext& ctx, long long tExpected);

    /**
     * @return the number of packets
     */
    int getNumPackets() const { return totPackets; }

    /**
     * @return the TopologyElement containing the neighbors of the sender
     */
    int getRssi() const { return rssi; }

    /**
     * @return the TopologyElement containing the neighbors of the sender
     */
    long long getTimestamp() const { return timestamp; }

    /**
     * @return the hop of the message sender
     * We use the most significant bit of the hop field as badAssignee flag
     */
    unsigned char getHop() const { return header.hop & 0x7F; }

    /**
     * @return the badAssignee flag
     * We use the most significant bit of the hop field as badAssignee flag
     */
    bool getBadAssignee() const {
        if(header.hop & 0x80) return true;
        else return false;
    }

    /**
     * @return the recipient node of the UplinkMessage
     */
    unsigned char getAssignee() const { return header.assignee; }

    /**
     * @return the number of Topologies saved in current packet
     */
    int getNumPacketTopologies() const { return packetTopologies; }

    /**
     * @return the number of SME saved in current packet
     */
    int getNumPacketSMEs() const { return packetSMEs; }

    /**
     * @return the TopologyElement containing the neighbors of the sender
     */
    TopologyElement getSenderTopology(unsigned char id) const {
        if(weakTop) return TopologyElement(id, topology, weakTopology);
        else return TopologyElement(id, topology);
    }

#ifdef CRYPTO
    /**
     * Wrapper to set IV on the uplink gcm. Needed because decryption needs to be
     * performed before semantic checks on uplink message.
     */
    void setIV(unsigned int tileNo, unsigned long long seqNo, unsigned int masterIndex) {
        if(ENABLE_CRYPTO_UPLINK_DBG)
            print_dbg("[U] Verifying uplink: tile=%d, seqNo=%d, mI=%d\n",
                      tileNo, seqNo, masterIndex);
        gcm.setIV(tileNo, seqNo, masterIndex);
    }
#endif

private:

    /**
     * Checks that the values in the first packet header are valid.
     * @return true if UplinkHeader of the received packet is valid, false otherwise
     */
    bool checkFirstPacket(const NetworkConfiguration& config);

    /**
     * Checks that the values in the second or following packet are valid.
     * @return true if the content of the received packet is valid, false otherwise
     */
    bool checkOtherPacket(const NetworkConfiguration& config);

    /**
     * Checks the Topologies and SMEs contained into the last received packet.
     * @return true if the content of the received packet is valid, false otherwise
     */
    bool checkTopologiesAndSMEs(const NetworkConfiguration& config,
                                UplinkHeader tempHeader);

    /* Constant values used in the methods */
    const unsigned short bitsetSize;
    const unsigned short maxNodes;
    bool weakTop;
    const unsigned int topologySize;
    const unsigned int smeSize;
    const unsigned short panId;

    /* One of the UplinkMessage packets */
    Packet packet;
    /* Number of packets composing the UplinkMessage */
    int totPackets = 0;
    /* Number of packets received */
    int receivedPackets = 0;
    /* RSSI of the received packer */
    int rssi = -120;
    /* Timestamp of the received packet */
    long long timestamp = -1;
    /* Header containing information on this packet */
    UplinkHeader header;
    /* TopologyElement containing topology of node sending the packet */
    RuntimeBitset topology;
    /* TopologyElement containing weak topology of node sending the packet */
    RuntimeBitset weakTopology;
    /* Number of topologies contained in the current packet */
    unsigned int packetTopologies = 0;
    /* Number of SMEs contained in the current packet */
    unsigned int packetSMEs = 0;

#ifdef CRYPTO
    AesGcm& gcm;
    bool authenticate;
    bool encrypt;
#endif
};

} /* namespace mxnet */
