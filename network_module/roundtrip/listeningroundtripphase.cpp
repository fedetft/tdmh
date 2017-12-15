/*
 * To change this license header, choose License Headers in Project Properties.
 * To change this template file, choose Tools | Templates
 * and open the template in the editor.
 */

/* 
 * File:   ListeningRoundtripPhase.cpp
 * Author: paolo
 * 
 * Created on November 30, 2017, 10:12 PM
 */

#include "listeningroundtripphase.h"
#include "led_bar.h"
#include "../macround/macround.h"
#include <stdio.h>

namespace miosix{
ListeningRoundtripPhase::~ListeningRoundtripPhase() {
}

void ListeningRoundtripPhase::execute(MACContext& ctx) {
    //Transceiver configured with non strict timeout
    long long timeoutTime = startTime + receiverWindow;
    greenLed::high();
    transceiver.configure(*ctx.getTransceiverConfig());
    transceiver.turnOn();
    
    unsigned char packet[replyPacketSize];
    bool isTimeout = false;
    long long now=0;
    printf("[RTT] Receiving until %lld\n", timeoutTime);
    RecvResult result;
    for(bool success = false; !(success || isTimeout);)
    {
        try {
            result = transceiver.recv(packet, replyPacketSize, timeoutTime);
        } catch(std::exception& e) {
            if(debug) printf("%s\n", e.what());
        }
        now = getTime();
        if(debug) {
            if(result.size){
                printf("[RTT] Received packet, error %d, size %d, timestampValid %d: ", result.error, result.size, result.timestampValid);
                memDump(packet, result.size);
            } else printf("[RTT] No packet received, timeout reached\n");
        }
        if(isRoundtripPacket(result, packet) && (packet[2] == ctx.getHop() + 1))
        {
            success = true;
        }
        if(now >= timeoutTime)
        {
            isTimeout = true;
        }
    }
    
    if(!isTimeout && result.error == RecvResult::OK && result.timestampValid){
        if(debug) printf("[RTT] Replying ledbar packet\n");
        LedBar<125> p;
        p.encode(7); //TODO: 7?! should check what's received, increment the led bar and filter it with a LPF
        try {
            transceiver.sendAt(p.getPacket(), p.getPacketSize(), result.timestamp + replyDelay);
        } catch(std::exception& e) {
            if(debug) puts(e.what());
        }
    } else {
        if(debug) printf("[RTT] Roundtrip packet not received\n");
    }
    
    transceiver.turnOff();
    greenLed::low();
}
}

