/***************************************************************************
 *   Copyright (C)  2018 by Polidori Paolo                                 *
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 *   This program is distributed in the hope that it will be useful,       *
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of        *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
 *   GNU General Public License for more details.                          *
 *                                                                         *
 *   As a special exception, if other files instantiate templates or use   *
 *   macros or inline functions from this file, or you compile this file   *
 *   and link it with other works to produce a work based on this file,    *
 *   this file does not by itself cause the resulting work to be covered   *
 *   by the GNU General Public License. However the source code for this   *
 *   file must still be made available in accordance with the GNU General  *
 *   Public License. This exception does not invalidate any other reasons  *
 *   why a work based on this file might be covered by the GNU General     *
 *   Public License.                                                       *
 *                                                                         *
 *   You should have received a copy of the GNU General Public License     *
 *   along with this program; if not, see <http://www.gnu.org/licenses/>   *
 ***************************************************************************/

#include "dataphase.h"

using namespace std;
using namespace miosix;

namespace mxnet {

    void DataPhase::execute(long long slotStart) {
        print_dbg("[D] GFAT=%llu\n", slotStart);
        transceiver.configure(ctx.getTransceiverConfig());
        transceiver.turnOn();
        if ((*nextSched)->getDataslot() != dataSlot) return;
        /*
        std::tuple<bool, ScheduleContext::Role, std::queue<std::vector<unsigned char>>*> slotJob = s->executeTimeslot(i);
        std::queue<std::vector<unsigned char>>* q;
        if (!std::get<0>(slotJob)) continue;
        vector<unsigned char> pkt(125);
        auto expectedTs = globalFirstActivityTime + ((long long) i) * (transmissionInterval + packetArrivalAndProcessingTime);
        auto wuTo = status == nullptr?
                std::make_pair(expectedTs - MediumAccessController::maxAdmittableResyncReceivingWindow - MediumAccessController::receivingNodeWakeupAdvance, expectedTs + MediumAccessController::maxAdmittableResyncReceivingWindow) :
                status->getWakeupAndTimeout(expectedTs);
        auto now = getTime();
        switch (std::get<1>(slotJob)) {
        case ScheduleContext::Role::SEND:
            str = (std::string("Hello, I am ") + std::to_string(ctx.getNetworkId()));
            strcpy((char *)pkt.data(), str.c_str());
            transceiver.sendAt(pkt.data(), sizeof(str) * sizeof(str[0]) / sizeof(unsigned char), expectedTs);
            print_dbg("[D] Sent packet with size %d at %llu\n", sizeof(str), expectedTs);
            break;
        case ScheduleContext::Role::RCV:
            if (now >= expectedTs - (status == nullptr? MediumAccessController::maxAdmittableResyncReceivingWindow : status->receiverWindow))
                print_dbg("[D] start late\n");
            if (now < wuTo.first)
                pm.deepSleepUntil(wuTo.first);
            r = transceiver.recv(pkt.data(), 125, wuTo.second);
            print_dbg("[D] Received packet with size %d at %llu:\n%s\n", r.size, r.timestamp, std::string((char*) pkt.data(), r.size).c_str());
            break;
        case ScheduleContext::Role::FWE:
            if (now >= expectedTs - (status == nullptr? MediumAccessController::maxAdmittableResyncReceivingWindow : status->receiverWindow))
                print_dbg("[D] start late\n");
            if (now < wuTo.first)
                pm.deepSleepUntil(wuTo.first);
            r = transceiver.recv(pkt.data(), 125, wuTo.second);
            pkt.resize(r.size);
            q = std::get<2>(slotJob);
            if (r.error == RecvResult::OK)
                q->push(pkt);
            print_dbg("[D] Received forwarded with size %d packet at %llu with error %d\n", r.size, r.timestamp, r.error);
            break;
        case ScheduleContext::Role::FWD:
            q = std::get<2>(slotJob);
            if (q->empty()) {
                print_dbg("[D] No packets to forward at %llu\n", expectedTs);
                break;
            }
            pkt = q->front();
            transceiver.sendAt(pkt.data(), pkt.size(), expectedTs);
            print_dbg("[D] Forwarded packet with size %d at %llu\n", pkt.size(), expectedTs);
            break;
        }*/
        transceiver.turnOff();
    }



}

