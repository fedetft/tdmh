/***************************************************************************
 *   Copyright (C)  2017 by Terraneo Federico, Polidori Paolo              *
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
 *   and link it with other works to produce a wor k based on this file,    *
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

#include <stdio.h>
#include "../debug_settings.h"
#include "schedule_distribution.h"
#include <algorithm>

using namespace miosix;

namespace mxnet {

ScheduleDownlinkPhase::ScheduleDownlinkPhase(MACContext& ctx) :
                MACPhase(ctx),
                networkConfig(ctx.getNetworkConfig()) {
    switch (ctx.getNetworkId()) {
    case 0: {
        //STREAM 0
        nodeSchedule.insert(new ReceiverScheduleElement(10, 1, 3));
        nodeSchedule.insert(new ReceiverScheduleElement(11, 2, 3));
        nodeSchedule.insert(new ReceiverScheduleElement(100, 15, 3));
        nodeSchedule.insert(new ReceiverScheduleElement(101, 17, 3));
        //STREAM 1
        nodeSchedule.insert(new ReceiverScheduleElement(20, 3, 6));
        nodeSchedule.insert(new ReceiverScheduleElement(21, 6, 6));
        //STREAM 2
        nodeSchedule.insert(new ReceiverScheduleElement(30, 8, 4));
        nodeSchedule.insert(new ReceiverScheduleElement(31, 10, 4));
        break;
    }
    case 1: {
        //STREAM 0
        auto* a = new ForwarderScheduleElement(10, 1);
        auto* b = new ForwarderScheduleElement(100, 16);
        nodeSchedule.insert(new ForwardeeScheduleElement(10, 0, a));
        nodeSchedule.insert(a);
        nodeSchedule.insert(new ForwardeeScheduleElement(100, 15, b));
        nodeSchedule.insert(b);
        forwardQueues[10] = std::queue<std::vector<unsigned char>>();
        forwardQueues[100] = std::queue<std::vector<unsigned char>>();
        break;
    }
    case 2: {
        //STREAM 1
        auto* a = new ForwarderScheduleElement(21, 4);
        nodeSchedule.insert(new ForwardeeScheduleElement(21, 1, a));
        nodeSchedule.insert(a);
        forwardQueues[21] = std::queue<std::vector<unsigned char>>();
        break;
    }
    case 3: {
        //STREAM 0
        nodeSchedule.insert(new SenderScheduleElement(10, 0, 0));
        nodeSchedule.insert(new SenderScheduleElement(11, 2, 0));
        nodeSchedule.insert(new SenderScheduleElement(100, 15, 0));
        nodeSchedule.insert(new SenderScheduleElement(101, 17, 0));
        break;
    }
    case 4: {
        //STREAM 1
        auto* a = new ForwarderScheduleElement(21, 5);
        nodeSchedule.insert(new ForwardeeScheduleElement(21, 4, a));
        nodeSchedule.insert(a);
        forwardQueues[21] = std::queue<std::vector<unsigned char>>();
        //STREAM 2
        nodeSchedule.insert(new SenderScheduleElement(30, 7, 0));
        nodeSchedule.insert(new SenderScheduleElement(31, 9, 0));
        break;
    }
    case 5: {
        //STREAM 1
        auto* a = new ForwarderScheduleElement(21, 6);
        nodeSchedule.insert(new ForwardeeScheduleElement(21, 5, a));
        nodeSchedule.insert(a);
        forwardQueues[21] = std::queue<std::vector<unsigned char>>();
        //STREAM 2
        auto* b = new ForwarderScheduleElement(31, 10);
        nodeSchedule.insert(new ForwardeeScheduleElement(31, 9, b));
        nodeSchedule.insert(b);
        forwardQueues[31] = std::queue<std::vector<unsigned char>>();
        break;
    }
    case 6: {
        //STREAM 1
        nodeSchedule.insert(new SenderScheduleElement(20, 0, 0));
        nodeSchedule.insert(new SenderScheduleElement(21, 1, 0));
        break;
    }
    case 7: {
        //STREAM 1
        auto* a = new ForwarderScheduleElement(20, 3);
        nodeSchedule.insert(new ForwardeeScheduleElement(20, 2, a));
        nodeSchedule.insert(a);
        forwardQueues[20] = std::queue<std::vector<unsigned char>>();
        //STREAM 2
        auto* b = new ForwarderScheduleElement(30, 8);
        nodeSchedule.insert(new ForwardeeScheduleElement(30, 7, b));
        nodeSchedule.insert(b);
        forwardQueues[30] = std::queue<std::vector<unsigned char>>();
        break;
    }
    case 8: {
        //STREAM 1
        auto* a = new ForwarderScheduleElement(20, 2);
        nodeSchedule.insert(new ForwardeeScheduleElement(20, 0, a));
        nodeSchedule.insert(a);
        forwardQueues[20] = std::queue<std::vector<unsigned char>>();
        break;
    }
    }
}

std::set<DynamicScheduleElement*>::iterator ScheduleDownlinkPhase::getScheduleForOrBeforeSlot(unsigned short slot) {
    return std::lower_bound(nodeSchedule.begin(), nodeSchedule.end(), slot, [](DynamicScheduleElement* elem, unsigned short slot) {
        return elem->getDataslot() == slot;
    });
}

}

