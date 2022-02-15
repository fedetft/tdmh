/***************************************************************************
 *   Copyright (C) 2018-2022 by Federico Amedeo Izzo, Valeria Mazzola,     *
 *                              Luca Conterio                              *
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
#ifdef CRYPTO
#include "../uplink_phase/uplink_phase.h"
#include "timesync/timesync_downlink.h"
#endif
#include "../scheduler/schedule_element.h"
#include "../util/debug_settings.h"
#include "schedule_expansion.h"

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
        unsigned long long duration = MediumAccessController::receivingNodeWakeupAdvance
               + networkConfig.getMaxAdmittedRcvWindow() * 2
               + networkConfig.getMaxHops() * computeRebroadcastInterval(networkConfig);
#ifdef CRYPTO
        //NOTE: assuming maxControlPktSize is equal to maxDataPktSize == 125
        //NOTE: account for presence/absence of print_dbg. The time is actually
        //even lower if encryption is disabled
        if(ENABLE_CRYPTO_DOWNLINK_DBG)
            duration+=272000  // max packet authenticated encryption
                     +342000; // max packet authenticated decryption
        else
            duration+=179000  // max packet authenticated encryption
                     +197000; // max packet authenticated decryption
#endif
        return duration;
    }
    
    /**
     * Nothing to do
     */
    void resync() override {}

    /**
     * @return the number of the activation tile of a schedule that has been
     * sent and not yet applied. If no schedule has been sent, return zero
     */
    virtual unsigned int getNextActivationTile() = 0;

    /**
     * Used by Timesync to force rekeying in dynamic nodes when the Timesync packet
     * contains an activation tile and the node is currently unaware of the schedule that
     * was distributed. This can happen if the node has started resyncing after the
     * rekeying process had started, or if a schedule was missed entirely (zero schedule
     * packets received).
     */
    virtual void forceScheduleActivation(long long slotStart, unsigned int activationTile) {}

protected:
    ScheduleDownlinkPhase(MACContext& ctx) : MACPhase(ctx),
                                             rebroadcastInterval(computeRebroadcastInterval(ctx.getNetworkConfig())),
                                             panId(ctx.getNetworkConfig().getPanId()),
                                             scheduleExpander(ctx),
                                             streamMgr(ctx.getStreamManager()),
                                             dataPhase(ctx.getDataPhase()) {}

    void setNewSchedule(long long slotStart);
    void setSameSchedule(long long slotStart);

    /**
     * Apply the explicit schedule to the rest of the MAC
     * \param slotStart downlink slot start time
     */
    void applyNewSchedule(long long slotStart);
    void applySameSchedule(long long slotStart);

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
        INCOMPLETE_SCHEDULE,
        PROCESSING,
        AWAITING_ACTIVATION
    };
    
    const int rebroadcastInterval;
    static const int scheduleRepetitions = 4;
    
    ScheduleDownlinkStatus status = ScheduleDownlinkStatus::APPLIED_SCHEDULE;

    // Schedule header with information on schedule distribution
    ScheduleHeader header;
    // Copy of last computed/received schedule
    std::vector<ScheduleElement> schedule;

    // Number of schedule distribution slots need to distribute the schedule
    unsigned int sendingRounds;
    unsigned int currentSendingRound;

    // True if the schedule expansion process still has to be started
    bool needToStartExpansion = false;
    // True if the schedule expansion process still has to be performed
    bool needToPerformExpansion = false;

    // Constant value from NetworkConfiguration
    const unsigned short panId;

    // Object used to expand implict schedule and to split it over multiple downlink slots, if needed
    ScheduleExpander scheduleExpander;

    // Pointer to StreamManager, used to apply distributed schedule and info elements
    StreamManager* const streamMgr;

    // Pointer to DataPhase, used to apply distributed schedule
    DataPhase* const dataPhase;
};

}
