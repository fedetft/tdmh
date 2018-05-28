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

/**
 * Represents a schedule element in the schedule of a dynamic node, characterized by a role,
 * a schedule id (to manage its deletion if needed) and the data slot at which it will operate.
 */
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
    unsigned short getId() const { return id; }
    unsigned short getDataslot() const { return dataslot; }
    virtual DynamicRole getRole() const { return role; }
    virtual void run(DynamicScheduleContext& ctx) = 0;
    bool operator>(const DynamicScheduleElement& other) const {
        return dataslot > other.dataslot;
    }
    bool operator<(const DynamicScheduleElement& other) const {
        return !(*this > other);
    }
protected:
    DynamicScheduleElement(unsigned short id, unsigned short dataslot, DynamicRole role) : id(id), dataslot(dataslot), role(role) {};
    unsigned short id;
    unsigned short dataslot;
    DynamicRole role;
};

/**
 * Represents a data slot in which the node is a sender for the given schedule,
 * i.e. the node will need to send a packet provided by the upper layers
 */
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

/**
 * Represents a data slot in which the node is a receiver in the provided schedule,
 * i.e. it will receive a packet that will be forwarded to the upper layers.
 */
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

/**
 * Represents a data slot in which the node is a sender, but it just needs to forward a packet
 * received in a previous data slot and temporarily stored in a buffer.
 */
class ForwarderScheduleElement : public DynamicScheduleElement {
public:
    ForwarderScheduleElement(unsigned short id, unsigned short dataslot) : DynamicScheduleElement(id, dataslot, DynamicRole::FORWARDER) {};
    virtual ~ForwarderScheduleElement() {};
    ForwarderScheduleElement() = delete;
    virtual void run(DynamicScheduleContext& ctx);
};

/**
 * Represents a data slot in which the node is a receiver, but it will not pass the packet to the upper layers,
 * but will instead store it in a buffer in order to send in it a data slot to follow, in order to travel to its
 * final receiver.
 */
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
