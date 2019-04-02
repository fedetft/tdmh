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

/**
 * This class defines the control superframe structure
 */
class ControlSuperframeStructure
{
public:
    /**
     * Default constructor, create the default control superframe with two
     * tiles, where the first is a control downlink and the second a control
     * uplink tile
     */
    ControlSuperframeStructure() : bitmask(0b01), sz(2) {}
    
    /**
     * Create a custom control superframe
     * \param bitmask bitmask defining the control superframe structure.
     * Bit 0 is the first tile in the control superframe, If a bit is 1, the
     * corresponding tile is a control downlink, else a control uplink.
     * Note that the first tile has to be a control downlink, and there must
     * be at least one control uplink tile in the superframe
     * \param size number of tiles in the control superframe, from 2 to 32
     * \throws logic_error if the parameters are not valid
     */
    ControlSuperframeStructure(unsigned int bitmask, int size)
        : bitmask(bitmask), sz(size) { validate(); }
    
    /**
     * \return the number of tiles in the control superframe
     */
    int size() const { return sz; }
    
    /**
     * \param index tile index from 0 to size()-1
     * \return true if the tile is a control downlink tile
     */
    bool isControlDownlink(int index) const { return (bitmask>>index) & 1; }
    
    /**
     * \param index tile index from 0 to size()-1
     * \return true if the tile is a control uplink tile
     */
    bool isControlUplink(int index) const { return !isControlDownlink(index); }
    
    /**
     * \return the number of downlink slots in the control superframe
     */
    int countDownlinkSlots() const { return size() - countUplinkSlots(); }
    
    /**
     * \return the number of uplink slots in the control superframe
     */
    int countUplinkSlots() const;
    
private:
    /**
     * Validates the control superframe structure
     */
    void validate() const;
    
    const unsigned int bitmask;
    const int sz;
};



class NetworkConfiguration {
public:
    enum TopologyMode {
        NEIGHBOR_COLLECTION,
        ROUTING_VECTOR
    };

    NetworkConfiguration(unsigned char maxHops, unsigned short maxNodes, unsigned short networkId,
            unsigned char staticHop, unsigned short panId, short txPower,
            unsigned int baseFrequency, unsigned long long clockSyncPeriod,
            unsigned char guaranteedTopologies, unsigned char numUplinkPackets,
            unsigned long long tileDuration, unsigned long long maxAdmittedRcvWindow,
            unsigned short maxRoundsUnavailableBecomesDead,
            short minNeighborRSSI, unsigned char maxMissedTimesyncs,
            ControlSuperframeStructure controlSuperframe=ControlSuperframeStructure(),
            TopologyMode topologyMode=TopologyMode::NEIGHBOR_COLLECTION);

    /**
     * @return the reference frequency for the protocol.
     */
    unsigned int getBaseFrequency() const {
        return baseFrequency;
    }

    /**
     * @return if the network id is statically or dinamically configured.
     */
    bool isDynamicNetworkId() const {
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
    unsigned char getMaxMissedTimesyncs() const {
        return maxMissedTimesyncs;
    }

    /**
     * @return the maximum time width that the mac protocol should support.
     * Id est, the threshold of synchronization beyond which the protocol can't work,
     * since it would have a too few accurate timing precision.
     */
    unsigned getMaxAdmittedRcvWindow() const {
        return maxAdmittedRcvWindow;
    }
    
    /**
     * @return the duration of the clock synchronization period
     */
    unsigned long long getClockSyncPeriod() const {
        return clockSyncPeriod;
    }
    
    /**
     * @return the tile duration
     */
    unsigned long long getTileDuration() const {
        return tileDuration;
    }
    
    /**
     * @return the control superframe structure
     */
    ControlSuperframeStructure getControlSuperframeStructure() const {
        return controlSuperframe;
    }

    /**
     * @return the number of topology messages that is guaranteed to be forwarded in an uplink message.
     */
    unsigned char getGuaranteedTopologies() const {
        return guaranteedTopologies;
    }
    
    /**
     * @return the number of uplink packets per uplink tile
     */
    unsigned char getNumUplinkPackets() const {
        return numUplinkPackets;
    }

    /**
     * @return the maximum number of hops the networks supports.
     */
    unsigned char getMaxHops() const {
        return maxHops;
    }

    /**
     * @return the maximum number of nodes the network supports.
     */
    unsigned short getMaxNodes() const {
        return maxNodes;
    }

    /**
     * @return the size of the bitmask used to store neighbors of a node.
     */
    unsigned short getNeighborBitmaskSize() const {
        return ((maxNodes + 7) / 8);
    }

    /**
     * @return the number of uplinks (for which it would be his turn) after which a neighbor not sending a packet is considered disconnected.
     */
    unsigned short getMaxRoundsUnavailableBecomesDead() const {
        return maxRoundsUnavailableBecomesDead;
    }

    /**
     * @return the minimum RSSI for considering a link reliable, thus adding the sender node as neighbor.
     */
    short getMinNeighborRSSI() const {
        return minNeighborRSSI;
    }

    /**
     * @return the pan id used in the timesync packets' header.
     */
    unsigned short getPanId() const {
        return panId;
    }

    /**
     * @return the statically configured network id.
     */
    unsigned short getStaticNetworkId() const {
        return staticNetworkId;
    }

    /**
     * @return the statically configured hop.
     */
    unsigned short getStaticHop() const {
        return staticHop;
    }

    /**
     * @return the topology mode used in the network to perform topology collection.
     */
    TopologyMode getTopologyMode() const {
        return topologyMode;
    }

    /**
     * @return the power, in dBm, at which the radio operates.
     */
    short getTxPower() const {
        return txPower;
    }
    
    /**
     * @return the number of uplink slots
     */
    unsigned int getNumUplinkSlotperSuperframe() const {
        return numUplinkPerSuperframe;
    }

    /**
     * @return the number of downlink slots
     */
    unsigned int getNumDownlinkSlotperSuperframe() const {
        return numDownlinkPerSuperframe;
    }

    /**
     * @return the duration of a control superframe
     */
    unsigned long long getControlSuperframeDuration() const {
        return controlSuperframeDuration;
    }

    /**
     * @return the number of superframes contained within a clock sync period
     */
    unsigned getNumSuperframesPerClockSync() const {
        return numSuperframesPerClockSync;
    }

private:
    /**
     * Validates the times configured
     */
    void validate() const;

    const unsigned char maxHops;
    const unsigned char hopBits;
    const unsigned char numUplinkPerSuperframe;
    const unsigned char numDownlinkPerSuperframe;
    const bool dynamicNetworkId = false;
    const unsigned short staticNetworkId;
    const unsigned short staticHop;
    const unsigned short maxNodes;
    const unsigned short panId;
    const short txPower;
    const unsigned int baseFrequency;
    const TopologyMode topologyMode;
    const unsigned long long clockSyncPeriod;
    const unsigned long long tileDuration;
    const unsigned long long maxAdmittedRcvWindow;
    const unsigned char maxMissedTimesyncs;
    const unsigned char guaranteedTopologies;
    const unsigned char numUplinkPackets;
    const unsigned short maxRoundsUnavailableBecomesDead;
    const short minNeighborRSSI;
    const ControlSuperframeStructure controlSuperframe;
    const unsigned long long controlSuperframeDuration;

    unsigned numSuperframesPerClockSync;
};

}
