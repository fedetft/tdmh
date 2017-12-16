/*
 * To change this license header, choose License Headers in Project Properties.
 * To change this template file, choose Tools | Templates
 * and open the template in the editor.
 */

/* 
 * File:   NetworkRound.h
 * Author: paolo
 *
 * Created on November 30, 2017, 9:57 PM
 */

#ifndef NETWORKROUND_H
#define NETWORKROUND_H

#include "../maccontext.h"
#include <memory>

namespace miosix {
    class FloodingPhase;
    class RoundtripPhase;
    class MACRound {
    public:
        MACRound() {};
        MACRound(FloodingPhase* flooding, RoundtripPhase* roundtrip) :
                flooding(flooding), roundtrip(roundtrip) {};
        MACRound(const MACRound& orig) = delete;
        virtual ~MACRound();

        void setFloodingPhase(FloodingPhase* fp);
        inline FloodingPhase* getFloodingPhase() { return flooding; }

        void setRoundTripPhase(RoundtripPhase* rtp);
        inline RoundtripPhase* getRoundtripPhase() { return roundtrip; }

        virtual void run(MACContext& ctx);
        static const long long roundDuration = 10000000000LL; //10s
    protected:
        FloodingPhase* flooding = nullptr;
        RoundtripPhase* roundtrip = nullptr;

    };
}

#endif /* NETWORKROUND_H */

