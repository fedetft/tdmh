#pragma once
#include "key_manager.h"

namespace mxnet {

class DynamicKeyManager : public KeyManager {
public:
    DynamicKeyManager() : KeyManager(KeyManagerStatus::DISCONNECTED) {
        loadMasterKey();
    }

    AesGcm& getUplinkGCM() override;

    AesGcm& getTimesyncGCM() override;

    AesGcm& getScheduleDistributionGCM() override;

    /**
     * Compute next value for master key, without applying it yet.
     */
    void startRekeying() override;

    /**
     * Actually rotate the master key with the last precomuted value.
     */
    void applyRekeying() override;

    /** 
     * @return a pointer to the 16-byte buffer containing the current master key
     */
    void* getMasterKey() override;

    /** 
     * @return a pointer to the 16-byte buffer containing the next master key
     */
    void* getNextMasterKey() override;

    /**
     * @return the current value of the hash chain master index
     */
    unsigned int getMasterIndex() override;

    /**
     * Called upon resync.
     * Advance hash chain to derive new master key from last known master key.
     * \param newIndex current updated master key index. Cannot decrese in time.
     * @return true if newIndex has acceptable value (masterIndex did not decrease).
     *
     */
    bool attemptResync(unsigned int newIndex) override;

    void advanceResync() override;

    void rollbackResync() override;

    void commitResync() override;

    void attemptAdvance() override;

    void commitAdvance() override;

    void rollbackAdvance() override;

    void desync() override;

private:
};

} //namespace mxnet
