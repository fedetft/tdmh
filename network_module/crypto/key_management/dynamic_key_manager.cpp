#include <cassert>
#include <cstdio>
#include "dynamic_key_manager.h"
#include "../hash.h"
#include "../crypto_utils.h"
#include "../aes.h"

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
    uplinkGCM.rekey(nextUplinkKey);
    downlinkGCM.rekey(nextDownlinkKey);
    timesyncGCM.rekey(nextTimesyncKey);
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
    bool result;
    if (doChallengeResponse && status == KeyManagerStatus::MASTER_UNTRUSTED) {
        challengeCtr++;
        if (challengeCtr >= challengeTimeout) {
            challengeCtr = 0;
            rollbackResync();
            result = true;
        } else result = false;
    } else result = false;

    return result;
}

bool DynamicKeyManager::attemptResync(unsigned int newIndex) {
    if (status != KeyManagerStatus::DISCONNECTED) return false;
    if (newIndex < masterIndex) return false;

    memcpy(tempMasterKey, masterKey, 16);
    for (unsigned int i = masterIndex ; i < newIndex; i++) {
        masterHash.digestBlock(tempMasterKey, tempMasterKey);
    }
    tempMasterIndex = newIndex;
    status = KeyManagerStatus::MASTER_UNTRUSTED;
    streamMgr.untrustMaster();
    sendChallenge();

    uplinkHash.digestBlock(uplinkKey, tempMasterKey);
    downlinkHash.digestBlock(downlinkKey, tempMasterKey);
    timesyncHash.digestBlock(timesyncKey, tempMasterKey);
    uplinkGCM.rekey(uplinkKey);
    downlinkGCM.rekey(downlinkKey);
    timesyncGCM.rekey(timesyncKey);

    return true;
}

void DynamicKeyManager::advanceResync() {
    if (status != KeyManagerStatus::MASTER_UNTRUSTED) {
        // error: reset
        status = KeyManagerStatus::DISCONNECTED;
        return;
    }

    masterHash.digestBlock(tempMasterKey, tempMasterKey);
    tempMasterIndex++;

    uplinkHash.digestBlock(uplinkKey, tempMasterKey);
    downlinkHash.digestBlock(downlinkKey, tempMasterKey);
    timesyncHash.digestBlock(timesyncKey, tempMasterKey);
    uplinkGCM.rekey(uplinkKey);
    downlinkGCM.rekey(downlinkKey);
    timesyncGCM.rekey(timesyncKey);

}

void DynamicKeyManager::rollbackResync() {
    status = KeyManagerStatus::DISCONNECTED;
    streamMgr.untrustMaster();
}

void DynamicKeyManager::commitResync() {
    if (status != KeyManagerStatus::MASTER_UNTRUSTED) {
        // error: reset
        status = KeyManagerStatus::DISCONNECTED;
        streamMgr.untrustMaster();
        return;
    }
    memcpy(masterKey, tempMasterKey, 16);
    masterIndex = tempMasterIndex;
    status = KeyManagerStatus::CONNECTED;
    streamMgr.trustMaster();
}

void DynamicKeyManager::attemptAdvance() {
    if (status != KeyManagerStatus::CONNECTED) return;
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
    timesyncGCM.rekey(timesyncKey);
}

void DynamicKeyManager::commitAdvance() {
    if (status != KeyManagerStatus::ADVANCING) return;
    memcpy(masterKey, tempMasterKey, 16);
    masterIndex = tempMasterIndex;
    status = KeyManagerStatus::CONNECTED;

    /**
     * Compute phase keys. Timesync has already been computed.
     */
    uplinkHash.digestBlock(uplinkKey, masterKey);
    downlinkHash.digestBlock(downlinkKey, masterKey);
    uplinkGCM.rekey(uplinkKey);
    downlinkGCM.rekey(downlinkKey);
}

void DynamicKeyManager::rollbackAdvance() {
    if (status != KeyManagerStatus::ADVANCING) return;
    // rollback to the values of masterKey and masterIndex 
    status = KeyManagerStatus::CONNECTED;

    /* restore last valid timesync key */
    timesyncHash.digestBlock(timesyncKey, masterKey);
    timesyncGCM.rekey(timesyncKey);
}

void DynamicKeyManager::desync() {
    status = KeyManagerStatus::DISCONNECTED;
    streamMgr.untrustMaster();
}

bool DynamicKeyManager::verifyResponse(InfoElement info) {
    if (status != KeyManagerStatus::MASTER_UNTRUSTED) {
        printf("DynamicKeyManager: unexpected call to verifyResponse\n");
        assert(false);
    }
    if (info.getType() != InfoType::RESPONSE) return false;
    if (info.getRx() != myId) return false;
    unsigned char response[6];

    // Manually re-deserialize bytes from InfoElement
    response[0] = info.getSrc();
    response[1] = info.getDst();
    response[2] = info.getSrcPort() | (info.getDstPort() << 4);
    StreamParameters p = info.getParams();
    memcpy(response + 3, &p, sizeof(StreamParameters));
    response[5] = info.getTx();

    /**
     * Compute correct answer to challenge.
     * NOTE: this cannot be precomputed earlier, as the response depends on the value of
     * the master key and that value can easily change between the moment the challenge is
     * sent and the moment the master replies. This is especially true when many nodes
     * join the network and the topology has yet to converge.
     * We check the response right after receiving it, in the same tile the master sent it.
     * This ensures that the master key is the same.
     */

    unsigned char key[16];
    unsigned char solution[16];
    xorBytes(key, tempMasterKey, challengeSecret, 16);
    Aes aes(key);
    aes.ecbEncrypt(solution, chal);
    bool valid = true;
    for (unsigned i=0; i<6; i++) {
        if (solution[i] != response[i]) {
            valid = false;
            break;
        }
    }

    return valid;
}

void DynamicKeyManager::sendChallenge() {
    /**
     * NOTE: because we are using SMEs to send challenges, we only have space for 4 random
     * bytes. This is not the best, and it would be a good idea in the future to implement
     * challeges differently and send more bytes.
     */
    const unsigned numBytes = 4;
    for (unsigned i=0; i<numBytes; i++) {
        //TODO: IMPORTANT! Use a secure (P)RNG! (to be implemented)
        chal[i] = rand() % 256;
    }

    streamMgr.enqueueSME(StreamManagementElement::makeChallengeSME(myId, chal));
    challengeCtr = 0;
}

} //namespace mxnet
#endif //ifdef CRYPTO
