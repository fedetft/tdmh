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
    //TODO add a way to use the syncStatus also with the master for having an optimized receiving window
    //maybe with a different class for the master node?
    long long timeoutTime = globalFirstActivityTime + receiverWindow;
    greenLed::high();
    //Transceiver configured with non strict timeout
    transceiver.configure(*ctx.getTransceiverConfig());
    transceiver.turnOn();
    
    unsigned char packet[askPacketSize];
#ifdef ENABLE_ROUNDTRIP_INFO_DBG
    printf("[RTT] Receiving until %lld\n", timeoutTime);
#endif /* ENABLE_ROUNDTRIP_INFO_DBG */
    RecvResult result;
    bool success = false;
    
    auto deepsleepDeadline = globalFirstActivityTime - MediumAccessController::receivingNodeWakeupAdvance;
    if(getTime() < deepsleepDeadline)
        pm.deepSleepUntil(deepsleepDeadline);
    for(; !(success || result.error == RecvResult::ErrorCode::TIMEOUT);
            success = isRoundtripPacket(result, packet, ctx.getMediumAccessController().getPanId(), ctx.getHop()))
    {
        try {
            result = transceiver.recv(packet, replyPacketSize, timeoutTime);
        } catch(std::exception& e) {
#ifdef ENABLE_RADIO_EXCEPTION_DBG
            printf("%s\n", e.what());
#endif /* ENABLE_RADIO_EXCEPTION_DBG */
        }
#ifdef ENABLE_PKT_INFO_DBG
        if(result.size){
            printf("[RTT] Received packet, error %d, size %d, timestampValid %d: ", result.error, result.size, result.timestampValid);
#ifdef ENABLE_PKT_DUMP_DBG
            memDump(packet, result.size);
#endif /* ENABLE_PKT_DUMP_DBG */
        } else printf("[RTT] No packet received, timeout reached\n");
#endif /* ENABLE_PKT_INFO_DBG */
    }
    
    if(success){
#ifdef ENABLE_ROUNDTRIP_INFO_DBG
        printf("[RTT] Replying ledbar packet\n");
#endif /* ENABLE_ROUNDTRIP_INFO_DBG */
        //TODO sto pacchetto non e` compatibile manco con se stesso, servono header di compatibilita`, indirizzo, etc etc
        LedBar<replyPacketSize> p;
        p.encode(7); //TODO: 7?! should put a significant cumulated RTT here.
        try {
            transceiver.sendAt(p.getPacket(), p.getPacketSize(), result.timestamp + replyDelay);
        } catch(std::exception& e) {
#ifdef ENABLE_RADIO_EXCEPTION_DBG
            puts(e.what());
#endif /* ENABLE_RADIO_EXCEPTION_DBG */
        }
#ifdef ENABLE_ROUNDTRIP_INFO_DBG
    } else {
        printf("[RTT] Roundtrip packet not received\n");
#endif /* ENABLE_ROUNDTRIP_INFO_DBG */
    }
    
    transceiver.turnOff();
    greenLed::low();
}
}

