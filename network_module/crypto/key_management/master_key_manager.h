#pragma once
#include "key_manager.h"

namespace mxnet {
class MasterKeyManager : public KeyManager {
public:
    MasterKeyManager() : KeyManager(KeyManagerStatus::CONNECTED) {
        loadMasterKey();
    }

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

private:
};

} //namespace mxnet
