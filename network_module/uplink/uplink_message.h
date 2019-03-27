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

struct UplinkHeader {
    unsigned char hop;
    unsigned char assignee;
    unsigned char numTopology;
    unsigned char numSME;
} __attribute__((packed));

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

class UplinkMessage {
public:

    const int panHeaderSize = 5 //panHeader size is 5 bytes

    /**
     * Constructor which accepts a Packet as parameter, for receiving an
     * UplinkMessage from the radio
     */
    UplinkMessage(const Packet& pkt) : packet(pkt), extracted(false) {};

    /**
     * Constructor with header data and myTopology as parameters,
     * for sending UplinkMessage
     */
    UplinkMessage(unsigned char hop,
                  unsigned char assignee,
                  const TopologyElement& myTopology);

    /**
     * Checks the IEEE 802.15.4 header to tell if the received packet
     * @return true if packet is an UplinkPacket, false otherwise
     */
    bool isUplinkPacket();

    /**
     * Allocate into the UplinkMessage a variable number of TopologyElements and
     * SMEs, to be forwarded to other nodes through the UplinkMessage.
     * NOTE: this method sets numTopology and numSME in the header
     */
    void putTopologiesAndSMEs(UpdatableQueue<TopologyElement>& topologies,
                              UpdatableQueue<StreamManagementElement>& smes);

    /**
     * Extract header and myTopology from the packet.
     */
    void extract();

    /**
     * @return the resulting Packet to be sent on the radio
     * NOTE: this method must check that extracted == false, otherwise the
     * header is missing from the Packet
     */
    Packet getPacket() const {
        if(extracted) throw std::runtime_error("UplinkMessage: can't return packet without header");
        return packet;
    }

    /**
     * @return the hop of the message sender
     */
    unsigned char getHop() const {
        if(!extracted) throw std::runtime_error("UplinkMessage: can't read data before extracting it from the packet");
        return header.hop;
    }

    /**
     * @return the recipient node of the UplinkMessage
     */
    unsigned char getAssignee() const {
        if(!extracted) throw std::runtime_error("UplinkMessage: can't read data before extracting it from the packet");
        return header.assignee;
    }

    /**
     * @return the recipient node of the UplinkMessage
     */
    unsigned char getNumTopology() const {
        if(!extracted) throw std::runtime_error("UplinkMessage: can't read data before extracting it from the packet");
        return header.assignee;
    }

    /**
     * @return the recipient node of the UplinkMessage
     */
    unsigned char getNumSME() const {
        if(!extracted) throw std::runtime_error("UplinkMessage: can't read data before extracting it from the packet");
        return header.assignee;
    }

    /**
     * @return the TopologyElement containing the neighbors of the sender
     */
    TopologyElement getSenderTopology() const {
        if(!extracted) throw std::runtime_error("UplinkMessage: can't read data before extracting it from the packet");
        return topology;
    }

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
     * Insert IEEE 802.15.4 header and UplinkHeader in Packet
     * NOTE: this method should check that Packet is empty
     */
    void putHeader();
    /**
     * Insert myTopology in Packet
     */
    void putMyTopology();


    /* Packet ready to be sent */
    Packet packet;
    /* Header containing information on this packet */
    UplinkHeader header;
    /* TopologyElement containing topology of node sending the packet */
    TopologyElement myTopology;
    /* Flag = true when data has been extracted from the packet */
    bool extracted;
};

} /* namespace mxnet */
