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

#include "network_configuration.h"
#include "mac_context.h"
#include "tdmh.h"
#include "util/bitwise_ops.h"
#include "util/debug_settings.h"
#include "uplink_phase/uplink_message.h"
#include <stdexcept>

namespace mxnet {

void ControlSuperframeStructure::validate() const
{
    // Size between 2 and 32
    if(size()<2 || size()>32) throw std::logic_error("size");
    
    // First tile has to be a control downlink
    if(isControlDownlink(0)==false) throw std::logic_error("first tile");
    
    // Has to have at least one control uplink
    for(int i=1;i<size();i++) if(isControlDownlink(i)==false) return;
    throw std::logic_error("no control uplink");
    
    //TODO: double check that is minimal.
    //Check it is minimal, example 0b0101 should be 0b01
    unsigned char halfLength = BitwiseOps::bitsForRepresentingCount(bitmask) >> 1;
    if ((bitmask >> halfLength) == (bitmask & ((1 << halfLength) - 1)))
        throw std::logic_error("must be minimal");
}

int ControlSuperframeStructure::countUplinkSlots() const
{
    int result = 0;
    for(int i = 0; i < size(); i++) if(isControlUplink(i)) result++;
    return result;
}

NetworkConfiguration::NetworkConfiguration(unsigned char maxHops, unsigned short maxNodes, unsigned short networkId,
        unsigned char staticHop, unsigned short panId, short txPower, unsigned int baseFrequency,
        unsigned long long clockSyncPeriod, unsigned char guaranteedTopologies,
        unsigned char numUplinkPackets, unsigned long long tileDuration,
        unsigned long long maxAdmittedRcvWindow,
        unsigned short maxRoundsUnavailableBecomesDead,
        unsigned short maxRoundsWeakLinkBecomesDead, 
        short minNeighborRSSI, short minWeakNeighborRSSI,
        unsigned char maxMissedTimesyncs, bool channelSpatialReuse,
        bool useWeakTopologies,
#ifdef CRYPTO
        bool authenticateControlMessages, bool encryptControlMessages,
        bool authenticateDataMessages, bool encryptDataMessages,
        bool doMasterChallengeAuthentication,
        unsigned int masterChallengeAuthenticationTimeout,
        unsigned int rekeyingPeriod,
#endif
        ControlSuperframeStructure controlSuperframe) :
    maxHops(maxHops), hopBits(BitwiseOps::bitsForRepresentingCount(maxHops)),
    numUplinkPerSuperframe(controlSuperframe.countUplinkSlots()), numDownlinkPerSuperframe(controlSuperframe.countDownlinkSlots()),
    staticNetworkId(networkId), staticHop(staticHop), maxNodes(maxNodes),
    panId(panId), txPower(txPower), baseFrequency(baseFrequency),
    clockSyncPeriod(clockSyncPeriod), tileDuration(tileDuration), maxAdmittedRcvWindow(maxAdmittedRcvWindow),
    maxMissedTimesyncs(maxMissedTimesyncs), guaranteedTopologies(guaranteedTopologies),
    numUplinkPackets(numUplinkPackets),
    maxRoundsUnavailableBecomesDead(maxRoundsUnavailableBecomesDead),
    maxRoundsWeakLinkBecomesDead(maxRoundsWeakLinkBecomesDead),
    minNeighborRSSI(minNeighborRSSI), minWeakNeighborRSSI(minWeakNeighborRSSI),
    channelSpatialReuse(channelSpatialReuse),
    useWeakTopologies(useWeakTopologies),
#ifdef CRYPTO
    authenticateControlMessages(authenticateControlMessages | encryptControlMessages),
    encryptControlMessages(encryptControlMessages),
    authenticateDataMessages(authenticateDataMessages | encryptDataMessages),
    encryptDataMessages(encryptDataMessages),
    doMasterChallengeAuthentication(doMasterChallengeAuthentication),
    masterChallengeAuthenticationTimeout(masterChallengeAuthenticationTimeout),
    rekeyingPeriod(rekeyingPeriod),
#endif
    controlSuperframe(controlSuperframe),
    controlSuperframeDuration(tileDuration * controlSuperframe.size()),
    numSuperframesPerClockSync(clockSyncPeriod / controlSuperframeDuration) {
    validate();
}

void NetworkConfiguration::validate() const {
    const int totAvailableBytes = getFirstUplinkPacketCapacity(*this) +
        (numUplinkPackets - 1) * getOtherUplinkPacketCapacity(*this);
    auto topologySize = guaranteedTopologies * TopologyElement::maxSize(
                                getNeighborBitmaskSize(), useWeakTopologies);
    if(topologySize > totAvailableBytes) {
        throwLogicError("guaranteedTopologies size of %d exceeds UplinkMessage available space of %d",topologySize,totAvailableBytes);
    }
    if(clockSyncPeriod % controlSuperframeDuration != 0)
        throwLogicError("control superframe (%lld) does not divide clock sync period (%lld)",
                        controlSuperframeDuration, clockSyncPeriod);
    // maxNodes must be a multiple of 8 because otherwise the RuntimeBitset won't work correctly
    if((maxNodes % 8) != 0)
      throwLogicError("Configuration error: maxNodes must be a multiple of 8");
}

} /* namespace mxnet */
