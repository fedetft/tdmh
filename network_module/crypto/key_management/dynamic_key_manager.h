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

#include "key_manager.h"

#ifdef CRYPTO
#include "../../stream/stream_manager.h"

namespace mxnet {

class DynamicKeyManager : public KeyManager {
public:
    DynamicKeyManager(StreamManager& streamMgr, const NetworkConfiguration& config) :
        KeyManager(streamMgr, KeyManagerStatus::DISCONNECTED),
        myId(config.getStaticNetworkId()),
        sendChallenges(config.getDoMasterChallengeAuthentication()),
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
    bool challengesPresent() override {
        assert(false);
        return false;
    }
    void enqueueChallenge(StreamManagementElement sme) override {}

    /**
     * Enqueue a challenge SME in the stream manager, and save it here
     */
    void sendChallenge() override; 

    std::vector<ResponseElement> solveChallengesAndGetResponses() override {
        assert(false);
        std::vector<ResponseElement> result;
        return result;
    }

    /**
     * Used in dynamic node to verify a challenge response.
     * @return true if the response is valid and the master should be trusted.
     */
    bool verifyResponse(ResponseElement response) override;

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
     * Resend same challenge
     */
    void resendChallenge();

    /* Maximum index advancement when attempting resync */
    const unsigned maxIndexDelta = 470000;
    const unsigned char myId;
    const bool sendChallenges;
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
    /**
     * Resend twice before resync:
     * sizing is done so that the resync timeout happens after the challenge has been
     * sent three times, and the two timeouts are not reached too close to each other
     */
    const unsigned int chalResendTimeout = (challengeTimeout/5)*2;
    unsigned int chalResendCtr = 0;
    unsigned int chalTimeoutCtr = 0;

    bool forceDesync = false;

    /**
     * Last challenge sent to master. This value is only meaningful if we are
     * in state MASTER_UNTRUSTED, waiting to check response.
     */
    unsigned char chal[16] = {0};

};

} //namespace mxnet
#endif //ifdef CRYPTO
