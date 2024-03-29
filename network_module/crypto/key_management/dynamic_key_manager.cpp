/***************************************************************************
 *   Copyright (C)  2020 by Valeria Mazzola                                *
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

#include <cassert>
#include <cstdio>
#include "dynamic_key_manager.h"
#include "../hash.h"
#include "../crypto_utils.h"
#include "../aes.h"
#include "../../util/debug_settings.h"

#ifdef CRYPTO

namespace mxnet {

/**
 * Compute next value for master key, without applying it yet.
 */
void DynamicKeyManager::startRekeying() {
    if (status == KeyManagerStatus::MASTER_UNTRUSTED) {
        masterHash.digestBlock(nextMasterKey, tempMasterKey);
        nextMasterIndex = tempMasterIndex + 1;
        status = KeyManagerStatus::REKEYING_UNTRUSTED;
    } else if (status == KeyManagerStatus::CONNECTED) {
        masterHash.digestBlock(nextMasterKey, masterKey);
        nextMasterIndex = masterIndex + 1;
        status = KeyManagerStatus::REKEYING;

        /* Also prepare the stream manager for rekeying */
        unsigned char nextIv[16];
        firstBlockStreamHash.digestBlock(nextIv, nextMasterKey);
        streamMgr.setSecondBlockHash(nextIv);
        memset(nextIv, 0, 16);
    } else {
        printf("DynamicKeyManager: unexpected call to startRekeying\n");
        assert(false);
    }

    if(ENABLE_CRYPTO_KEY_MGMT_DBG) {
        print_dbg("[KM] N=%d starting rekeying\n", myId);
    }

    uplinkHash.digestBlock(nextUplinkKey, nextMasterKey);
    downlinkHash.digestBlock(nextDownlinkKey, nextMasterKey);
    timesyncHash.digestBlock(nextTimesyncKey, nextMasterKey);

}

/**
 * Actually rotate the master key with the next precomputed value.
 */
void DynamicKeyManager::applyRekeying() { 
    if (status == KeyManagerStatus::REKEYING_UNTRUSTED) {
        tempMasterIndex = nextMasterIndex;
        memcpy(tempMasterKey, nextMasterKey, 16);
        status = KeyManagerStatus::MASTER_UNTRUSTED;
    } else if (status == KeyManagerStatus::REKEYING) {
        masterIndex = nextMasterIndex;
        memcpy(masterKey, nextMasterKey, 16);
        status = KeyManagerStatus::CONNECTED;
    } else {
        printf("DynamicKeyManager: unexpected call to applyRekeying\n");
        assert(false);
    }

    if(ENABLE_CRYPTO_KEY_MGMT_DBG) {
        print_dbg("[KM] N=%d applying rekeying\n", myId);
    }

    uplinkOCB.rekey(nextUplinkKey);
    downlinkOCB.rekey(nextDownlinkKey);
    timesyncOCB.rekey(nextTimesyncKey);
}

void* DynamicKeyManager::getMasterKey() {
    switch (status) {
        case KeyManagerStatus::MASTER_UNTRUSTED: return tempMasterKey;
        case KeyManagerStatus::REKEYING_UNTRUSTED: return tempMasterKey;
        case KeyManagerStatus::CONNECTED: return masterKey;
        case KeyManagerStatus::REKEYING: return masterKey;
        case KeyManagerStatus::ADVANCING: return tempMasterKey;
        default: {
            printf("DynamicKeyManager: unexpected call to getMasterKey\n");
            assert(false);
        }
    }
}

void* DynamicKeyManager::getNextMasterKey() {
    switch (status) {
        case KeyManagerStatus::REKEYING_UNTRUSTED: return nextMasterKey;
        case KeyManagerStatus::REKEYING: return nextMasterKey;
        default: {
            printf("DynamicKeyManager: unexpected call to getNextMasterKey\n");
            assert(false);
        }
    }
}

unsigned int DynamicKeyManager::getMasterIndex() {
    switch (status) {
        case KeyManagerStatus::DISCONNECTED: return masterIndex;
        case KeyManagerStatus::MASTER_UNTRUSTED: return tempMasterIndex;
        case KeyManagerStatus::REKEYING_UNTRUSTED: return tempMasterIndex;
        case KeyManagerStatus::CONNECTED: return masterIndex;
        case KeyManagerStatus::REKEYING: return masterIndex;
        case KeyManagerStatus::ADVANCING: return tempMasterIndex;
        default: assert(false);
    }
}

