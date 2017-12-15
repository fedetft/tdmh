/*
 * To change this license header, choose License Headers in Project Properties.
 * To change this template file, choose Tools | Templates
 * and open the template in the editor.
 */

/* 
 * File:   roundtripphase.h
 * Author: paolo
 *
 * Created on December 3, 2017, 11:42 PM
 */

#ifndef ROUNDTRIPPHASE_H
#define ROUNDTRIPPHASE_H

#include "../mediumaccesscontroller.h"
#include "../macphase.h"
#include "interfaces-impl/transceiver.h"
#include <utility>

namespace miosix {
class RoundtripPhase : public MACPhase {
public:
    /**
     * First phase constructor
     * @param mac
     * @param startTime
     */
    RoundtripPhase(const MediumAccessController& mac, long long startTime, bool debug = true) :
            RoundtripPhase(startTime, mac.getPanId(), debug) {};
    RoundtripPhase(long long startTime, short panId, bool debug = true) : 
            MACPhase(startTime),
            transceiver(Transceiver::instance()),
            debug(debug),
            panId(panId) {};
    RoundtripPhase() = delete;
    RoundtripPhase(const RoundtripPhase& orig) = delete;
    virtual ~RoundtripPhase();
    
    static const int receiverWindow = 100000; //100us
    static const int senderDelay = 50000; //50us
    static const int replyDelay = 100000; //100us
    static const int askPacketSize = 7;
    static const int replyPacketSize = 125;
    
    /**
     * We are very limited with possible value to send through led bar, 
     * so we have to choose accurately the quantization level because it limits
     * the maximum value. We can send values from [0; replyPacketSize*2-1], so
     * with accuracy 15ns --> max value is 15*253=3795ns, or in meter:
     * 3,795e9s * 3e8m/s=1138.5m
     * Remember that each tick@48Mhz are 6.3m at speed of light.
     */
    static const int accuracy = 15;
protected:
    Transceiver& transceiver;
    bool debug;
    unsigned short panId;
    long long lastDelay;
    long long totalDelay;
    
    inline bool isRoundtripPacket(RecvResult& result, unsigned char *packet) {
        return result.error == RecvResult::OK && result.timestampValid
                && result.size == askPacketSize
                && packet[0] == 0x46 && packet[1] == 0x08
                && packet[3] == static_cast<unsigned char> (panId >> 8)
                && packet[4] == static_cast<unsigned char> (panId & 0xff)
                && packet[5] == 0xff && packet[6] == 0xfe;
    }
};
}

#endif /* ROUNDTRIPPHASE_H */

