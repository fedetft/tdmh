/*
 * To change this license header, choose License Headers in Project Properties.
 * To change this template file, choose Tools | Templates
 * and open the template in the editor.
 */

/* 
 * File:   ListeningRoundtripPhase.h
 * Author: paolo
 *
 * Created on November 30, 2017, 10:12 PM
 */

#ifndef LISTENINGROUNDTRIPPHASE_H
#define LISTENINGROUNDTRIPPHASE_H

#include "../maccontext.h"
#include "roundtripphase.h"
#include <memory>

namespace miosix{
class ListeningRoundtripPhase : public RoundtripPhase  {
public:
    explicit ListeningRoundtripPhase(long long masterFloodingEndTime) :
            RoundtripPhase(masterFloodingEndTime) {};
    ListeningRoundtripPhase() = delete;
    ListeningRoundtripPhase(const ListeningRoundtripPhase& orig) = delete;
    virtual ~ListeningRoundtripPhase();
    void execute(MACContext& ctx) override;
private:

};
}

#endif /* LISTENINGROUNDTRIPPHASE_H */

