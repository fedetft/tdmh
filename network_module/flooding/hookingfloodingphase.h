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

namespace miosix {
class HookingFloodingPhase : public FloodingPhase {
public:
    /**
     * This function creates a HookingFloodingPhase as first phase
     * @param mac
     */
    explicit HookingFloodingPhase(const MediumAccessController& mac) :
            HookingFloodingPhase(mac, 0) {};
    /**
     * This function creates a HookingFloodingPhase as first phase with known frameStart and theoreticalFrameStart
     * @param mac
     * @param frameStart
     */
    HookingFloodingPhase(const MediumAccessController& mac, long long frameStart) :
            FloodingPhase(frameStart, mac.getPanId()) {};
    HookingFloodingPhase() = delete;
    HookingFloodingPhase(const HookingFloodingPhase& orig) = delete;
    void execute(MACContext& ctx) override;
    virtual ~HookingFloodingPhase();
protected:
    HookingFloodingPhase(unsigned short panId, long long frameStart) :
            HookingFloodingPhase(frameStart, panId) {};
private:
};
}

#endif /* HOOKINGFLOODINGPHASE_H */

