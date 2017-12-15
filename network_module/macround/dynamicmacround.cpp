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

#include "dynamicmacround.h"
#include "../flooding/periodiccheckfloodingphase.h"
#include "../flooding/syncstatus.h"

namespace miosix {
    DynamicMACRound::~DynamicMACRound() {
    }
    
    void DynamicMACRound::run(MACContext& ctx) {
        auto* nextRound = new DynamicMACRound();
        ctx.setNextRound(nextRound);
        flooding->execute(ctx);
        SyncStatus* syncStatus = ctx.getSyncStatus();
        auto& mac = ctx.getMediumAccessController();
        switch (syncStatus->getInternalStatus()) {
            case SyncStatus::MacroStatus::DESYNCHRONIZED:
                roundtrip = nullptr;
                break;
            case SyncStatus::MacroStatus::IN_SYNC:
                //TODO add phase changes
                roundtrip = new ListeningRoundtripPhase(mac, syncStatus->measuredFrameStart + FloodingPhase::phaseDuration, debug);
                break;
        }
        if (roundtrip != nullptr) roundtrip->execute(ctx);
        syncStatus->next();
        switch (syncStatus->getInternalStatus()) {
            case SyncStatus::MacroStatus::DESYNCHRONIZED:
                nextRound->setFloodingPhase(new HookingFloodingPhase(mac, syncStatus->computedFrameStart, debug));
                break;
            case SyncStatus::MacroStatus::IN_SYNC:
                nextRound->setFloodingPhase(new PeriodicCheckFloodingPhase(mac, syncStatus->getWakeupTime(), debug));
                break;
        }
    }
        
    MACRound* DynamicMACRound::DynamicMACRoundFactory::create(MACContext& ctx, bool debug) const {
        ctx.initializeSyncStatus(new SyncStatus());
        return new DynamicMACRound(ctx.getMediumAccessController(), debug);
    }


}

