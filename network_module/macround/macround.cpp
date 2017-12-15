/*
 * To change this license header, choose License Headers in Project Properties.
 * To change this template file, choose Tools | Templates
 * and open the template in the editor.
 */

/* 
 * File:   NetworkRound.cpp
 * Author: paolo
 * 
 * Created on November 30, 2017, 9:57 PM
 */

#include "macround.h"
#include "../flooding/floodingphase.h"
#include "../roundtrip/roundtripphase.h"

namespace miosix {
    MACRound::~MACRound() {
    }
    
    void MACRound::run(MACContext& ctx) {
        if (flooding != nullptr) flooding->execute(ctx);
        if (roundtrip != nullptr) roundtrip->execute(ctx);
    }

}

