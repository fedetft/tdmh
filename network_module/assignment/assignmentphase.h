/***************************************************************************
 *   Copyright (C)  2018 by Polidori Paolo              *
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


#ifndef ASSIGNMENTPHASE_H
#define ASSIGNMENTPHASE_H

#include "../macphase.h"
#include "interfaces-impl/power_manager.h"
#include <array>
#include <vector>
#include <list>

namespace miosix {
class AssignmentPhase : public MACPhase {
public:
    AssignmentPhase() = delete;
    AssignmentPhase(const AssignmentPhase& orig) = delete;
    virtual ~AssignmentPhase();
    
    long long getPhaseEnd() const { return globalStartTime + phaseDuration; }
    
    static const int assignmentPacketSize = 127;
    static const int retransmissionDelay = 5000000; //5ms
    static const int firstSenderDelay = 500000; //TODO tune it, guessed based on the need to RCV -> SND if was asking before
    static const int packetArrivalAndProcessingTime = 5000000;//32 us * 127 B + tp = 5ms
    static const long long phaseDuration = 2 * retransmissionDelay + firstSenderDelay + packetArrivalAndProcessingTime;
    
protected:
    AssignmentPhase(long long reservationEndTime, unsigned char hop, unsigned char myId, std::vector<unsigned char>* childrenIds) :
            MACPhase(reservationEndTime, reservationEndTime + firstSenderDelay,
                    reservationEndTime + firstSenderDelay + hop * retransmissionDelay),
            transceiver(Transceiver::instance()),
            pm(PowerManager::instance()),
            forwardIds(childrenIds) {};
        
    inline bool isAssignmentPacket(RecvResult& result, int panId, unsigned char myHop) {
        return result.error == RecvResult::OK
                && result.timestampValid && result.size == assignmentPacketSize
                && packet[0] == 0x46 && packet[1] == 0x08
                && packet[2] == myHop - 1
                && packet[3] == static_cast<unsigned char>(panId >> 8)
                && packet[4] == static_cast<unsigned char>(panId & 0xff)
                && packet[5] == 0xff && packet[6] == 0xff;
    }
    
    inline void getEmptyPkt(int panId, unsigned char hop){
        packet = {{
            0x46, //frame type 0b110 (reserved), intra pan
            0x08, //no source addressing, short destination addressing
            hop, //seq no reused as glossy hop count, 0=root node, it has to contain the source hop
            static_cast<unsigned char>(panId>>8),
            static_cast<unsigned char>(panId & 0xff), //destination pan ID
            0xff, 0xff 
                                                              }};
        memset(packet.data() + 7, 0, assignmentPacketSize - 7);
    }
    
    void receiveOrGetEmpty(MACContext& ctx);
    
    /**
     * Warning: not thread safe
     */
    inline unsigned char getMyAssignment() { if(!processed) processPacket(); return mySlot; }
    
    /**
     * Warning: not thread safe
     */
    inline std::list<unsigned char>* getForwardSlots() { if(!processed) processPacket(); return forwardSlots; }
    
    /**
     * Warning: not thread safe
     */
    inline unsigned char getSlotCount() { if(!processed) processPacket(); return slotCount; }
    
    void forwardPacket();
    
    Transceiver& transceiver;
    PowerManager& pm;
    std::vector<unsigned char> packet;
    long long measuredActivityTime;
    unsigned char myId;
    unsigned char mySlot;
private:
    bool processed = false;
    unsigned char slotCount;
    std::vector<unsigned char>* forwardIds;
    std::list<unsigned char>* forwardSlots = new std::list<unsigned char>();
    void processPacket();
};
}

#endif /* ASSIGNMENTPHASE_H */

