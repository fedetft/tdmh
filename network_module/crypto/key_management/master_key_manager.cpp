#include "master_key_manager.h"
#include <cassert>
#include <cstdio>

namespace mxnet {

void MasterKeyManager::startRekeying() {
    if (status != CONNECTED) {
        printf("MasterKeyManager: unexpected call to startRekeying\n");
        assert(false);
    }
    masterHash.digestBlock(nextMasterKey, masterKey);
    nextMasterIndex = masterIndex + 1;
    status = REKEYING;

    uplinkHash.digestBlock(nextUplinkKey, nextMasterKey);
    downlinkHash.digestBlock(nextDownlinkKey, nextMasterKey);
    timesyncHash.digestBlock(nextTimesyncKey, nextMasterKey);
}

void MasterKeyManager::applyRekeying() {
    if (status != REKEYING) {
        printf("MasterKeyManager: unexpected call to applyRekeying\n");
        assert(false);
    }
    masterIndex = nextMasterIndex;
    memcpy(masterKey, nextMasterKey, 16);
    status = CONNECTED;

    uplinkGCM.rekey(nextUplinkKey);
    downlinkGCM.rekey(nextDownlinkKey);
    timesyncGCM.rekey(nextTimesyncKey);
}

void* MasterKeyManager::getMasterKey() {
    switch (status) {
        case KeyManagerStatus::CONNECTED: return masterKey;
        case KeyManagerStatus::REKEYING: return masterKey;
        default: {
            printf("MasterKeyManager: unexpected call to getMasterKey\n");
            assert(false);
        }
    }
}

void* MasterKeyManager::getNextMasterKey() {
    switch (status) {
        case KeyManagerStatus::REKEYING: return nextMasterKey;
        default: {
            printf("MasterKeyManager: unexpected call to getNextMasterKey\n");
            assert(false);
        }
    }
}

unsigned int MasterKeyManager::getMasterIndex() {
    switch (status) {
        case KeyManagerStatus::CONNECTED: return masterIndex;
        case KeyManagerStatus::REKEYING: return masterIndex;
        default: {
            printf("MasterKeyManager: unexpected call to getMasterIndex\n");
            assert(false);
        }
    }
}

} //namespace mxnet
