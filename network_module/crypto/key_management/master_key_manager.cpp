#include "master_key_manager.h"
#include <cassert>
#include <cstdio>

#ifdef CRYPTO
namespace mxnet {

void MasterKeyManager::startRekeying() {
    if (status != KeyManagerStatus::CONNECTED) {
        printf("MasterKeyManager: unexpected call to startRekeying\n");
        assert(false);
    }
    masterHash.digestBlock(nextMasterKey, masterKey);
    nextMasterIndex = masterIndex + 1;
    status = KeyManagerStatus::REKEYING;

    uplinkHash.digestBlock(nextUplinkKey, nextMasterKey);
    downlinkHash.digestBlock(nextDownlinkKey, nextMasterKey);
    timesyncHash.digestBlock(nextTimesyncKey, nextMasterKey);

    /* Also prepare the stream manager for rekeying */
    unsigned char nextIv[16];
    firstBlockStreamHash.digestBlock(nextIv, nextMasterKey);
    streamMgr.setSecondBlockHash(nextIv);
    memset(nextIv, 0, 16);
}

void MasterKeyManager::applyRekeying() {
    if (status != KeyManagerStatus::REKEYING) {
        printf("MasterKeyManager: unexpected call to applyRekeying\n");
        assert(false);
    }
    masterIndex = nextMasterIndex;
    memcpy(masterKey, nextMasterKey, 16);
    status = KeyManagerStatus::CONNECTED;

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

void MasterKeyManager::enqueueChallenge(StreamManagementElement sme) {
    unsigned char bytes[16] = {0};
    bytes[0] = sme.getDst();
    bytes[1] = sme.getSrcPort() | (sme.getDstPort() << 4);
    StreamParameters p = sme.getParams();
    memcpy(bytes + 2, &p, sizeof(StreamParameters));
    std::array<unsigned char, 16> ar;
    std::copy(std::begin(bytes), std::end(bytes), std::begin(ar));
    challenges.enqueue(sme.getSrc(), ar);
}

} //namespace mxnet
#endif //ifdef CRYPTO
