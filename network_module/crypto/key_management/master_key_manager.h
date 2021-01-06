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
#include "../../stream/stream_manager.h"
#include "../../util/updatable_queue.h"
#include <array>

namespace mxnet {
class MasterKeyManager : public KeyManager {
public:
    MasterKeyManager(StreamManager& streamMgr) :
        KeyManager(streamMgr, KeyManagerStatus::CONNECTED) {
            /*
             * TEST CODE
             * for measuring computation time for many hash rounds.
             * Current results: ~21 us per hash
             *                  leaving time for about 470k hashes in the 10 seconds
             *                  between two timesyncs
             *
            unsigned how_many = 10000;
            for (unsigned i=0; i<how_many; i++) {
                masterHash.digestBlock(masterKey, masterKey);
            }
            masterIndex = how_many;

            uplinkHash.digestBlock(uplinkKey, masterKey);
            downlinkHash.digestBlock(downlinkKey, masterKey);
            timesyncHash.digestBlock(timesyncKey, masterKey);
            uplinkOCB.rekey(uplinkKey);
            downlinkOCB.rekey(downlinkKey);
            timesyncOCB.rekey(timesyncKey);
            */

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
    //NOTE: must be less than SchedulePacket::getPacketCapacity(), see how
    //MasterScheduleDownlinkPhase::sendInfoPkt() calls solveChallengesAndGetResponses()
    const unsigned int maxSolvesPerSlot = 5;
};

} //namespace mxnet
