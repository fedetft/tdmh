#include "dynamic_key_manager.h"
#include "../hash.h"

namespace mxnet {

/**
 * Compute next value for master key, without applying it yet.
 */
void DynamicKeyManager::startRekeying() {
    if (status == MASTER_UNTRUSTED) {
        masterHash.digestBlock(nextMasterKey, tempMasterKey);
        nextMasterIndex = tempMasterIndex + 1;
        status = REKEYING_UNTRUSTED;
    } else if (status == CONNECTED) {
        masterHash.digestBlock(nextMasterKey, masterKey);
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
    switch (status) {
        case KeyManagerStatus::MASTER_UNTRUSTED: return tempMasterKey;
        case KeyManagerStatus::REKEYING_UNTRUSTED: return tempMasterKey;
        case KeyManagerStatus::CONNECTED: return masterKey;
        case KeyManagerStatus::REKEYING: return masterKey;
        case KeyManagerStatus::ADVANCING: return tempMasterKey;
        default: assert(false);
    }
}

void* DynamicKeyManager::getNextMasterKey() {
    switch (status) {
        case KeyManagerStatus::REKEYING_UNTRUSTED: return tempMasterKey;
        case KeyManagerStatus::REKEYING: return masterKey;
        default: assert(false);
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

bool DynamicKeyManager::attemptResync(unsigned int newIndex) {
    if (status != DISCONNECTED) return false;
    if (newIndex < masterIndex) return false;

    memcpy(tempMasterKey, masterKey, 16);
    for (unsigned int i = masterIndex ; i < newIndex; i++) {
        masterHash.digestBlock(tempMasterKey, tempMasterKey);
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

    masterHash.digestBlock(tempMasterKey, tempMasterKey);
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
    masterHash.digestBlock(tempMasterKey, masterKey);
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
