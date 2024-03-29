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

    uplinkOCB.rekey(nextUplinkKey);
    downlinkOCB.rekey(nextDownlinkKey);
    timesyncOCB.rekey(nextTimesyncKey);
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

std::vector<ResponseElement> MasterKeyManager::solveChallengesAndGetResponses() {
    std::vector<ResponseElement> result;
    result.reserve(maxSolvesPerSlot);
    unsigned char key[16];
    unsigned char response[16];
    xorBytes(key, masterKey, challengeSecret, 16);
    Aes aes(key);

    unsigned int solved = 0;
    while (!challenges.empty() && solved < maxSolvesPerSlot) {
        std::pair<unsigned char, std::array<unsigned char, 16>> chal;
        chal = challenges.dequeuePair();

        if(ENABLE_CRYPTO_KEY_MGMT_DBG) {
            print_dbg("[KM] Solving challenge for node %d\n", chal.first);
        }

        aes.ecbEncrypt(response, chal.second.data());

        /**
         * NOTE: because of how the responses are constructed into
         * ResponseElements, we only actually send the first 8 bytes of response.
         * This is not the best, and it would be a good idea in the future to
         * implement responses differently and send more bytes.
         */
        ResponseElement e = ResponseElement(chal.first, response);
        result.push_back(e);
        solved++;
    }

    memset(key, 0, 16);
    memset(response, 0, 16);
    return result;
}

} //namespace mxnet
#endif //ifdef CRYPTO
