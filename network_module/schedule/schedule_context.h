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

#pragma once

#include "schedule_information.h"
#include "master_schedule_information.h"
#include <map>
#include <vector>
#include <deque>
#include <set>

namespace mxnet {

class MACContext;
class ScheduleContext {
public:
    ScheduleContext(MACContext& ctx) : ctx(ctx) {};
    virtual ~ScheduleContext() {}
    std::set<DynamicScheduleElement*>::iterator getFirstSchedule() { return nodeSchedule.begin(); };
    std::queue<std::vector<unsigned char>>* getQueueForId(unsigned short id) {
        std::map<unsigned short, std::queue<std::vector<unsigned char>>>::iterator retval = forwardQueues.find(id);
        if (retval == forwardQueues.end()) return nullptr;
        return &(retval->second);
    }
protected:
    MACContext& ctx;
    std::set<DynamicScheduleElement*> nodeSchedule;
    std::map<unsigned short, std::queue<std::vector<unsigned char>>> forwardQueues;
};

class MasterScheduleContext : public ScheduleContext {
public:
    MasterScheduleContext(MACContext& ctx) : ScheduleContext(ctx) {};
    virtual ~MasterScheduleContext() {};
    std::pair<std::vector<ScheduleAddition*>, std::vector<unsigned char>> getScheduleToDistribute(unsigned short bytes);
protected:
    std::set<MasterScheduleElement> currentSchedule;
    std::deque<ScheduleDeltaElement*> deltaToDistribute;
};

class DynamicScheduleContext : public ScheduleContext {
public:
    DynamicScheduleContext(MACContext& ctx) : ScheduleContext(ctx) {};
    virtual ~DynamicScheduleContext() {};
    void addSchedule(DynamicScheduleElement* element) {
        nodeSchedule.insert(element);
        scheduleById[element->getId()] = element;
        if (element->getRole() == DynamicScheduleElement::FORWARDEE) {
            forwardQueues[element->getId()] = std::queue<std::vector<unsigned char>>();
        }
        //TODO notify of opened stream in reception/sending, if needed.
    }
    void deleteSchedule(unsigned char id) {
        auto elem = scheduleById.find(id);
        if (elem == scheduleById.end()) return;
        if (elem->second->getRole() == DynamicScheduleElement::FORWARDEE) {
            nodeSchedule.erase(dynamic_cast<ForwardeeScheduleElement*>(elem->second)->getNext());
            forwardQueues.erase(elem->second->getId());
        }
        nodeSchedule.erase(elem->second);
        scheduleById.erase(id);
        //TODO notify successful stream close, if needed
    }
    void parseSchedule(std::vector<unsigned char>& pkt);
protected:
    /**
     * Not containing forwarder elements, only sender, receiver and forwardee.
     * Forwarder will be accessible as `forwardee.next`.
     * Useful for deleting a schedule, being able to obtain timestamps and access the nodeSchedule for their removal.
     */
    std::map<unsigned short, DynamicScheduleElement*> scheduleById;
};


} /* namespace mxnet */
