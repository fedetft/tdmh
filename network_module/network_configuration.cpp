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
#include "bitwise_ops.h"
#include "mac_context.h"
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
    
    //TODO: check it is minimal, example 0b0101 should be 0x01
}

NetworkConfiguration::NetworkConfiguration(unsigned char maxHops, unsigned short maxNodes, unsigned short networkId,
        unsigned char staticHop, unsigned short panId, short txPower, unsigned int baseFrequency,
        unsigned long long clockSyncPeriod, unsigned char maxForwardedTopologies,
        unsigned long long tileDuration, unsigned long long maxAdmittedRcvWindow,
        unsigned short maxRoundsUnavailableBecomesDead, unsigned short maxRoundsUnreliableParent,
        unsigned char maxMissedTimesyncs,
        TopologyMode topologyMode) :
    maxHops(maxHops), hopBits(BitwiseOps::bitsForRepresentingCount(maxHops)), staticNetworkId(networkId),
    staticHop(staticHop), maxNodes(maxNodes), networkIdBits(BitwiseOps::bitsForRepresentingCount(maxNodes)),
    panId(panId), txPower(txPower), baseFrequency(baseFrequency), topologyMode(topologyMode),
    clockSyncPeriod(clockSyncPeriod), tileDuration(tileDuration), maxAdmittedRcvWindow(maxAdmittedRcvWindow),
    maxMissedTimesyncs(maxMissedTimesyncs), maxForwardedTopologies(maxForwardedTopologies),
    maxRoundsUnavailableBecomesDead(maxRoundsUnavailableBecomesDead),
    maxRoundsUnreliableParent(maxRoundsUnreliableParent),
    controlSuperframe(controlSuperframe) {}

} /* namespace mxnet */
