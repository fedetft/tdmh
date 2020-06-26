#pragma once
#include "key_manager.h"
#include "../../stream/stream_manager.h"
#include "../../util/updatable_queue.h"
#include <array>

namespace mxnet {
class MasterKeyManager : public KeyManager {
public:
    MasterKeyManager(StreamManager& streamMgr) :
        KeyManager(streamMgr, KeyManagerStatus::CONNECTED) {}

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
     * @return true if challenge timeout has run out and we should desync
     * This never happens in master node.
     */
    bool periodicUpdate() override { return false; }

    /**
     * Used in master node to collect challenges to solve
     */
    void enqueueChallenge(StreamManagementElement sme) override;

private:
    /**
     * Queue mapping the nodeId of the node that sent the challenge to the challenge bytes
     */
    UpdatableQueue<unsigned char, std::array<unsigned char, 16>> challenges;
};

} //namespace mxnet
