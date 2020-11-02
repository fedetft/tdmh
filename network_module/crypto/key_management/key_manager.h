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

#pragma once
#include "../aes_ocb.h"
#include "../hash.h"
#include "../../stream/stream_manager.h"
#include "../../scheduler/schedule_element.h"

namespace mxnet {

enum class KeyManagerStatus {
    DISCONNECTED,
    MASTER_UNTRUSTED,
    REKEYING_UNTRUSTED,
    CONNECTED,
    REKEYING,
    ADVANCING
};

class KeyManager {
public:
    KeyManager() = delete;
    explicit KeyManager(StreamManager& streamMgr, KeyManagerStatus status) :
                                                        streamMgr(streamMgr) {
        this->status = status;
        loadMasterKey();
    }

    bool rekeyingInProgress() {
        return (status == KeyManagerStatus::REKEYING ||
                status == KeyManagerStatus::REKEYING_UNTRUSTED);
    }

    virtual ~KeyManager() {}

    KeyManagerStatus getStatus() { return status; }

    AesOcb& getUplinkOCB() { return uplinkOCB; }
    AesOcb& getTimesyncOCB() { return timesyncOCB; }
    AesOcb& getScheduleDistributionOCB() { return downlinkOCB; }

    /**
     * Compute next value for master key, without applying it yet.
     */
    virtual void startRekeying() = 0;

    /**
     * Actually rotate the master key with the last precomuted value.
     */
    virtual void applyRekeying() = 0;

    /** 
     * @return a pointer to the 16-byte buffer containing the current master key
     */
    virtual void* getMasterKey() = 0;

    /** 
     * @return a pointer to the 16-byte buffer containing the current master key
     */
    virtual void* getNextMasterKey() = 0;

    /**
     * @return the current value of the hash chain master index
     */
    virtual unsigned int getMasterIndex() = 0;

    /**
     * @return true if challenge timeout has run out and we should desync
     */
    virtual bool periodicUpdate() = 0;

    /* TODO: change these functions to make masterIndex and rekeying persistent
     * across reboot */
    /**
     * Called only upon construction (at reboot).
     * Load last Master Key and Index from persistent memory (TODO)
     */
    void loadMasterKey() {
        masterIndex = 0;

        // Initialize phase OCBs
        uplinkHash.digestBlock(uplinkKey, masterKey);
        downlinkHash.digestBlock(downlinkKey, masterKey);
        timesyncHash.digestBlock(timesyncKey, masterKey);
        uplinkOCB.rekey(uplinkKey);
        downlinkOCB.rekey(downlinkKey);
        timesyncOCB.rekey(timesyncKey);
    }

    /**
     * Used in master node to check for challenges to solve.
     * @return true if there are challenges that need solving
     */
    virtual bool challengesPresent() = 0;

    /**
     * Used in master node to collect challenges to solve
     */
    virtual void enqueueChallenge(StreamManagementElement sme) = 0;

    /**
     * Used in dynamic node to send challenge SME to master.
     */
    virtual void sendChallenge() = 0;

    /**
     * Used in master node to solve challenges. Called by schedule
     * distribution phase.
     */
    virtual std::vector<ResponseElement> solveChallengesAndGetResponses() = 0;

    /**
     * Used in dynamic node to verify a challenge response.
     * @return true if the response is valid and the master should be trusted.
     */
    virtual bool verifyResponse(ResponseElement info) = 0;

    /**
     * Called upon resync.
     * Advance hash chain to derive new master key from last known master key.
     * \param newIndex current updated master key index. Cannot decrese in time.
     * @return true if newIndex has acceptable value (masterIndex did not decrease).
     *
     */
    virtual bool attemptResync(unsigned int newIndex) { return false; }

    virtual void advanceResync() {}

    virtual void rollbackResync() {}

    virtual void commitResync() {}

    virtual void attemptAdvance() {}

    virtual void commitAdvance() {}

    virtual void rollbackAdvance() {}

    virtual void desync() {}

protected:

    StreamManager& streamMgr;

    KeyManagerStatus status;

    unsigned int masterIndex;
    unsigned int nextMasterIndex;