bool DynamicKeyManager::periodicUpdate() {
    /**
     * Check if a challenge verification has recently failed and reset flag and timeout
     */
    if (forceDesync) {
        chalResendCtr = 0;
        chalTimeoutCtr = 0;
        forceDesync = false;
        return true;
    }

    bool result;
    if (doChallengeResponse && (status == KeyManagerStatus::MASTER_UNTRUSTED
                || status == KeyManagerStatus::REKEYING_UNTRUSTED)) {
        chalResendCtr++;
        chalTimeoutCtr++;
        if (chalResendCtr >= chalResendTimeout) {
            chalResendCtr = 0;
            resendChallenge();
            result = false;
        } else if (chalTimeoutCtr >= challengeTimeout) {
            if(ENABLE_CRYPTO_KEY_MGMT_DBG) {
                print_dbg("[KM] N=%d challenge timeout\n", myId);
            }
            chalResendCtr = 0;
            chalTimeoutCtr = 0;
            rollbackResync();
            result = true;
        } else result = false;
    } else result = false;

    return result;
}

void DynamicKeyManager::sendChallenge() {
    if (status != KeyManagerStatus::MASTER_UNTRUSTED &&
            status != KeyManagerStatus::REKEYING_UNTRUSTED) {
        printf("DynamicKeyManager: unexpected call to sendChallenge\n");
        assert(false);
    }
    /**
     * NOTE: because we are using SMEs to send challenges, we only have space for 4 random
     * bytes. This is not the best, and it would be a good idea in the future to implement
     * challeges differently and send more bytes.
     */
    if (sendChallenges) {
        if(ENABLE_CRYPTO_KEY_MGMT_DBG) {
            print_dbg("[KM] N=%d sending challenge SME\n", myId);
        }
        const unsigned numBytes = 4;
        for (unsigned i=0; i<numBytes; i++) {
            //TODO: IMPORTANT! Use a secure (P)RNG! (to be implemented)
            chal[i] = rand() % 256;
        }

        streamMgr.enqueueSME(StreamManagementElement::makeChallengeSME(myId, chal));
        chalTimeoutCtr = 0;
        chalResendCtr = 0;
    }
}

bool DynamicKeyManager::attemptResync(unsigned int newIndex) {
    if (status != KeyManagerStatus::DISCONNECTED) return false;
    if (newIndex < masterIndex) return false;
    if (newIndex - masterIndex > maxIndexDelta) return false;

    memcpy(tempMasterKey, masterKey, 16);
    for (unsigned int i = masterIndex ; i < newIndex; i++) {
        masterHash.digestBlock(tempMasterKey, tempMasterKey);
    }
    tempMasterIndex = newIndex;

    if(ENABLE_CRYPTO_KEY_MGMT_DBG) {
        print_dbg("[KM] N=%d attempting resync\n", myId);
    }

    status = KeyManagerStatus::MASTER_UNTRUSTED;
    streamMgr.untrustMaster();

    uplinkHash.digestBlock(uplinkKey, tempMasterKey);
    downlinkHash.digestBlock(downlinkKey, tempMasterKey);
    timesyncHash.digestBlock(timesyncKey, tempMasterKey);
    uplinkOCB.rekey(uplinkKey);
    downlinkOCB.rekey(downlinkKey);
    timesyncOCB.rekey(timesyncKey);

    return true;
}

void DynamicKeyManager::advanceResync() {
    if (status != KeyManagerStatus::MASTER_UNTRUSTED) {
        // error: reset
        status = KeyManagerStatus::DISCONNECTED;
        return;
    }

    if(ENABLE_CRYPTO_KEY_MGMT_DBG) {
        print_dbg("[KM] N=%d advancing resync\n", myId);
    }

    masterHash.digestBlock(tempMasterKey, tempMasterKey);
    tempMasterIndex++;

    uplinkHash.digestBlock(uplinkKey, tempMasterKey);
    downlinkHash.digestBlock(downlinkKey, tempMasterKey);
    timesyncHash.digestBlock(timesyncKey, tempMasterKey);
    uplinkOCB.rekey(uplinkKey);
    downlinkOCB.rekey(downlinkKey);
    timesyncOCB.rekey(timesyncKey);

}

void DynamicKeyManager::rollbackResync() {
    if(ENABLE_CRYPTO_KEY_MGMT_DBG) {
        print_dbg("[KM] N=%d aborting resync\n", myId);
    }
    status = KeyManagerStatus::DISCONNECTED;
    streamMgr.untrustMaster();
}

void DynamicKeyManager::commitResync() {
    if (status != KeyManagerStatus::MASTER_UNTRUSTED &&
            status != KeyManagerStatus::REKEYING_UNTRUSTED) {
        // error: reset
        status = KeyManagerStatus::DISCONNECTED;
        streamMgr.untrustMaster();
        return;
    }

    if(ENABLE_CRYPTO_KEY_MGMT_DBG) {
        print_dbg("[KM] N=%d committing resync\n", myId);
    }

    if(status == KeyManagerStatus::MASTER_UNTRUSTED) {
        status = KeyManagerStatus::CONNECTED;
    } else if(status == KeyManagerStatus::REKEYING_UNTRUSTED) {
        status = KeyManagerStatus::REKEYING;
    }
    memcpy(masterKey, tempMasterKey, 16);
    masterIndex = tempMasterIndex;
    streamMgr.trustMaster();
}

