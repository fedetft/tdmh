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

#include "stream_management_context.h"
#include <algorithm>

namespace mxnet {

void MasterStreamManagementContext::receive(std::vector<StreamManagementElement*>& smes) {
    for (auto* sme: smes) {
        open(sme);
        delete sme;
    }
}

void MasterStreamManagementContext::open(StreamManagementElement* sme) {
    auto it = std::find_if(opened.begin(), opened.end(), [sme](StreamManagementElement* el){ return sme->getId() == el->getId(); });
    if (it == opened.end())
        opened.push_back(sme);
    else {
        delete *it;
        it = opened.erase(it);
        opened.insert(it, sme);
    }
}

void DynamicStreamManagementContext::receive(std::vector<StreamManagementElement*>& smes) {
    for (auto* sme : smes) {
        auto id = sme->getId();
        if (queue.hasKey(id)) {
            delete queue.getByKey(id);
            queue.update(id, sme);
        } else
            queue.enqueue(id, sme);
        delete sme;
    }
}

void DynamicStreamManagementContext::open(StreamManagementElement* sme) {
    pending.push_back(sme);
    queue.enqueue(sme->getId(), sme);
}

void DynamicStreamManagementContext::opened(StreamManagementElement* sme) {
    auto it = std::find(pending.begin(), pending.end(), sme);
    if (it == pending.end()) return;
    pending.erase(it);
    auto it2 = queue.getByKey(sme->getId());
    queue.removeElement(sme->getId());
    delete it2;
}

std::vector<StreamManagementElement*> DynamicStreamManagementContext::dequeue(std::size_t count) {
    count = std::min(count, queue.size());
    std::vector<StreamManagementElement*> retval(count);
    for (unsigned i = 0; i < count; i++) {
        auto&& val = queue.dequeue();
        retval[i] = val;
        auto it = std::find(pending.begin(), pending.end(), val);
        if (it != pending.end())
            queue.enqueue((*it)->getId(), *it);
    }
    return retval;
}

} /* namespace mxnet */
