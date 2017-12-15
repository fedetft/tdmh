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
        MACPhase();
        MACPhase(long long startTime) : startTime(startTime) {};
        MACPhase(const MACPhase& orig);
        virtual ~MACPhase();
        virtual void execute(MACContext& ctx) = 0;
    protected:
        long long startTime; 
    private:

    };
}

#include "maccontext.h"

#endif /* NETWORKPHASE_H */

