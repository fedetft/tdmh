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

#include "schedule_information.h"
#include "../bitwise_ops.h"
#include <algorithm>
#include <numeric>
#include <limits>

namespace mxnet {

void ScheduleTransition::serialize(unsigned char* pkt, unsigned short startBit) {
    BitwiseOps::bitwisePopulateBitArrTop<unsigned char>(
            pkt, sizeof(ScheduleTransitionPkt), reinterpret_cast<unsigned char*>(&content), sizeof(ScheduleTransitionPkt) + 1, startBit, getBitSize());
}

ScheduleTransition ScheduleTransition::deserialize(std::vector<unsigned char>& pkt, unsigned short startBit) {
    assert(pkt.size() * std::numeric_limits<unsigned char>::digits + startBit >= getMaxBitSize());
    return deserialize(pkt.data(), startBit);
}

ScheduleTransition ScheduleTransition::deserialize(unsigned char* pkt, unsigned short startBit) {
    ScheduleTransitionPkt content;
    BitwiseOps::bitwisePopulateBitArrTop<unsigned char>(
            reinterpret_cast<unsigned char*>(&content), sizeof(ScheduleTransitionPkt),
            pkt, sizeof(ScheduleTransitionPkt) + 1,
            startBit, getMaxBitSize()
    );
    return ScheduleTransition(content);
}

void ScheduleAddition::serialize(unsigned char* pkt, unsigned short startBit) {
    auto bitCount = 33;
    BitwiseOps::bitwisePopulateBitArrTop<unsigned char>(
            pkt, sizeof(ScheduleAdditionPkt) + 1, reinterpret_cast<unsigned char*>(&content), sizeof(ScheduleAdditionPkt) + 1, startBit, bitCount);
    for (auto st : transitions) {
        st.serialize(pkt, bitCount);
        bitCount += st.getBitSize();
    }
}

ScheduleAddition* ScheduleAddition::deserialize(std::vector<unsigned char>& pkt, unsigned short  startBit) {
    assert(pkt.size() * std::numeric_limits<unsigned char>::digits >= 33 + ScheduleTransition::getMaxBitSize());
    return deserialize(pkt.data(), startBit);
}

ScheduleAddition* ScheduleAddition::deserialize(unsigned char* pkt, unsigned short startBit) {
    auto bitCount = 33;
    ScheduleAdditionPkt content;
    BitwiseOps::bitwisePopulateBitArrTop<unsigned char>(
            reinterpret_cast<unsigned char*>(&content), sizeof(ScheduleAdditionPkt),
            pkt, sizeof(ScheduleAdditionPkt) + 1,
            startBit, startBit + bitCount
    );
    auto transitions = ScheduleTransition::deserializeMultiple(pkt, bitCount, content.hops);/*std::vector<ScheduleTransition>();//content.hops);
    for (int i = 0; i < content.hops; i++, bitCount += ScheduleTransition::getMaxBitSize()) {
        auto elem = ScheduleTransition::deserialize(pkt, bitCount);
        transitions.push_back(elem);
    }*/
    return new ScheduleAddition(content, std::move(transitions));
}

void ScheduleDeletion::serialize(unsigned char* pkt, unsigned short startBit) {
    auto idx = startBit / std::numeric_limits<unsigned char>::digits;
    auto msbShift = startBit % std::numeric_limits<unsigned char>::digits;
    auto lsbShift = std::numeric_limits<unsigned char>::digits - msbShift;
    if (msbShift == 0) {
        pkt[idx] = scheduleId;
        return;
    }
    pkt[idx] &= ~0 << lsbShift;
    pkt[idx] |= scheduleId >> msbShift;
    pkt[idx + 1] = scheduleId << lsbShift;
}

ScheduleDeletion* ScheduleDeletion::deserialize(std::vector<unsigned char>& pkt, unsigned short startBit) {
    auto idx = startBit / std::numeric_limits<unsigned char>::digits;
    auto msbShift = startBit % std::numeric_limits<unsigned char>::digits;
    auto lsbShift = std::numeric_limits<unsigned char>::digits - msbShift;
    return new ScheduleDeletion(pkt[idx] << msbShift | (msbShift == 0? 0 : pkt[idx + 1] >> lsbShift));
}

ScheduleDeletion* ScheduleDeletion::deserialize(unsigned char* pkt, unsigned short startBit) {
    auto idx = startBit / std::numeric_limits<unsigned char>::digits;
    auto msbShift = startBit % std::numeric_limits<unsigned char>::digits;
    auto lsbShift = std::numeric_limits<unsigned char>::digits - msbShift;
    return new ScheduleDeletion(pkt[idx] << msbShift | (msbShift == 0? 0 : pkt[idx + 1] >> lsbShift));
}

void ScheduleDelta::serialize(unsigned char* pkt, unsigned short startBit) {
    pkt[0] = additions.size();
    auto bitCount = std::numeric_limits<unsigned char>::digits;
    for (auto sa : additions) {
        sa->serialize(pkt, bitCount);
        bitCount += sa->getBitSize();
    }

    auto idx = bitCount / std::numeric_limits<unsigned char>::digits;
    auto msbShift = bitCount % std::numeric_limits<unsigned char>::digits;
    if (msbShift == 0) {
        for(auto d : deletions) {
            pkt[idx++] = *d;
        }
    } else {
        auto lsbShift = std::numeric_limits<unsigned char>::digits - msbShift;
        pkt[idx] &= ~0 << lsbShift;
        for(auto d : deletions) {
            pkt[idx++] |= *d >> msbShift;
            pkt[idx] = *d << lsbShift;
        }
    }
}

ScheduleDelta ScheduleDelta::deserialize(std::vector<unsigned char>& pkt) {
    assert(pkt.size() > 0);
    auto count = pkt[0];
    auto bitCount = std::numeric_limits<unsigned char>::digits;
    std::vector<ScheduleAddition*> additions(count);
    for (int i = 0; i < count; i++) {
        auto sa = ScheduleAddition::deserialize(pkt, bitCount);
        additions.push_back(sa);
        bitCount += sa->getBitSize();
    }
    auto delBits = pkt.size() * std::numeric_limits<unsigned char>::digits - bitCount;
    auto delCount = delBits / std::numeric_limits<unsigned char>::digits;
    auto index = bitCount / std::numeric_limits<unsigned char>::digits;
    auto msbShift = bitCount % std::numeric_limits<unsigned char>::digits;
    std::vector<ScheduleDeletion*> deletions(delCount);
    for (int i = 0; i < delCount; i++, index++) {
        deletions[i] = new ScheduleDeletion(
                pkt[index] << msbShift |
                (msbShift == 0? 0 : (pkt[index + 1] >> (std::numeric_limits<unsigned char>::digits - msbShift)))
        );
    }
    return ScheduleDelta(std::move(additions), std::move(deletions));
}

std::size_t ScheduleDelta::getSize() {
    return 1 + deletions.size() + std::accumulate(additions.begin(), additions.end(), 0, [](std::size_t acc, ScheduleAddition* val) {
        return acc + val->getSize();
    });
}

std::size_t ScheduleDelta::getBitSize() {
    return ((deletions.size() + 1) << 3) +
            std::accumulate(additions.begin(), additions.end(), 0, [](std::size_t acc, ScheduleAddition* val) {
        return acc + val->getBitSize();
    });
}

std::pair<ScheduleTransition*, ScheduleTransition*> ScheduleAddition::getTransitionForNode(unsigned char nodeId) {
    if (content.nodeId == nodeId) //sender
        return std::make_pair(nullptr, transitions.data());
    else if (transitions.rbegin()->getNodeId() == nodeId) //receiver
        return std::make_pair(transitions.data() + transitions.size() - 1, nullptr);
    else { //forwardee - forwarder
        std::vector<ScheduleTransition>::iterator t;
        for (t = transitions.begin(); t != transitions.end() && t->getNodeId() != nodeId; t++);
        return t != transitions.end()? std::make_pair(&t[0], &t[1]) : std::make_pair(nullptr, nullptr);
    }
}

std::vector<ScheduleTransition> ScheduleTransition::deserializeMultiple(unsigned char* pkt, unsigned short startBit, unsigned short count) {
    std::vector<ScheduleTransition> transitions;
    transitions.reserve(count);
    for (int i = 0; i < count; i++, startBit += getMaxBitSize()) {
        auto elem = ScheduleTransition::deserialize(pkt, startBit);
        transitions.push_back(elem);
    }
    return transitions;
}

} /* namespace mxnet */
