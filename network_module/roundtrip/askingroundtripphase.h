/*
 * To change this license header, choose License Headers in Project Properties.
 * To change this template file, choose Tools | Templates
 * and open the template in the editor.
 */

/* 
 * File:   AskingRoundtripPhase.h
 * Author: paolo
 *
 * Created on November 30, 2017, 10:12 PM
 */

#ifndef ASKINGROUNDTRIPPHASE_H
#define ASKINGROUNDTRIPPHASE_H

#include "../maccontext.h"
#include "roundtripphase.h"
#include <memory>
#include <array>

namespace miosix {
class AskingRoundtripPhase : public RoundtripPhase {
public:
    AskingRoundtripPhase(long long masterFloodingEndTime) :
            RoundtripPhase(masterFloodingEndTime) {};
    AskingRoundtripPhase() = delete;
    AskingRoundtripPhase(const AskingRoundtripPhase& orig) = delete;
    virtual ~AskingRoundtripPhase();
    void execute(MACContext& ctx) override;
protected:
    inline std::array<unsigned char, askPacketSize> getRoundtripAskPacket(int panId) {
        return {{
            0x46, //frame type 0b110 (reserved), intra pan
            0x08, //no source addressing, short destination addressing
            0x00, //seq no reused as glossy hop count, 0=root node, it has to contain the source hop
            static_cast<unsigned char>(panId>>8),
            static_cast<unsigned char>(panId & 0xff), //destination pan ID
            0xff, 0xff                                //destination addr (broadcast)
                }};
    }
private:

};
}

#endif /* ASKINGROUNDTRIPPHASE_H */

