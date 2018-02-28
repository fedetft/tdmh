/***************************************************************************
 *   Copyright (C)  2017 by Polidori Paolo, Terraneo Federico,             *
 *                          Riccardi Fabiano                               *
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

#ifndef ROUNDTRIPPHASE_H
#define ROUNDTRIPPHASE_H

#include "../mediumaccesscontroller.h"
#include "../macphase.h"
#include "interfaces-impl/transceiver.h"
#include "interfaces-impl/power_manager.h"
#include <utility>

namespace miosix {
class RoundtripPhase : public MACPhase {
public:
    /**
     * First phase constructor
     * @param mac
     * @param startTime
     */
    RoundtripPhase(long long masterFloodingEndTime) : 
            MACPhase(masterFloodingEndTime, masterFloodingEndTime + senderDelay + tPktElab),
            transceiver(Transceiver::instance()),
            pm(PowerManager::instance()) {};
    RoundtripPhase() = delete;
    RoundtripPhase(const RoundtripPhase& orig) = delete;
    virtual ~RoundtripPhase();
    
    long long getPhaseEnd() const { return globalStartTime + phaseDuration; }
    
    static const unsigned int receiverWindow = 200000; //200us
    static const unsigned int senderDelay = 100000; //100us
    static const unsigned short tPktElab = 50000; //50us
    static const unsigned int replyDelay = 100000; //100us
    static const unsigned short askPacketSize = 7;
    static const unsigned int tAskPkt = 416000; //416us
    static const unsigned short replyPacketSize = 125;
    static const unsigned int tReplyPkt = 4256000; //4256us
    //phase should last at least 5588200 ns. Given 6ms.
    //tPktElab + maxPropDelay + tAskPkt + rcvWin + replyDelay + maxPropDelay + tReplyPkt + rcvWin + tPktElab
    static const unsigned long long phaseDuration = 6000000;
    
    /**
     * We are very limited with possible value to send through led bar, 
     * so we have to choose accurately the quantization level because it limits
     * the maximum value. We can send values from [0; replyPacketSize*2-1], so
     * with accuracy 15ns --> max value is 15*253=3795ns, or in meter:
     * 3,795e9s * 3e8m/s=1138.5m
     * Remember that each tick@48Mhz are 6.3m at speed of light.
     */
    static const int accuracy = 15;
protected:
    Transceiver& transceiver;
    PowerManager& pm;
    long long lastDelay;
    long long totalDelay;
    
    inline bool isRoundtripAskPacket(RecvResult& result, unsigned char *packet, unsigned short panId, unsigned char myHop) {
        return result.error == RecvResult::OK && result.timestampValid
                && result.size == askPacketSize
                && packet[0] == 0x46 && packet[1] == 0x08
                && packet[2] == myHop + 1
                && packet[3] == static_cast<unsigned char> (panId >> 8)
                && packet[4] == static_cast<unsigned char> (panId & 0xff)
                && packet[5] == 0xff && packet[6] == 0xff;
    }
    
    inline bool isRoundtripPacket(RecvResult& result, unsigned char *packet, unsigned short panId, unsigned char myHop) {
        //TODO fix this, actually the packet doesn't have this format
        if (!(result.error == RecvResult::OK && result.timestampValid
                && result.size == replyPacketSize
                && packet[0] == 0x46 && packet[1] == 0x08
                && packet[2] == myHop + 1
                && packet[3] == static_cast<unsigned char> (panId >> 8)
                && packet[4] == static_cast<unsigned char> (panId & 0xff)
                && packet[5] == 0xff && packet[6] == 0xff)) return false;
        bool legit = true;
        bool isFs = true;
        for (int i = 7; i < replyPacketSize && legit; i++) {
            auto elem = packet[i];
            if (isFs) {
                if (elem != 0xFF) {
                    isFs = elem != 0xF0;
                    legit = elem == 0xF0 || !elem;
                }
            } else if (elem) legit = false;
        }
        return legit;
    }
};
}

#endif /* ROUNDTRIPPHASE_H */

