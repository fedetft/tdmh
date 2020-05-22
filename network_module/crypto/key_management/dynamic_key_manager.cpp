#include "dynamic_key_manager.h"
#include "../hash.h"

namespace mxnet {

AesGcm& DynamicKeyManager::getUplinkGCM() {
}

AesGcm& DynamicKeyManager::getTimesyncGCM() {
}

AesGcm& DynamicKeyManager::getScheduleDistributionGCM() {
}

/**
 * Compute next value for master key, without applying it yet.
 */
void DynamicKeyManager::startRekeying() {
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
 * Actually rotate the master key with the next precomputed value.
 */
void DynamicKeyManager::applyRekeying() { 
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

void* DynamicKeyManager::getMasterKey() {
}

void* DynamicKeyManager::getNextMasterKey() {
}

unsigned int DynamicKeyManager::getMasterIndex() {
}

bool DynamicKeyManager::attemptResync(unsigned int newIndex) {
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

void DynamicKeyManager::advanceResync() {
    if (status != MASTER_UNTRUSTED) {
        // error: reset
        status = DISCONNECTED;
        return;
    }

    hash.reset();
    hash.digestBlock(tempMasterKey, tempMasterKey);
    tempMasterIndex++;
}

void DynamicKeyManager::rollbackResync() {
    status = DISCONNECTED;
}

void DynamicKeyManager::commitResync() {
    if (status != MASTER_UNTRUSTED) {
        // error: reset
        status = DISCONNECTED;
        return;
    }
    memcpy(masterKey, tempMasterKey, 16);
    masterIndex = tempMasterIndex;
    status = CONNECTED;
}

void DynamicKeyManager::attemptAdvance() {
    if (status != CONNECTED) return;
    /**
     * A master index change while we are connected can happen: 
     * - if the master has rebooted, or
     * - if resync has just happened, while the network was rekeying
     * However, the index+key change must only be committed after
     * packet verification, by calling commitAdvance()
     */
    hash.reset();
    hash.digestBlock(tempMasterKey, masterKey);
    status = ADVANCING;
    tempMasterIndex = masterIndex + 1;
}

void DynamicKeyManager::commitAdvance() {
    if (status != ADVANCING) return;
    memcpy(masterKey, tempMasterKey, 16);
    masterIndex = tempMasterIndex;
    status = CONNECTED;
}

void DynamicKeyManager::rollbackAdvance() {
    if (status != ADVANCING) return;
    // rollback to the values of masterKey and masterIndex: no other action needed
    // except for state change
    status = CONNECTED;
}

void DynamicKeyManager::desync() {
    status = DISCONNECTED;
}

} //namespace mxnet
