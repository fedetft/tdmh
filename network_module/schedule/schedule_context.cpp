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
#include "schedule_context.h"

#include "../debug_settings.h"
#include "../bitwise_ops.h"
#include <stdexcept>
#include "../mac_context.h"

using namespace std;

namespace mxnet {

std::pair<std::vector<ScheduleAddition*>, std::vector<unsigned char>> MasterScheduleContext::getScheduleToDistribute(unsigned short bytes) {
    static int i = 0;
    if(i++ == 15) {
        vector<std::tuple<unsigned short, unsigned short, unsigned short, unsigned short>>* vec;

        //pass from master
        //                                    TS                              SCHEDNUM                SRC                 DST
        /*schedule.insert(std::make_pair((unsigned short)0, std::make_tuple((unsigned short)0, (unsigned char)2, (unsigned char)0)));
        schedule.insert(std::make_pair((unsigned short)1, std::make_tuple((unsigned short)0, (unsigned char)0, (unsigned char)1)));
        schedule.insert(std::make_pair((unsigned short)2, std::make_tuple((unsigned short)0, (unsigned char)1, (unsigned char)3)));
        //                                          DELNUM           SCHEDNUM                TS                SRC                 DST
        vec = new vector<std::tuple<unsigned short, unsigned short, unsigned short, unsigned short>> {
                                              std::make_tuple((unsigned short)0, (unsigned short)0, (unsigned char)2, (unsigned char)0),
                                              std::make_tuple((unsigned short)0, (unsigned short)1, (unsigned char)0, (unsigned char)1),
                                              std::make_tuple((unsigned short)0, (unsigned short)2, (unsigned char)1, (unsigned char)3)
        };
        deltaToDistribute.push_back(std::make_pair((unsigned short) 0, vec));
        //forwarding queues
        queuesBySched[0] = new std::queue<std::vector<unsigned char>>();
        toForward[0] = queuesBySched[0];
        forwarded[1] = queuesBySched[0];*/

        //ping pong
        //                                    TS                              SCHEDNUM                SRC                 DST
        /*schedule.insert(std::make_pair((unsigned short)0, std::make_tuple((unsigned short)0, (unsigned char)0, (unsigned char)1)));
        schedule.insert(std::make_pair((unsigned short)1, std::make_tuple((unsigned short)0, (unsigned char)1, (unsigned char)3)));
        schedule.insert(std::make_pair((unsigned short)2, std::make_tuple((unsigned short)1, (unsigned char)3, (unsigned char)1)));
        schedule.insert(std::make_pair((unsigned short)3, std::make_tuple((unsigned short)1, (unsigned char)1, (unsigned char)0)));
        //                                          DELNUM           SCHEDNUM                TS                SRC                 DST
        vec = new vector<std::tuple<unsigned short, unsigned short, unsigned short, unsigned short>> {
                                              std::make_tuple((unsigned short)0, (unsigned short)0, (unsigned char)0, (unsigned char)1),
                                              std::make_tuple((unsigned short)0, (unsigned short)1, (unsigned char)1, (unsigned char)3)
        };
        deltaToDistribute.push_back(std::make_pair((unsigned short) 0, vec));
        vec = new vector<std::tuple<unsigned short, unsigned short, unsigned short, unsigned short>> {
                                              std::make_tuple((unsigned short)1, (unsigned short)2, (unsigned char)3, (unsigned char)1),
                                              std::make_tuple((unsigned short)1, (unsigned short)3, (unsigned char)1, (unsigned char)0)
        };
        deltaToDistribute.push_back(std::make_pair((unsigned short) 0, vec));*/

        //ping mirrored dt kite
        //                                    TS                              SCHEDNUM                SRC                 DST
        /*schedule.insert(std::make_pair((unsigned short)0, std::make_tuple((unsigned short)0, (unsigned char)0, (unsigned char)1)));
        schedule.insert(std::make_pair((unsigned short)1, std::make_tuple((unsigned short)0, (unsigned char)1, (unsigned char)2)));
        schedule.insert(std::make_pair((unsigned short)2, std::make_tuple((unsigned short)0, (unsigned char)2, (unsigned char)3)));
        schedule.insert(std::make_pair((unsigned short)4, std::make_tuple((unsigned short)0, (unsigned char)3, (unsigned char)5)));
        schedule.insert(std::make_pair((unsigned short)5, std::make_tuple((unsigned short)0, (unsigned char)5, (unsigned char)6)));
        schedule.insert(std::make_pair((unsigned short)6, std::make_tuple((unsigned short)0, (unsigned char)6, (unsigned char)7)));
        schedule.insert(std::make_pair((unsigned short)0, std::make_tuple((unsigned short)1, (unsigned char)7, (unsigned char)6)));
        schedule.insert(std::make_pair((unsigned short)1, std::make_tuple((unsigned short)1, (unsigned char)6, (unsigned char)5)));
        schedule.insert(std::make_pair((unsigned short)3, std::make_tuple((unsigned short)1, (unsigned char)5, (unsigned char)4)));
        schedule.insert(std::make_pair((unsigned short)4, std::make_tuple((unsigned short)1, (unsigned char)4, (unsigned char)2)));
        schedule.insert(std::make_pair((unsigned short)5, std::make_tuple((unsigned short)1, (unsigned char)2, (unsigned char)1)));
        schedule.insert(std::make_pair((unsigned short)6, std::make_tuple((unsigned short)1, (unsigned char)1, (unsigned char)0)));
        //                                          DELNUM           SCHEDNUM                TS                SRC                 DST
        vec = new vector<std::tuple<unsigned short, unsigned short, unsigned short, unsigned short>> {
                                              std::make_tuple((unsigned short)0, (unsigned short)0, (unsigned char)0, (unsigned char)1),
                                              std::make_tuple((unsigned short)0, (unsigned short)1, (unsigned char)1, (unsigned char)2),
                                              std::make_tuple((unsigned short)0, (unsigned short)2, (unsigned char)2, (unsigned char)3),
                                              std::make_tuple((unsigned short)0, (unsigned short)4, (unsigned char)3, (unsigned char)5),
                                              std::make_tuple((unsigned short)0, (unsigned short)5, (unsigned char)5, (unsigned char)6),
                                              std::make_tuple((unsigned short)0, (unsigned short)6, (unsigned char)6, (unsigned char)7)
        };
        deltaToDistribute.push_back(std::make_pair((unsigned short) 0, vec));
        vec = new vector<std::tuple<unsigned short, unsigned short, unsigned short, unsigned short>> {
                                              std::make_tuple((unsigned short)1, (unsigned short)0, (unsigned char)7, (unsigned char)6),
                                              std::make_tuple((unsigned short)1, (unsigned short)1, (unsigned char)6, (unsigned char)5),
                                              std::make_tuple((unsigned short)1, (unsigned short)3, (unsigned char)5, (unsigned char)4),
                                              std::make_tuple((unsigned short)1, (unsigned short)4, (unsigned char)4, (unsigned char)2),
                                              std::make_tuple((unsigned short)1, (unsigned short)5, (unsigned char)2, (unsigned char)1),
                                              std::make_tuple((unsigned short)1, (unsigned short)6, (unsigned char)1, (unsigned char)0)
        };
        deltaToDistribute.push_back(std::make_pair((unsigned short) 0, vec));*/
    }
    std::vector<ScheduleAddition*> retval1;
    std::vector<unsigned char> retval2;
    auto bitsLimit = bytes * std::numeric_limits<unsigned char>::digits;
    //insert until i encounter the first one exceeding
    for (bool exceeds = false; bitsLimit > 0 && !exceeds;) {
        auto* val = deltaToDistribute.front();
        auto nextSize = val->getBitSize();
        if (nextSize > bitsLimit)
            exceeds = true;
        else {
            deltaToDistribute.pop_front();
            if (val->getDeltaType() == ScheduleDeltaElement::ADDITION)
                retval1.push_back(static_cast<ScheduleAddition*>(val));
            else
                retval2.push_back(val->getScheduleId());
            bitsLimit -= nextSize;
        }
    }
    //then start browsing the queue for trying to fill all the available space greedily
    for (auto it = deltaToDistribute.begin(); it != deltaToDistribute.end() && bitsLimit >= std::numeric_limits<unsigned char>::digits;) {
        if ((*it)->getBitSize() > bitsLimit) it++;
        else {
            if ((*it)->getDeltaType() == ScheduleDeltaElement::ADDITION)
                retval1.push_back(static_cast<ScheduleAddition*>(*it));
            else
                retval2.push_back((*it)->getScheduleId());
            it = deltaToDistribute.erase(it);
        }
    }
    return std::make_pair(retval1, retval2);
}

void DynamicScheduleContext::parseSchedule(std::vector<unsigned char>& pkt) {
    assert(pkt.size() > 0);
    auto addCount = pkt[0];
    auto myId = ctx.getNetworkId();
    auto bitsRead = std::numeric_limits<unsigned char>::digits;
    for (int i = 0; i < addCount; i++) {
        auto sa = ScheduleAddition::deserialize(pkt.data(), bitsRead);
        bitsRead += sa->getBitSize();
        auto transitions = sa->getTransitions();
        auto t = sa->getTransitionForNode(myId);
        if (t.first == nullptr)
            addSchedule(new SenderScheduleElement(sa->getScheduleId(), t.second->getDataslot(), transitions.rbegin()->getNodeId()));
        else if (t.second == nullptr)
            addSchedule(new ReceiverScheduleElement(sa->getScheduleId(), t.second->getDataslot(), sa->getNodeId()));
        else {
            auto* fwd = new ForwarderScheduleElement(sa->getScheduleId(), t.second->getDataslot());
            addSchedule(fwd);
            addSchedule(new ForwardeeScheduleElement(sa->getScheduleId(), t.first->getDataslot(), fwd));
        }
    }
    for (int i = (pkt.size() * std::numeric_limits<unsigned char>::digits - bitsRead) / pkt.size() * std::numeric_limits<unsigned char>::digits; i >= 0; i--) {
        auto* sd = ScheduleDeletion::deserialize(pkt.data(), bitsRead);
        deleteSchedule(sd->getScheduleId());
        bitsRead += std::numeric_limits<unsigned char>::digits;
        delete sd;
    }
}

} /* namespace mxnet */