    /**
     * Value of the secret key used for challenge-respose authentication of
     * master node during resync. This value is SECRET and hardcoding it
     * is meant as a temporary solution.
     * Note that this secret is not used directly, instead it is used
     * combined in XOR with the current master key:
     *
     * Response = AES_Encrypt(Key = current master key XOR challengeSecret,
     *                        Data = Challenge)
     */
    unsigned char challengeSecret[16] = {
                0x51, 0x75, 0x65, 0x53, 0x74, 0x61, 0x20, 0x45,
                0x20, 0x62, 0x65, 0x4e, 0x7a, 0x69, 0x6e, 0x41
        };

    /**
     * Value of the first master key. This value is SECRET and hardcoding it
     * is meant as a temporary solution.
     */
    unsigned char masterKey[16] = {
                0x4d, 0x69, 0x6c, 0x6c, 0x6f, 0x63, 0x61, 0x74,
                0x4d, 0x69, 0x6c, 0x6c, 0x6f, 0x63, 0x61, 0x74
        };
    unsigned char nextMasterKey[16];
    /**
     * IV for the Miyaguchi-Preneel Hash used for rotating the master key.
     * Value for this constant is arbitrary and is NOT secret.
     */
    const unsigned char masterRotationIv[16] = {
                0x6d, 0x61, 0x73, 0x74, 0x65, 0x72, 0x49, 0x56,
                0x6d, 0x61, 0x73, 0x74, 0x65, 0x72, 0x49, 0x56
        };
    SingleBlockMPHash masterHash = SingleBlockMPHash(masterRotationIv);

    /*
     * Timesync: key, next key, current valid OCB, hash for
     * key derivation
     */
    unsigned char timesyncKey[16];
    unsigned char nextTimesyncKey[16];
    /**
     * IV for the Miyaguchi-Preneel Hash used for deriving timesync key from
     * master key.
     * Value for this constant is arbitrary and is NOT secret */
    const unsigned char timesyncDerivationIv[16] = {
                0x54, 0x69, 0x4d, 0x65, 0x53, 0x79, 0x4e, 0x63,
                0x74, 0x49, 0x6d, 0x45, 0x73, 0x59, 0x6e, 0x43
        };
    SingleBlockMPHash timesyncHash = SingleBlockMPHash(timesyncDerivationIv);
    AesOcb timesyncOCB;

    /*
     * ScheduleDistribution: key, next key, current valid OCB, hash for
     * key derivation
     */
    unsigned char downlinkKey[16];
    unsigned char nextDownlinkKey[16];
    /**
     * IV for the Miyaguchi-Preneel Hash used for deriving downlink key from
     * master key.
     * Value for this constant is arbitrary and is NOT secret.
     */
    const unsigned char downlinkDerivationIv[16] = {
                0x44, 0x6f, 0x57, 0x6e, 0x4c, 0x69, 0x4e, 0x6b,
                0x64, 0x4f, 0x77, 0x4e, 0x6c, 0x49, 0x6e, 0x4b
        };
    SingleBlockMPHash downlinkHash = SingleBlockMPHash(downlinkDerivationIv);
    AesOcb downlinkOCB;

    /*
     * Uplink: key, next key, current valid OCB, hash for
     * key derivation
     */
    unsigned char uplinkKey[16];
    unsigned char nextUplinkKey[16];
    /**
     * IV for the Miyaguchi-Preneel Hash used for deriving uplink key from
     * master key.
     * Value for this constant is arbitrary and is NOT secret */
    const unsigned char uplinkDerivationIv[16] = {
                0x55, 0x70, 0x4c, 0x69, 0x6e, 0x6b, 0x49, 0x76,
                0x55, 0x70, 0x4c, 0x69, 0x6e, 0x6b, 0x49, 0x76
        };
    SingleBlockMPHash uplinkHash = SingleBlockMPHash(uplinkDerivationIv);
    AesOcb uplinkOCB;
    /**
     * IV for the Miyaguchi-Preneel Hash used for deriving stream keys from
     * master key.
     * Value for this constant is arbitrary and is NOT secret.
     */
    const unsigned char streamKeyRotationIv[16] = {
                0x73, 0x54, 0x72, 0x45, 0x61, 0x4d, 0x6d, 0x41,
                0x6e, 0x61, 0x47, 0x65, 0x72, 0x49, 0x76, 0x30
        };
    SingleBlockMPHash firstBlockStreamHash = SingleBlockMPHash(streamKeyRotationIv);


};

} //namespace mxnet
