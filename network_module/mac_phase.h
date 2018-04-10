/***************************************************************************
 *   Copyright (C)  2017 by Polidori Paolo                                 *
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

#include "medium_access_controller.h"
//#include "timesync/sync_status.h"
#include "interfaces-impl/transceiver.h"
#include "interfaces-impl/power_manager.h"
#include <array>

namespace mxnet {
class SyncStatus;
class MACPhase {
public:
    MACPhase() = delete;
    /**
     * Constructor to be used if the node is the first one sending in this phase.
     * @param ctx the MACContext containing the status of the whole protocol in its phases.
     */
    MACPhase(MACContext& ctx);
    MACPhase(const MACPhase& orig) = delete;
    virtual ~MACPhase() {};
    /**
     * Executes the phase for the slot starting when specified.
     * @param slotStart timestamp identifying the first action computed in the network.
     */
    virtual void execute(long long slotStart) = 0;
    virtual unsigned long long getDuration() = 0;
protected:
    MACContext& ctx;
    SyncStatus* const syncStatus;
    miosix::PowerManager& pm;
    miosix::Transceiver& transceiver;
    std::array<unsigned char, MediumAccessController::maxPktSize> packet;
    miosix::RecvResult rcvResult;
};
}

