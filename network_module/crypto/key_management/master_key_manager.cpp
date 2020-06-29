#include <cassert>
#include <cstdio>
#include "master_key_manager.h"
#include "../aes.h"
#include "../crypto_utils.h"

#ifdef CRYPTO
namespace mxnet {

void MasterKeyManager::startRekeying() {
    if (status != KeyManagerStatus::CONNECTED) {
        printf("MasterKeyManager: unexpected call to startRekeying\n");
        assert(false);
    }

    if(ENABLE_CRYPTO_KEY_MGMT_DBG) {
        print_dbg("[KM] N=0 starting rekeying\n");
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

    if(ENABLE_CRYPTO_KEY_MGMT_DBG) {
        print_dbg("[KM] N=0 applying rekeying\n");
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

    // Manually re-deserialize bytes from SME
    bytes[0] = sme.getDst();
    bytes[1] = sme.getSrcPort() | (sme.getDstPort() << 4);
    StreamParameters p = sme.getParams();
    memcpy(bytes + 2, &p, sizeof(StreamParameters));

    // Put bytes in queue
    std::array<unsigned char, 16> ar;
    std::copy(std::begin(bytes), std::end(bytes), std::begin(ar));
    challenges.enqueue(sme.getSrc(), ar);
}

std::vector<InfoElement> MasterKeyManager::solveChallengesAndGetResponses() {
    std::vector<InfoElement> result;
    result.reserve(maxSolvesPerSlot);

    unsigned int solved = 0;
    while (!challenges.empty() && solved < maxSolvesPerSlot) {
        std::pair<unsigned char, std::array<unsigned char, 16>> chal;
        chal = challenges.dequeuePair();

        unsigned char key[16];
        unsigned char response[16];
        xorBytes(key, masterKey, challengeSecret, 16);
        Aes aes(key);
        aes.ecbEncrypt(response, chal.second.data());
        memset(key, 0, 16);

        /**
         * NOTE: because of how the responses are constructed into
         * InfoElements, we only actually send the first 6 bytes of response.
         * This is not the best, and it would be a good idea in the future to
         * implement responses differently and send more bytes.
         */
        InfoElement e = InfoElement::makeResponseInfoElement(chal.first, response);
        result.push_back(e);
        solved++;
    }

    return result;
}

} //namespace mxnet
#endif //ifdef CRYPTO
