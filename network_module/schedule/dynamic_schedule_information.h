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

//#include "schedule_context.h"
#include <vector>
#include <queue>

namespace mxnet {

class DynamicScheduleContext;
class DynamicScheduleElement {
public:
    enum DynamicRole {
        SENDER,
        RECEIVER,
        FORWARDER,
        FORWARDEE
    };
    DynamicScheduleElement() = delete;
    virtual ~DynamicScheduleElement() {};
    unsigned short getId() { return id; }
    unsigned short getDataslot() { return dataslot; }
    virtual DynamicRole getRole() { return role; }
    virtual void run(DynamicScheduleContext& ctx) = 0;
    bool operator>(const DynamicScheduleElement& other) {
        return dataslot > other.dataslot;
    }
    bool operator<(const DynamicScheduleElement& other) {
        return !(*this > other);
    }
protected:
    DynamicScheduleElement(unsigned short id, unsigned short dataslot, DynamicRole role) : id(id), dataslot(dataslot), role(role) {};
    unsigned short id;
    unsigned short dataslot;
    DynamicRole role;
};
class SenderScheduleElement : public DynamicScheduleElement {
public:
    SenderScheduleElement(unsigned short id, unsigned short dataslot, unsigned char destination) :
        DynamicScheduleElement(id, dataslot, DynamicRole::SENDER), destination(destination) {};
    virtual ~SenderScheduleElement() {};
    SenderScheduleElement() = delete;
    virtual void run(DynamicScheduleContext& ctx);
    unsigned short getDestination() { return destination; };
protected:
    unsigned short destination;
};
class ReceiverScheduleElement : public DynamicScheduleElement {
public:
    ReceiverScheduleElement(unsigned short id, unsigned short dataslot, unsigned short source) :
        DynamicScheduleElement(id, dataslot, DynamicRole::RECEIVER), source(source) {};
    virtual ~ReceiverScheduleElement() {};
    ReceiverScheduleElement() = delete;
    virtual void run(DynamicScheduleContext& ctx);
    unsigned short getSource() { return source; };
protected:
    unsigned short source;
};
class ForwarderScheduleElement : public DynamicScheduleElement {
public:
    ForwarderScheduleElement(unsigned short id, unsigned short dataslot) : DynamicScheduleElement(id, dataslot, DynamicRole::FORWARDER) {};
    virtual ~ForwarderScheduleElement() {};
    ForwarderScheduleElement() = delete;
    virtual void run(DynamicScheduleContext& ctx);
};
class ForwardeeScheduleElement : public DynamicScheduleElement {
public:
    ForwardeeScheduleElement(unsigned short id, unsigned short dataslot, ForwarderScheduleElement* const next) :
        DynamicScheduleElement(id, dataslot, DynamicRole::FORWARDEE), next(next) {};
    virtual ~ForwardeeScheduleElement() {};
    ForwardeeScheduleElement() = delete;
    virtual void run(DynamicScheduleContext& ctx);
    ForwarderScheduleElement* const getNext() const { return next; }
protected:
    ForwarderScheduleElement* const next;
private:
};

} /* namespace mxnet */
