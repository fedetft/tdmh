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
#ifdef CRYPTO

    ContextStatus status = DISCONNECTED;

    /**
     * Temporary values for master key and index.
     * These values are computed and used, but not yet committed. Committing them
     * consists in copying these values to masterKey and masterIndex. Committing
     * sets the context status to CONNECTED, meaning we have reached a point where the
     * master index advancement is completely verified.
     */
    unsigned char tempMasterKey[16];
    unsigned int tempMasterIndex;

    /**
     * Called upon resync.
     * Advance hash chain to derive new master key from last known master key.
     * \param newIndex current updated master key index. Cannot decrese in time.
     * @return true if newIndex has acceptable value (masterIndex did not decrease).
     *
     */
    bool attemptResync(unsigned int newIndex) {
        if (status != DISCONNECTED) return false;
        if (newIndex < masterIndex) return false;

        memcpy(tempMasterKey, masterKey, 16);
        for (unsigned int i = masterIndex ; i < newIndex; i++) {
            hash.reset();
            hash.digestBlock(tempMasterKey, tempMasterKey);
        }
        tempMasterIndex = newIndex;
        status = MASTER_UNTRUSTED;
        return true;
    }

    bool advanceResync(unsigned int newIndex) {
        if (status != MASTER_UNTRUSTED && status != CONNECTED) {
            // error: reset
            status = DISCONNECTED;
            return false;
        }
        if (newIndex < masterIndex) return false;

        if (status == CONNECTED) {
            /**
             * A master index change while we are connected can happen: 
             * - if the master has rebooted one or multiple times, or
             * - if resync has just happened, while the network was rekeying
             * However, the index+key change must only be committed after
             * packet verification, by calling commitResync
             */
            status = MASTER_UNTRUSTED;
            memcpy(tempMasterKey, masterKey, 16);
        }
        for (unsigned int i = tempMasterIndex ; i < newIndex; i++) {
            hash.reset();
            hash.digestBlock(tempMasterKey, tempMasterKey);
        }
        tempMasterIndex = newIndex;
        return true;
    }

    void rollbackResync() {
        status = DISCONNECTED;
    }

    void commitResync() {
        if (status != MASTER_UNTRUSTED) {
            // error: reset
            status = DISCONNECTED;
            return;
        }
        memcpy(masterKey, tempMasterKey, 16);
        masterIndex = tempMasterIndex;
        status = CONNECTED;
    }

    void desync() {
        status = DISCONNECTED;
    }

    /**
     * Compute next value for master key, without applying it yet.
     */
    void startRekeying() {
        if (status == MASTER_UNTRUSTED) {
            hash.reset();
            hash.digestBlock(nextMasterKey, tempMasterKey);
            nextMasterIndex = tempMasterIndex + 1;
            status = REKEYING_UNTRUSTED;
        } else if (status == CONNECTED) {
            hash.reset();
            hash.digestBlock(nextMasterKey, masterKey);
            nextMasterIndex = masterIndex + 1;
            status = REKEYING;
        }
    }

    /**
     * Actually rotate the master key with the last precomuted value.
     */
    void applyRekeying() { 
        if (status == REKEYING_UNTRUSTED) {
            tempMasterIndex = nextMasterIndex;
            memcpy(tempMasterKey, nextMasterKey, 16);
            status = MASTER_UNTRUSTED;
        } else if (status == REKEYING) {
            masterIndex = nextMasterIndex;
            memcpy(masterKey, nextMasterKey, 16);
            status = CONNECTED;
        }
    }

#endif

};

} /* namespace mxnet */
