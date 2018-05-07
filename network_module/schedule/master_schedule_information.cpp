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

#include "master_schedule_information.h"
#include <algorithm>

namespace mxnet {

ScheduleTransition MasterScheduleTransition::toDeltaTransition() {
    return ScheduleTransition(dataslot, destination);
}

MasterScheduleTransition MasterScheduleElement::getActionForDs(unsigned short dataslot) {
    return *std::find_if(transitions.begin(), transitions.end(), [dataslot](MasterScheduleTransition t) {
        return t.getDataslot() == dataslot;
    });
}

unsigned char MasterScheduleElement::toDeletion() {
    return id;
}

ScheduleAddition MasterScheduleElement::toAddition() {
    std::vector<ScheduleTransition> t;
    t.reserve(transitions.size());
    std::transform(transitions.begin(), transitions.end(), std::back_inserter(t), [](MasterScheduleTransition mst){
        return mst.toDeltaTransition();
    });
    return ScheduleAddition(id, transitions.size(), transitions[0].getSource(), std::move(t));
}

std::pair<MasterScheduleTransition*, MasterScheduleTransition*> MasterScheduleElement::getActionsForNodeId(unsigned char nodeId) {
    if (getSource() == nodeId)
        return std::make_pair(nullptr, &(*transitions.begin()));
    if (getDestination() == nodeId)
        return std::make_pair(&(*transitions.rbegin()), nullptr);
    auto rcv = std::find_if(transitions.begin(), transitions.end(), [nodeId](MasterScheduleTransition t){
        return t.getDestination() == nodeId;
    });
    if (rcv == transitions.end())
        return std::make_pair(nullptr, nullptr);
    return std::make_pair(&(*rcv), &(rcv[1]));
}

std::pair<DynamicScheduleElement*, DynamicScheduleElement*> MasterScheduleElement::getActionsForMasterNode() {
    if (getSource() == 0)
        return std::make_pair(nullptr, new SenderScheduleElement(id, transitions.begin()->getDataslot(), getDestination()));
    if (getDestination() == 0)
        return std::make_pair(new ReceiverScheduleElement(id, transitions.rbegin()->getDataslot(), getSource()), nullptr);
    auto rcv = std::find_if(transitions.begin(), transitions.end(), [](MasterScheduleTransition t){
        return t.getDestination() == 0;
    });
    if (rcv == transitions.end())
        return std::make_pair(nullptr, nullptr);
    auto fwd = new ForwarderScheduleElement(id, (rcv + 1)->getDataslot());
    return std::make_pair(new ForwardeeScheduleElement(id, rcv->getDataslot(), fwd), fwd);
}

} /* namespace mxnet */
