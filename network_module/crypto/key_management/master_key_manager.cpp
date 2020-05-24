#include "master_key_manager.h"
#include <cassert>

namespace mxnet {

void MasterKeyManager::startRekeying() {
    if (status != CONNECTED) {
        assert(false, "MasterKeyManager: unexpected call to startRekeying");
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
        assert(false, "MasterKeyManager: unexpected call to applyRekeying");
    }
    masterIndex = nextMasterIndex;
    memcpy(masterKey, nextMasterKey, 16);
    status = CONNECTED;

    uplinkGCM.rekey(nextUplinkKey);
    downlinkGCM.rekey(nextDownlinkKey);
    timesyncGCM.rekey(nextTimesyncKey);
}

void* MasterKeyManager::getMasterKey() {
    case KeyManagerStatus::CONNECTED: return masterKey;
    case KeyManagerStatus::REKEYING: return masterKey;
    default: assert(false, "MasterKeyManager: unexpected call to getMasterKey");
}

void* MasterKeyManager::getNextMasterKey() {
    case KeyManagerStatus::REKEYING: return nextMasterKey;
    default: assert(false, "MasterKeyManager: unexpected call to getNextMasterKey");
}

unsigned int MasterKeyManager::getMasterIndex() {
    case KeyManagerStatus::CONNECTED: return masterIndex;
    case KeyManagerStatus::REKEYING: return masterIndex;
    default: assert(false, "MasterKeyManager: unexpected call to getMasterIndex");
}

} //namespace mxnet
