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
class MACContext;
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

    const unsigned int getBaseFrequency() const {
        return baseFrequency;
    }

    const bool isDynamicNetworkId() const {
        return dynamicNetworkId;
    }

    unsigned char getHopBits() const {
        return hopBits;
    }

    const unsigned char getMaxMissedTimesyncs() const {
        return maxMissedTimesyncs;
    }

    const unsigned char getMaxAdmittedRcvWindow() const {
        return maxAdmittedRcvWindow;
    }

    const unsigned long long getSlotframeDuration() const {
        return slotframeDuration;
    }

    const unsigned char getMaxForwardedTopologies() const {
        return maxForwardedTopologies;
    }

    const unsigned char getMaxHops() const {
        return maxHops;
    }

    const unsigned short getMaxNodes() const {
        return maxNodes;
    }

    const unsigned short getMaxRoundsUnavailableBecomesDead() const {
        return maxRoundsUnavailableBecomesDead;
    }

    const unsigned short getMaxRoundsUnreliableParent() const {
        return maxRoundsUnreliableParent;
    }

    unsigned short getNetworkIdBits() const {
        return networkIdBits;
    }

    const unsigned short getPanId() const {
        return panId;
    }

    const unsigned short getStaticNetworkId() const {
        return staticNetworkId;
    }

    const TopologyMode getTopologyMode() const {
        return topologyMode;
    }

    const short getTxPower() const {
        return txPower;
    }


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

