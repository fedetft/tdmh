/*
 * To change this license header, choose License Headers in Project Properties.
 * To change this template file, choose Tools | Templates
 * and open the template in the editor.
 */

/* 
 * File:   networkphase.h
 * Author: paolo
 *
 * Created on December 3, 2017, 10:40 PM
 */

#ifndef NETWORKPHASE_H
#define NETWORKPHASE_H

#include "mediumaccesscontroller.h"
#include <utility>

namespace miosix {
    class MACContext;
    class MACPhase {
    public:
        MACPhase() = delete;
        /**
         * Constructor to be used if the node is the first one sending in this phase
         * @param startTime
         * @param activityTime
         */
        MACPhase(long long startTime, long long activityTime) :
                MACPhase(startTime, activityTime, activityTime) {};
        /**
         * Constructor to be used if the node has to wait before acting in this phase
         * @param startTime
         * @param globalActivityTime
         * @param activityTime
         */
        MACPhase(long long startTime, long long globalActivityTime, long long activityTime) :
                globalStartTime(startTime),
                globalFirstActivityTime(globalActivityTime),
                localFirstActivityTime(activityTime) {};
        MACPhase(const MACPhase& orig) = delete;
        virtual ~MACPhase();
        virtual void execute(MACContext& ctx) = 0;
    protected:
        
        /**
         * Represents when the network theoretically switched to this phase.
         * Can be considered a worst case start time of execution.
         */
        const long long globalStartTime;
        /**
         * Represents when the node making the first action of the phase must do it.
         */
        const long long globalFirstActivityTime;
        /**
         * Represents when at least one node in the network starts this phase.
         */
        const long long localFirstActivityTime;
    private:

    };
}

#include "maccontext.h"

#endif /* NETWORKPHASE_H */

