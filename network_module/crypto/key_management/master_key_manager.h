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
     * @return true if there are challenges that need solving
     */
    bool challengesPresent() override { return !challenges.empty(); }

    /**
     * Used in master node to collect challenges to solve
     * \param sme a SME of type CHALLENGE to enqueue and solve later.
     */
    void enqueueChallenge(StreamManagementElement sme) override;

    /*
     * Used in dynamic node. Do nothing.
     */
    void sendChallenge() override {}

    /**
     * Used in master node to solve challenges. Called by schedule
     * distribution phase.
     * Solve a maximum of maxSolvesPerSlot challenges present in queue.
     * @return a vector of ResponseElement to be sent in this slot.
     */
    std::vector<ResponseElement> solveChallengesAndGetResponses() override;

    /**
     * Used in dynamic node to verify a challenge response.
     * Should not be called in master node. Do nothing.
     * @return true if the response is valid and the master should be trusted.
     */
    bool verifyResponse(ResponseElement response) override { return false; }

private:
    /**
     * Queue mapping the nodeId of the node that sent the challenge to the challenge bytes
     */
    UpdatableQueue<unsigned char, std::array<unsigned char, 16>> challenges;

    //TODO: tweak this value
    const unsigned int maxSolvesPerSlot = 5;
};

} //namespace mxnet
