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

#include "network_configuration.h"
#include "mac_context.h"
#include "downlink_phase/timesync/dynamic_timesync_downlink.h"
#include "downlink_phase/dynamic_schedule_distribution.h"
#include "uplink_phase/dynamic_uplink_phase.h"

namespace mxnet {
class DynamicMACContext : public MACContext {
public:
    DynamicMACContext(const MediumAccessController& mac,
                      miosix::Transceiver& transceiver,
                      const NetworkConfiguration& config);
    DynamicMACContext() = delete;
    virtual ~DynamicMACContext() {};

#ifdef CRYPTO
    enum ContextStatus {
        DISCONNECTED,
        MASTER_UNTRUSTED,
        CONNECTED,
        REKEYING_UNTRUSTED,
        REKEYING
    };

#endif

private:
    ContextStatus status = DISCONNECTED;

#ifdef CRYPTO
    /**
     * Called upon resync.
     * Advance hash chain to derive new master key from last known master key.
     * \param newIndex current updated master key index. Cannot decrese in time.
     * @return true if newIndex has acceptable value (masterIndex did not decrease).
     *
     */
    bool attemptResync(unsigned int newIndex) {
        if (newIndex < masterIndex) return false;

        for (unsigned int i = masterIndex ; i < newIndex; i++) {
            hash.reset();
            hash.digestBlock(masterKey, masterKey);
        }
        // Copy new master key for consistency with normal rekeying behavior
        memcpy(nextMasterKey, masterKey, 16);
        masterIndex = newIndex;
        return true;
    }

#endif

};

} /* namespace mxnet */