void DynamicKeyManager::attemptAdvance() {
    if (status != KeyManagerStatus::CONNECTED) return;

    if(ENABLE_CRYPTO_KEY_MGMT_DBG) {
        print_dbg("[KM] N=%d attempting advance\n", myId);
    }

    /**
     * A master index change while we are connected can happen: 
     * - if the master has rebooted, or
     * - if resync has just happened, while the network was rekeying
     * However, the index+key change must only be committed after
     * packet verification, by calling commitAdvance().
     *
     * State ADVANCING is intended to be used ephimerally in a single timesync
     * slot, by calling either commitAdvance or rollbackAdvance after packet
     * verification, to go back to state CONNECTED. Because only the timesync
     * phase will be executed while in state ADVANCING, we only derive the
     * timesync key here.
     */
    masterHash.digestBlock(tempMasterKey, masterKey);
    status = KeyManagerStatus::ADVANCING;
    tempMasterIndex = masterIndex + 1;

    timesyncHash.digestBlock(timesyncKey, tempMasterKey);
    timesyncOCB.rekey(timesyncKey);
}

void DynamicKeyManager::commitAdvance() {
    if (status != KeyManagerStatus::ADVANCING) return;

    if(ENABLE_CRYPTO_KEY_MGMT_DBG) {
        print_dbg("[KM] N=%d committing advance\n", myId);
    }

    memcpy(masterKey, tempMasterKey, 16);
    masterIndex = tempMasterIndex;
    status = KeyManagerStatus::CONNECTED;

    /**
     * Compute phase keys. Timesync has already been computed.
     */
    uplinkHash.digestBlock(uplinkKey, masterKey);
    downlinkHash.digestBlock(downlinkKey, masterKey);
    uplinkOCB.rekey(uplinkKey);
    downlinkOCB.rekey(downlinkKey);
}

void DynamicKeyManager::rollbackAdvance() {
    if (status != KeyManagerStatus::ADVANCING) return;

    if(ENABLE_CRYPTO_KEY_MGMT_DBG) {
        print_dbg("[KM] N=%d aborting advance\n", myId);
    }

    // rollback to the values of masterKey and masterIndex 
    status = KeyManagerStatus::CONNECTED;

    /* restore last valid timesync key */
    timesyncHash.digestBlock(timesyncKey, masterKey);
    timesyncOCB.rekey(timesyncKey);
}

void DynamicKeyManager::desync() {
    if(ENABLE_CRYPTO_KEY_MGMT_DBG) {
        print_dbg("[KM] N=%d desyncing\n", myId);
    }
    status = KeyManagerStatus::DISCONNECTED;
    streamMgr.untrustMaster();
}

bool DynamicKeyManager::verifyResponse(ResponseElement response) {
    if (status != KeyManagerStatus::MASTER_UNTRUSTED &&
            status != KeyManagerStatus::REKEYING_UNTRUSTED) {
        printf("DynamicKeyManager: unexpected call to verifyResponse\n");
        assert(false);
    }
    if (response.getNodeId() != myId) return false;

    /**
     * Compute correct answer to challenge.
     * NOTE: this cannot be precomputed earlier, as the response depends on the value of
     * the master key and that value can easily change between the moment the challenge is
     * sent and the moment the master replies. This is especially true when many nodes
     * join the network and the topology has yet to converge.
     * We check the response right after receiving it, in the same tile the master sent it.
     * This ensures that the master key is the same.
     */
    const unsigned char *bytes = response.getResponseBytes();
    unsigned char key[16];
    unsigned char solution[16];
    xorBytes(key, tempMasterKey, challengeSecret, 16);
    Aes aes(key);
    aes.ecbEncrypt(solution, chal);
    bool valid = true;
    for (unsigned i=0; i<8; i++) {
        if (solution[i] != bytes[i]) {
            valid = false;
            break;
        }
    }

    if(ENABLE_CRYPTO_KEY_MGMT_DBG) {
        print_dbg("[KM] N=%d Verifying challenge response\n", myId);
        if(!valid) print_dbg("[KM] N=%d verify failed!\n", myId);
    }
    /**
     * If verification fails, we set this flag in order to force the timesync to desync
     * the MAC at the next periodicUpdate.
     */
    if(!valid) forceDesync = true;

    return valid;
}

void DynamicKeyManager::resendChallenge() {
    if(ENABLE_CRYPTO_KEY_MGMT_DBG) {
        print_dbg("[KM] N=%d resending challenge SME\n", myId);
    }

    streamMgr.enqueueSME(StreamManagementElement::makeChallengeSME(myId, chal));
}

} //namespace mxnet
#endif //ifdef CRYPTO
