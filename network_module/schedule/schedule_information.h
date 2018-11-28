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

#include "../serializable_message.h"
#include "../bitwise_ops.h"
#include <vector>

namespace mxnet {

/**
 * Represents a single arc in the schedule, as a data slot number and a destination node.
 */
class ScheduleTransition : public BitSerializableMessage {
public:
    ScheduleTransition(unsigned short dataslot, unsigned char nodeId) : content({dataslot, nodeId}) {};
    virtual ~ScheduleTransition() {};

    /**
     * @return the dataslot in which the packet transits the network
     */
    unsigned short getDataslot() { return content.dataslot; }

    /**
     * @return the network id of the receiver node
     */
    unsigned char getNodeId() { return content.nodeId; }
    void serializeImpl(unsigned char* pkt, unsigned short startBit) const override;
    static ScheduleTransition deserialize(std::vector<unsigned char>& pkt, unsigned short startBit);
    static ScheduleTransition deserialize(unsigned char* pkt, unsigned short startBit);

    /**
     * Deserializes multiple ScheduleTransition from serialized data
     */
    static std::vector<ScheduleTransition> deserializeMultiple(unsigned char* pkt, unsigned short startBit, unsigned short count);
    std::size_t size() const override { return sizeof(ScheduleTransitionPkt); }
    std::size_t bitSize() const override { return getMaxBitSize(); };
    static std::size_t getMaxSize() { return sizeof(ScheduleTransitionPkt); }
    static std::size_t getMaxBitSize() { return 22; }
private:
    struct ScheduleTransitionPkt {
        unsigned short dataslot : 14;
        unsigned char nodeId : 8;
    } __attribute__((packed));
    ScheduleTransition() {};
    ScheduleTransition(ScheduleTransitionPkt content) : content(content) {};
    ScheduleTransitionPkt content;
};

/**
 * Represents an element in the schedule downlink delta message,
 * representing such message as a list of these
 */
class ScheduleDeltaElement : public BitSerializableMessage {
public:
    enum DeltaType {
        ADDITION, DELETION
    };
    ScheduleDeltaElement() = delete;
    ScheduleDeltaElement(DeltaType type) : type(type) {};
    virtual ~ScheduleDeltaElement() {};
    virtual unsigned short getScheduleId() = 0;
    DeltaType getDeltaType() { return type; }
protected:
    DeltaType type;
};

/**
 * Represents a schedule that has been added to the list of schedules
 */
class ScheduleAddition : public ScheduleDeltaElement {
public:
    ScheduleAddition() = delete;
    ScheduleAddition(unsigned short scheduleId, unsigned short hops, unsigned char nodeId, std::vector<ScheduleTransition>&& transitions) :
        ScheduleDeltaElement(ADDITION),
        content({scheduleId, hops, nodeId}),
        transitions(transitions) {};
    virtual ~ScheduleAddition() {};

    /**
     * @return the id of the schedule
     */
    unsigned short getScheduleId() override { return content.scheduleId; }

    /**
     * @return the number of hops the schedule travels
     */
    unsigned short getHops() { return content.hops; }

    /**
     * @return the address of the sender
     */
    unsigned char getNodeId() { return content.nodeId; }

    /**
     * @return the transitions included in the schedule
     */
    std::vector<ScheduleTransition>& getTransitions() { return transitions; }
    void serializeImpl(unsigned char* pkt, unsigned short startBit) const override;
    static ScheduleAddition* deserialize(std::vector<unsigned char>& pkt, unsigned short startBit);
    static ScheduleAddition* deserialize(unsigned char* pkt, unsigned short startBit);
    std::size_t size() const override {
        return sizeof(ScheduleAdditionPkt) + transitions.size() * ScheduleTransition::getMaxSize();
    }
    std::size_t bitSize() const override { return 33 + transitions.size() * ScheduleTransition::getMaxBitSize(); };

    /**
     * @return the transitions involving the node. The first is if the node
     */
    std::pair<ScheduleTransition*, ScheduleTransition*> getTransitionForNode(unsigned char nodeId);
protected:
    struct ScheduleAdditionPkt {
        unsigned short scheduleId : 16;
        unsigned short hops : 9;
        unsigned char nodeId : 8;
    } __attribute__((packed));
    ScheduleAddition(ScheduleAdditionPkt content, std::vector<ScheduleTransition>&& transitions) :
        ScheduleDeltaElement(ADDITION), content(content), transitions(transitions) {};
    ScheduleAdditionPkt content;
    std::vector<ScheduleTransition> transitions;
};

/**
 * Represents a schedule that has been deleted to the list of schedules
 */
class ScheduleDeletion : public ScheduleDeltaElement {
public:
    ScheduleDeletion() = delete;
    ScheduleDeletion(unsigned short scheduleId) : ScheduleDeltaElement(DELETION), scheduleId(scheduleId) {};
    virtual ~ScheduleDeletion() {};
    unsigned short getScheduleId() override { return scheduleId; }
    void serializeImpl(unsigned char* pkt, unsigned short startBit) const override;
    static ScheduleDeletion* deserialize(std::vector<unsigned char>& pkt, unsigned short startBit);
    static ScheduleDeletion* deserialize(unsigned char* pkt, unsigned short startBit);
    std::size_t size() const override { return 1; }
    std::size_t bitSize() const override { return std::numeric_limits<unsigned char>::digits; };
    operator unsigned char() const {return scheduleId; }
protected:
    unsigned short scheduleId;
};


/**
 * Represents a schedule delta as a a list of additions followed by a list of deletions
 */
class ScheduleDelta : public BitSerializableMessage {
public:
    ScheduleDelta() = delete;
    ScheduleDelta(std::vector<ScheduleAddition*>&& additions, std::vector<ScheduleDeletion*>&& deletions) :
        additions(additions), deletions(deletions) {};
    virtual ~ScheduleDelta() {};
    std::vector<ScheduleAddition*>& getAdditions() { return additions; }
    std::vector<ScheduleDeletion*>& getDeletions() { return deletions; }
    void serializeImpl(unsigned char* pkt, unsigned short startBit) const override;
    static ScheduleDelta deserialize(std::vector<unsigned char>& pkt);
    std::size_t size() const override;
    std::size_t bitSize() const override;
private:
    std::vector<ScheduleAddition*> additions;
    std::vector<ScheduleDeletion*> deletions;
};

} /* namespace mxnet */
