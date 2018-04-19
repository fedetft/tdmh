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

#pragma once

namespace mxnet {
class NetworkConfiguration {
public:
    enum TopologyMode {
        NEIGHBOR_COLLECTION,
        ROUTING_VECTOR
    };

    NetworkConfiguration(const unsigned char maxHops, const unsigned short maxNodes, unsigned short networkId,
            const unsigned short panId, const short txPower, const unsigned int baseFrequency,
            const unsigned long long slotframeDuration, const unsigned char maxForwardedTopologies,
            const unsigned short scheduleDownlinkPerSlotframeCount, const unsigned long long maxAdmittedRcvWindow,
            const unsigned short maxRoundsUnavailableBecomesDead, const unsigned short maxRoundsUnreliableParent,
            const unsigned char maxMissedTimesyncs,
            const TopologyMode topologyMode=TopologyMode::NEIGHBOR_COLLECTION);

    /**
     * @return the reference frequency for the protocol.
     */
    const unsigned int getBaseFrequency() const {
        return baseFrequency;
    }

    /**
     * @return if the network id is statically or dinamically configured.
     */
    const bool isDynamicNetworkId() const {
        return dynamicNetworkId;
    }

    /**
     * @return the number of bits needed to represent the hop count
     */
    unsigned char getHopBits() const {
        return hopBits;
    }

    /**
     * @return number of time synchronization missed, based on the local clock progress
     */
    const unsigned char getMaxMissedTimesyncs() const {
        return maxMissedTimesyncs;
    }

    /**
     * @return the maximum time width that the mac protocol should support.
     * Id est, the threshold of synchronization beyond which the protocol can't work,
     * since it would have a too few accurate timing precision.
     */
    const unsigned char getMaxAdmittedRcvWindow() const {
        return maxAdmittedRcvWindow;
    }

    /**
     * @return the duration of the slotframe, which corresponds to the interval separating two timesync downlink phases.
     */
    const unsigned long long getSlotframeDuration() const {
        return slotframeDuration;
    }

    /**
     * @return the maximum number of topology messages that can be forwarded in an uplink.
     */
    const unsigned char getMaxForwardedTopologies() const {
        return maxForwardedTopologies;
    }

    /**
     * @return the maximum number of hops the networks supports.
     */
    const unsigned char getMaxHops() const {
        return maxHops;
    }

    /**
     * @return the maximum number of nodes the network supports.
     */
    const unsigned short getMaxNodes() const {
        return maxNodes;
    }

    /**
     * @return the number of uplinks (for which it would be his turn) after which a neighbor not sending a packet is considered disconnected.
     */
    const unsigned short getMaxRoundsUnavailableBecomesDead() const {
        return maxRoundsUnavailableBecomesDead;
    }

    /**
     * @return the number of uplinks (for which it would be his turn) after which a parent non sending a packet is considered disconnected.
     */
    const unsigned short getMaxRoundsUnreliableParent() const {
        return maxRoundsUnreliableParent;
    }

    /**
     * @return the number of bits needed to represent a network id.
     */
    unsigned short getNetworkIdBits() const {
        return networkIdBits;
    }

    /**
     * @return the pan id used in the timesync packets' header.
     */
    const unsigned short getPanId() const {
        return panId;
    }

    /**
     * @return the statically configured network id.
     */
    const unsigned short getStaticNetworkId() const {
        return staticNetworkId;
    }

    /**
     * @return the topology mode used in the network to perform topology collection.
     */
    const TopologyMode getTopologyMode() const {
        return topologyMode;
    }

    /**
     * @return the power, in dBm, at which the radio operates.
     */
    const short getTxPower() const {
        return txPower;
    }

    /**
     * @return the number of schedule downlinks each slotframe contains.
     */
    const unsigned short getScheduleDownlinkPerSlotframeCount() const {
        return scheduleDownlinkPerSlotframeCount;
    }
private:
    const unsigned char maxHops;
    unsigned char hopBits;
    const bool dynamicNetworkId = false;
    const unsigned short staticNetworkId;
    const unsigned short maxNodes;
    unsigned short networkIdBits;
    const unsigned short panId;
    const short txPower;
    const unsigned int baseFrequency;
    const TopologyMode topologyMode;
    const unsigned long long slotframeDuration;
    const unsigned char maxMissedTimesyncs;
    const unsigned long long maxAdmittedRcvWindow;
    const unsigned char maxForwardedTopologies;
    const unsigned short maxRoundsUnavailableBecomesDead;
    const unsigned short maxRoundsUnreliableParent;
    const unsigned short scheduleDownlinkPerSlotframeCount;
};
}

