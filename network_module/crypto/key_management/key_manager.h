#pragma once
#include "../aes_gcm.h"
#include "../hash.h"

namespace mxnet {

enum KeyManagerStatus {
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
    KeyManager(KeyManagerStatus status) { this->status = status; }

    virtual ~KeyManager() {}

    KeyManagerStatus getStatus() { return status; }

    virtual AesGcm& getUplinkGCM() = 0;

    virtual AesGcm& getTimesyncGCM() = 0;

    virtual AesGcm& getScheduleDistributionGCM() = 0;

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

    /* TODO: change these functions to make masterIndex and rekeying persistent
     * across reboot */
    /**
     * Called only upon construction (at reboot).
     * Load last Master Key and Index from persistent memory (TODO)
     */
    void loadMasterKey() {
        masterIndex = 0;
    }

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

    KeyManagerStatus status = DISCONNECTED;

    unsigned int masterIndex;
    unsigned int nextMasterIndex;

    /**
     * Temporary values for master key and index.
     * These values are computed and used, but not yet committed. Committing
     * them consists in copying these values to masterKey and masterIndex.
     * Committing sets the context status to CONNECTED, meaning we have reached
     * a point where the master index advancement is completely verified.
     */
    unsigned char tempMasterKey[16];
    unsigned int tempMasterIndex;

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
    MPHash hash = MPHash(masterRotationIv);
};

} //namespace mxnet
