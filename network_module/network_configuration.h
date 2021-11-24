/***************************************************************************
 *   Copyright (C)  2018-2020 by Polidori Paolo, Valeria Mazzola           *
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

#define CRYPTO

/**
* enables propagation delay compensation
*/
//#define PROPAGATION_DELAY_COMPENSATION


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

    NetworkConfiguration(unsigned char maxHops, unsigned short maxNodes, unsigned short networkId,
            unsigned char staticHop, unsigned short panId, short txPower,
            unsigned int baseFrequency, unsigned long long clockSyncPeriod,
            unsigned char guaranteedTopologies, unsigned char numUplinkPackets,
            unsigned long long tileDuration, unsigned long long maxAdmittedRcvWindow,
            unsigned long long callbacksExecutionTime,
            unsigned short maxRoundsUnavailableBecomesDead,
            unsigned short maxRoundsWeakLinkBecomesDead,
            short minNeighborRSSI, short minWeakNeighborRSSI,
            unsigned char maxMissedTimesyncs,
            bool channelSpatialReuse, bool useWeakTopologies,
#ifdef CRYPTO
            bool authenticateControlMessages, bool encryptControlMessages,
            bool authenticateDataMessages, bool encryptDataMessages,
            bool doMasterChallengeAuthentication,
            unsigned int masterChallengeAuthenticationTimeout,
            unsigned int rekeyingPeriod,
#endif
            ControlSuperframeStructure controlSuperframe=ControlSuperframeStructure());

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
     * @return the maximum time, among all the nodes, for executing receive/send
     * callbacks (i.e. time needed by the application to copy/read data to/from 
     * the dataphase), if callbacks are used.
     */
    unsigned long long getCallbacksExecutionTime() const {
        return callbacksExecutionTime;
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
     * @return the number of uplinks (for which it would be his turn)
     * after which a neighbor not sending a packet is considered disconnected.
     */
    unsigned short getMaxRoundsUnavailableBecomesDead() const {
        return maxRoundsUnavailableBecomesDead;
    }

    /**
     * @return the number of uplinks (for which it would be his turn)
     * after which a weak neighbor not sending a packet is considered disconnected.
     */
    unsigned short getMaxRoundsWeakLinkBecomesDead() const {
        return maxRoundsWeakLinkBecomesDead;
    }

    /**
     * @return the minimum RSSI for considering a link reliable, thus adding the sender node as neighbor.
     */
    short getMinNeighborRSSI() const {
        return minNeighborRSSI;
    }

    /**
     * @return the minimum RSSI for considering a link present, but weak, thus considering possible interference between the nodes.
     */
    short getMinWeakNeighborRSSI() const {
        return minWeakNeighborRSSI;
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

    /**
     * @return true if spatial reuse of channel is enabled
     */
    bool getChannelSpatialReuse() const {
        return channelSpatialReuse;
    }

    /**
     * @return true if weak topologies are collected and forwarded
     * for spatial reuse of channel functionality
     */
    bool getUseWeakTopologies() const {
        return useWeakTopologies;
    }

#ifdef CRYPTO
    /**
     * @return true if control messages are authenticated
     */
    bool getAuthenticateControlMessages() const {
        return authenticateControlMessages;
    }

    /**
     * @return true if control messages are authenticated and encrypted
     */
    bool getEncryptControlMessages() const {
        return encryptControlMessages;
    }

    /**
     * @return true if data messages are authenticated
     */
    bool getAuthenticateDataMessages() const {
        return authenticateDataMessages;
    }

    /**
     * @return true if data messages are authenticated and encrypted
     */
    bool getEncryptDataMessages() const {
        return encryptDataMessages;
    }

    /**
     * @return true if the challenge-response mechanism for authenticating the
     * master node at resync is being used.
     */
    bool getDoMasterChallengeAuthentication() const {
        return doMasterChallengeAuthentication;
    }

    /**
     * @return the number of schedule distribution tiles after which, if the
     * master has not responded, master node authentication via challenge
     * fails.
     */
    unsigned int getMasterChallengeAuthenticationTimeout() const {
        return masterChallengeAuthenticationTimeout;
    }

    /**
     * @return the number of schedule distribution tiles after which the same
     * schedule is resent to force rekeying
     */
    unsigned int getRekeyingPeriod() const {
        return rekeyingPeriod;
    }

#endif // #ifdef CRYPTO


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
    const unsigned long long clockSyncPeriod;
    const unsigned long long tileDuration;
    const unsigned long long maxAdmittedRcvWindow;
    const unsigned long long callbacksExecutionTime;
    const unsigned char maxMissedTimesyncs;
    const unsigned char guaranteedTopologies;
    const unsigned char numUplinkPackets;
    const unsigned short maxRoundsUnavailableBecomesDead;
    const unsigned short maxRoundsWeakLinkBecomesDead;
    const short minNeighborRSSI;
    const short minWeakNeighborRSSI;
    const bool channelSpatialReuse;
    const bool useWeakTopologies;
#ifdef CRYPTO
    const bool authenticateControlMessages;
    const bool encryptControlMessages;
    const bool authenticateDataMessages;
    const bool encryptDataMessages;
    const bool doMasterChallengeAuthentication;
    const unsigned int masterChallengeAuthenticationTimeout;
    const unsigned int rekeyingPeriod;
#endif

    const ControlSuperframeStructure controlSuperframe;
    const unsigned long long controlSuperframeDuration;

    unsigned numSuperframesPerClockSync;
};

}
