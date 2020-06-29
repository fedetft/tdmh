#pragma once
#include "../aes_gcm.h"
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

    virtual ~KeyManager() {}

    KeyManagerStatus getStatus() { return status; }

    AesGcm& getUplinkGCM() { return uplinkGCM; }
    AesGcm& getTimesyncGCM() { return timesyncGCM; }
    AesGcm& getScheduleDistributionGCM() { return downlinkGCM; }

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

        // Initialize phase GCMs
        uplinkHash.digestBlock(uplinkKey, masterKey);
        downlinkHash.digestBlock(downlinkKey, masterKey);
        timesyncHash.digestBlock(timesyncKey, masterKey);
        uplinkGCM.rekey(uplinkKey);
        downlinkGCM.rekey(downlinkKey);
        timesyncGCM.rekey(timesyncKey);
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
     * Used in master node to solve challenges. Called by schedule
     * distribution phase.
     */
    virtual std::vector<InfoElement> solveChallengesAndGetResponses() = 0;

    /**
     * Used in dynamic node to verify a challenge response.
     * \return true if the response is valid and the master should be trusted.
     */
    virtual bool verifyResponse(InfoElement info) = 0;

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
     * Timesync: key, next key, current valid GCM, hash for
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
    AesGcm timesyncGCM;

    /*
     * ScheduleDistribution: key, next key, current valid GCM, hash for
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
    AesGcm downlinkGCM;

    /*
     * Uplink: key, next key, current valid GCM, hash for
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
    AesGcm uplinkGCM;
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
