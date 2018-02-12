//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU Lesser General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
// 
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU Lesser General Public License for more details.
// 
// You should have received a copy of the GNU Lesser General Public License
// along with this program.  If not, see http://www.gnu.org/licenses/.
// 

#include "Node.h"
#include "Transceiver.h"
#include <iostream>

Define_Module(Node);

void Node::activity()
{
    Transceiver t(*this);
    t.configure(TransceiverConfiguration(2450, 0, false, false));
    unsigned char pkt[127];
    auto result = t.recv(pkt, sizeof(pkt), 10000000000);
    EV_INFO << "ERROR: " << result.error << " AT " << result.timestamp << endl;
    if(result.error == RecvResult::OK) {
        EV_INFO << "[ ";
        for (int i = 0; i < result.size; i++)
            EV_INFO << std::hex << (unsigned int) pkt[i] << " ";
        EV_INFO << std::dec << "]"<< endl;
        t.sendAt(pkt, result.size, result.timestamp + 500000, "sync");
    } else {
        EV_INFO << "ERROR : " << result.error << endl;
    }
    for(;;)
    {
        long long sleepTime = result.timestamp + 5000000 - simTime().inUnit(SIMTIME_NS);
        EV_INFO << "Sleeping for " << sleepTime << endl;
        waitAndDeletePackets(SimTime(sleepTime, SIMTIME_NS));
        //trying with a 5 ms window
        long long timeout = result.timestamp + 15000000;
        EV_INFO << "Will listen for packets from " << simTime().inUnit(SIMTIME_NS) << " to " << timeout << endl;
        result = t.recv(pkt, sizeof(pkt), timeout);
        EV_INFO << "ERROR: " << result.error << " AT " << result.timestamp << endl;
        if(result.error == RecvResult::OK) {
            EV_INFO << "[ ";
            for (int i = 0; i < result.size; i++)
                EV_INFO << std::hex << (unsigned int) pkt[i] << " ";
            EV_INFO << std::dec << "]"<< endl;
            t.sendAt(pkt, result.size, result.timestamp + 500000, "sync");
        }
    }
}
