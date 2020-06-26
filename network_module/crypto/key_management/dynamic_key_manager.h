#pragma once
#include "key_manager.h"
#include "../../stream/stream_manager.h"

namespace mxnet {

class DynamicKeyManager : public KeyManager {
public:
    DynamicKeyManager(StreamManager& streamMgr, const NetworkConfiguration& config) :
        KeyManager(streamMgr, KeyManagerStatus::DISCONNECTED),
        myId(config.getStaticNetworkId()),
        doChallengeResponse(config.getDoMasterChallengeAuthentication()),
        challengeTimeout(config.getMasterChallengeAuthenticationTimeout())
        {
            //TODO: IMPORTANT! Use a secure (P)RNG! (to be implemented)
            srand(myId);
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

    /**
     * @return true if challenge timeout has run out and we should desync
     */
    bool periodicUpdate() override;

    /**
     * Only used in master. Do nothing.
     */
    void enqueueChallenge(StreamManagementElement sme) override {}

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

    /**
     * Enqueue a challenge SME in the stream manager, and save it here
     */
    void sendChallenge();

    const unsigned char myId;
    /**
     * Temporary values for master key and index.
     * These values are computed and used, but not yet committed. Committing
     * them consists in copying these values to masterKey and masterIndex.
     * Committing sets the context status to CONNECTED, meaning we have reached
     * a point where the master index advancement is completely verified.
     */
    unsigned char tempMasterKey[16];
    unsigned int tempMasterIndex;

    const bool doChallengeResponse;
    const unsigned int challengeTimeout;
    unsigned int challengeCtr = 0;

    /**
     * Last challenge sent to master. This value is only meaningful if we are
     * in state MASTER_UNTRUSTED, waiting to check response.
     */
    unsigned char chal[16] = {0};

};

} //namespace mxnet
