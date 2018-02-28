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

#include "../macphase.h"
#include "interfaces-impl/power_manager.h"

namespace miosix {
class TopologyDiscoveryPhase : public MACPhase {
public:
    TopologyDiscoveryPhase(const TopologyDiscoveryPhase& orig) = delete;
    virtual ~TopologyDiscoveryPhase();
    
    long long getPhaseEnd() const { return globalStartTime + phaseDuration; }
    long long getNodeTransmissionTime(unsigned short networkId) const {
        return globalFirstActivityTime + (nodesCount - networkId - 1) * (transmissionInterval + packetArrivalAndProcessingTime);
    }
    
    static const int transmissionInterval = 1000000; //1ms
    static const int firstSenderDelay = 1000000; //TODO tune it, guessed based on the need to RCV -> SND if was asking before
    static const int packetArrivalAndProcessingTime = 5000000;//32 us * 127 B + tp = 5ms
    static const int packetTime = 4256000;//physical time for transmitting/receiving the packet: 4256us
    const long long phaseDuration;
protected:
    TopologyDiscoveryPhase(long long roundtripEndTime, unsigned short networkId, unsigned short nodesCount) :
            MACPhase(roundtripEndTime, roundtripEndTime + firstSenderDelay,
                    roundtripEndTime + (nodesCount - networkId) * (transmissionInterval + packetArrivalAndProcessingTime)
                    + firstSenderDelay),
            phaseDuration(nodesCount * (transmissionInterval + packetArrivalAndProcessingTime) + firstSenderDelay),
            transceiver(Transceiver::instance()),
            pm(PowerManager::instance()),
            nodesCount(nodesCount) {};
    
    Transceiver& transceiver;
    PowerManager& pm;
    unsigned short nodesCount;
private:
};
}
