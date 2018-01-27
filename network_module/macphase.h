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

#include <utility>

namespace miosix {
    class MACContext;
    class MACPhase {
    public:
        MACPhase() = delete;
        explicit MACPhase(long long startTime) : startTime(startTime), wakeupTime(startTime) {};
        MACPhase(long long startTime, long long wakeupTime) : startTime(startTime), wakeupTime(wakeupTime) {};
        MACPhase(const MACPhase& orig) = delete;
        virtual ~MACPhase();
        virtual void execute(MACContext& ctx) = 0;
    protected:
        /**
         * Represents when the whole network must start this phase.
         * Based on the node's role it can be the scheduling wakeup deadline (in the worst case)
         */
        long long startTime;
        
        /**
         * Represents when the node must start his turn in the network.
         * It is an upper bound for the completion of the activity in the network.
         */
        long long wakeupTime;
    private:

    };
}

#include "maccontext.h"

#endif /* NETWORKPHASE_H */

