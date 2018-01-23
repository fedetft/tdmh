/***************************************************************************
 *   Copyright (C)  2017 by Terraneo Federico, Polidori Paolo              *
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

#ifndef FLOODINGPHASE_H
#define FLOODINGPHASE_H

#include "interfaces-impl/transceiver.h"
#include "interfaces-impl/power_manager.h"
#include "../mediumaccesscontroller.h"
#include "../macphase.h"
#include <utility>

namespace miosix {
class FloodingPhase : public MACPhase {
public:
    FloodingPhase() = delete;
    FloodingPhase(const FloodingPhase& orig) = delete;
    /**
     * Needs to be periodically called to send/receive the synchronization
     * packet. This member function sleeps till it's time to send the packet,
     * then sends it and returns. If this function isn't called again within
     * the synchronization period, the synchronization won't work.
     * \return true if the node desynchronized
     */
    //virtual void execute()=0;
    
    virtual ~FloodingPhase();
    
    //Minimum ~550us, 200us of slack added
    static const int rootNodeWakeupAdvance = 750000;
    static const unsigned short maxMissedPackets = 3;
    static const int syncPacketSize = 7;
    static const int rebroadcastInterval = 1016000; //32us per-byte + 600us total delta
    //5 byte (4 preamble, 1 SFD) * 32us/byte
    static const int packetPreambleTime = 160000;
    //350us and forced receiverWindow=1 fails, keeping this at minimum
    //This is dependent on the optimization level, i usually use level -O2
    static const int syncNodeWakeupAdvance = 450000;
    static const long long phaseDuration = 500000000; //duration of the flooding phase, 500ms
    
protected:
    FloodingPhase(long long startTime, unsigned short panId) :
            MACPhase(startTime),
            transceiver(Transceiver::instance()),
            pm(PowerManager::instance()),
            panId(panId) {};
    /**
     * Rebroadcst the synchronization packet using glossy
     * \param receivedTimestamp the timestamp when the packet was received
     * \param packet the received packet
     */
    void rebroadcast(long long receivedTimestamp, unsigned char *packet);
        
    inline bool isSyncPacket(RecvResult& result, unsigned char *packet) {
        return result.error == RecvResult::OK
                && result.timestampValid && result.size == syncPacketSize
                && packet[0] == 0x46 && packet[1] == 0x08
                && packet[3] == static_cast<unsigned char>(panId >> 8)
                && packet[4] == static_cast<unsigned char>(panId & 0xff)
                && packet[5] == 0xff && packet[6] == 0xff;
    }
    
    long long fastNegMul(long long a,unsigned int bi, unsigned int bf){
        if(a<0){
            return -mul64x32d32(-a,bi,bf);
        }else{
            return mul64x32d32(a,bi,bf);
        }
    }
    
    Transceiver& transceiver;
    PowerManager& pm;
    unsigned short panId;
    
private:

};
}

#endif /* FLOODINGPHASE_H */

