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

#ifndef HOOKINGFLOODINGPHASE_H
#define HOOKINGFLOODINGPHASE_H

#include "floodingphase.h"
#include "time_synchronizers/synchronizer.h"
#include "time_synchronizers/flopsync2.h"
#include "../maccontext.h"
#include "syncstatus.h"
#include <utility>
#include <stdexcept>

namespace miosix {
class HookingFloodingPhase : public FloodingPhase {
public:
    /**
     * This function creates a HookingFloodingPhase as first phase
     */
    HookingFloodingPhase() :
            HookingFloodingPhase(0, 0) {};
    /**
     * This function creates a HookingFloodingPhase as first phase with known startTime and expectedReceivingTime
     * @param startTime
     */
    explicit HookingFloodingPhase(long long masterSendTime, long long expectedReceivingTime) :
            FloodingPhase(masterSendTime, expectedReceivingTime) {};
    HookingFloodingPhase(const HookingFloodingPhase& orig) = delete;
    void execute(MACContext& ctx) override;
    virtual ~HookingFloodingPhase();
    inline virtual bool isSyncPacket(RecvResult& result, unsigned char *packet, unsigned short panId) {
        return result.error == RecvResult::OK
                && result.timestampValid && result.size == syncPacketSize
                && packet[0] == 0x46 && packet[1] == 0x08
                && packet[3] == static_cast<unsigned char>(panId >> 8)
                && packet[4] == static_cast<unsigned char>(panId & 0xff)
                && packet[5] == 0xff && packet[6] == 0xff;
    }
protected:
    bool consumed = false;
    SyncStatus* syncStatus = nullptr;
private:
};
}

#endif /* HOOKINGFLOODINGPHASE_H */

