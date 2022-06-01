
#pragma once

#include "led_bar.h"
#include "../util/packet.h"
#include "../mac_context.h"

static const int delayCompensationMessageSize=32;
using LedBarMessage=LedBar<delayCompensationMessageSize>;

namespace mxnet {

class DelayCompensationMessage
{
public:
    DelayCompensationMessage() {}

    DelayCompensationMessage(int delayVal) { setValue(delayVal); }

    void setValue(int delayVal)
    {
        LedBarMessage ledBar(delayVal);
        packet.clear();
        packet.put(ledBar.getPacket(), delayCompensationMessageSize);
        received = false;
    }

    void send(MACContext& ctx, long long sendTime)
    {
        packet.send(ctx, sendTime);
    }

    bool recv(MACContext& ctx, long long tExpected)
    {
        received = false;

        packet.clear();
        auto rcvResult = packet.recv(ctx, tExpected);
        if(rcvResult.error != miosix::RecvResult::ErrorCode::OK
            || rcvResult.size!=delayCompensationMessageSize)
            return false;

        timestamp = rcvResult.timestamp;
        LedBarMessage ledBar(&packet[0]);
        auto decoded = ledBar.decode();
        if(!decoded.second)
        {
            cumulatedPropagationDelay=-2;
            return false;
        }
        
        cumulatedPropagationDelay=decoded.first;
        received = true;
        return true;
    }
    
    int getValue() const { return cumulatedPropagationDelay; }

    long long getTimestamp() const { return timestamp; }

private:
    
    Packet packet;
    long long timestamp=0;
    int cumulatedPropagationDelay=0;
    bool received=false;
};

}
