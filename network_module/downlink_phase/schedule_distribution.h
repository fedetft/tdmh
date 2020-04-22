/***************************************************************************
 *   Copyright (C) 2018-2019 by Federico Amedeo Izzo                       *
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

#include "../mac_phase.h"
#include "../mac_context.h"
#include "../scheduler/schedule_element.h"
#include "../util/debug_settings.h"

#define FLOOD_TYPE 1

/**
 * Represents the phase in which the schedule is distributed to the entire network,
 * in order to reach all the nodes so they can playback it when it becomes active.
 */

namespace mxnet {

class ScheduleDownlinkPhase : public MACPhase
{
public:
    ScheduleDownlinkPhase() = delete;
    ScheduleDownlinkPhase(const ScheduleDownlinkPhase& orig) = delete;

    /**
     * \return the duration in nanoseconds of a downlink slot
     */
    static unsigned long long getDuration(const NetworkConfiguration& networkConfig)
    {
        return   MediumAccessController::receivingNodeWakeupAdvance
               + networkConfig.getMaxAdmittedRcvWindow() * 2
               + networkConfig.getMaxHops() * computeRebroadcastInterval(networkConfig);
    }
    
    /**
     * Nothing to do
     */
    void resync() override {}

#ifdef CRYPTO
    void precomputeRekeying() {
        hash.reset();
        hash.digestBlock(newDownlinkKey, ctx.getMasterKey());
    }

    void applyRekeying() { 
        memcpy(downlinkKey, newDownlinkKey, 16);
    }
#endif

protected:
    ScheduleDownlinkPhase(MACContext& ctx) : MACPhase(ctx),
                                             rebroadcastInterval(computeRebroadcastInterval(ctx.getNetworkConfig())),
                                             panId(ctx.getNetworkConfig().getPanId()),
                                             streamMgr(ctx.getStreamManager()),
                                             dataPhase(ctx.getDataPhase()) {
#ifdef CRYPTO
        /* Initialize the uplink key and GCM from current master key */
        hash.reset();
        hash.digestBlock(newDownlinkKey, ctx.getMasterKey());
        memcpy(downlinkKey, newDownlinkKey, 16);
        gcm.rekey(downlinkKey);
#endif
    }

    /**
     * Convert the explicit schedule to an implicit one
     * \param nodeID node for which the explicit schedule is needed
     * \return the explicit schedule
     */
    std::vector<ExplicitScheduleElement> expandSchedule(unsigned char nodeID);

    /**
     * Apply the explicit schedule to the rest of the MAC
     * \param slotStart downlink slot start time
     */
    void applySchedule(long long slotStart);
    
#ifndef _MIOSIX
    /**
     * Print the implicit schedule and explicit schedule of all nodes
     * Very verbose.
     */
    void printCompleteSchedule();
    
    /**
     * Print the implict schedule, just like the schedule computation
     */
    void printSchedule(unsigned char nodeID);
    
    /**
     * Print the explicit schedule of a given node
     */
    void printExplicitSchedule(unsigned char nodeID, bool printHeader,
                               const std::vector<ExplicitScheduleElement>& expSchedule);
#else
    void printCompleteSchedule() {}
    void printExplicitSchedule(unsigned char nodeID, bool printHeader,
                               const std::vector<ExplicitScheduleElement>& expSchedule) {}
#endif

    static int computeRebroadcastInterval(const NetworkConfiguration& cfg)
    {
        const int computationTime=244000;
        const int txTime=(MediumAccessController::maxControlPktSize+8)*32000;
#if FLOOD_TYPE==0
        return txTime+computationTime+MediumAccessController::sendingNodeWakeupAdvance;
#elif FLOOD_TYPE==1
        auto a=MediumAccessController::sendingNodeWakeupAdvance;
        auto b=MediumAccessController::receivingNodeWakeupAdvance+cfg.getMaxAdmittedRcvWindow();
        return txTime+computationTime+std::max(a,b);
#endif
    }

    enum class ScheduleDownlinkStatus : unsigned char
    {
        APPLIED_SCHEDULE,
        SENDING_SCHEDULE,
        AWAITING_ACTIVATION,
        INCOMPLETE_SCHEDULE
    };
    
    const int rebroadcastInterval;
    static const int scheduleRepetitions = 4;
    
    ScheduleDownlinkStatus status = ScheduleDownlinkStatus::APPLIED_SCHEDULE;

    // Schedule header with information on schedule distribution
    ScheduleHeader header;
    // Copy of last computed/received schedule
    std::vector<ScheduleElement> schedule;

    /* Structure used to keep count of redundancy groups of streams that this
     * node is scheduled to forward to others. This info will be moved to 
     * dataphase at the end of schedule explicitation and used to correctly
     * clear the buffers.
     * For each stream being forwarded, the second number of the pair is set
     * to the number of transmissions that this node is assigned. The first 
     * number of the pair will be set to zero and used as counter by
     * the dataphase */
    std::map<StreamId, std::pair<unsigned char, unsigned char>> forwardedStreamCtr;

    // Constant value from NetworkConfiguration
    const unsigned short panId;

    // Pointer to StreamManager, used to apply distributed schedule and info elements
    StreamManager* const streamMgr;

    // Pointer to DataPhase, used to apply distributed schedule
    DataPhase* const dataPhase;

#ifdef CRYPTO
    unsigned char downlinkKey[16];
    unsigned char newDownlinkKey[16];
    /* Value for this constant is arbitrary and is NOT secret */
    const unsigned char downlinkDerivationIv[16] = {
                0x44, 0x6f, 0x57, 0x6e, 0x4c, 0x69, 0x4e, 0x6b,
                0x64, 0x4f, 0x77, 0x4e, 0x6c, 0x49, 0x6e, 0x4b
        };
    MPHash hash = MPHash(downlinkDerivationIv);
    AesGcm gcm = AesGcm(downlinkKey);
#endif
};

}
