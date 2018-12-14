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
 *   and link it with other works to  produce a work based on this file,    *
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

void MasterStreamManagementContext::receive(const std::vector<StreamManagementElement>& smes) {
    for (auto sme: smes) {
        open(sme);
    }
}

void MasterStreamManagementContext::open(StreamManagementElement sme) {
    auto it = std::find_if(opened.begin(), opened.end(), [sme](StreamManagementElement el){return sme.getKey() == el.getKey(); });
    if (it == opened.end()) {
        opened.push_back(sme);
        added_flag = true;
    }
    else {
        it = opened.erase(it);
        opened.insert(it, sme);
    }
    modified_flag = true;
}

StreamManagementElement MasterStreamManagementContext::getStream(int index) {
    return opened.at(index);
}

int MasterStreamManagementContext::getStreamNumber() {
    return opened.size();
}

std::vector<StreamManagementElement> MasterStreamManagementContext::getStreams() {
    auto streams = opened;
    return streams;
}

std::vector<StreamManagementElement> MasterStreamManagementContext::getEstablishedStreams() {
    std::vector<StreamManagementElement> established (opened.size());
    auto it = std::copy_if (opened.begin(), opened.end(), established.begin(),
                            [](StreamManagementElement el){return el.getStatus() == StreamStatus::ESTABLISHED; });
    established.resize(std::distance(established.begin(),it));  // shrink container to new size
    return established;
}

std::vector<StreamManagementElement> MasterStreamManagementContext::getNewStreams() {
    std::vector<StreamManagementElement> new_streams (opened.size());
    auto it = std::copy_if (opened.begin(), opened.end(), new_streams.begin(),
                            [](StreamManagementElement el){return el.getStatus() == StreamStatus::NEW; });
    new_streams.resize(std::distance(new_streams.begin(),it));  // shrink container to new size
    return new_streams;
}

bool MasterStreamManagementContext::hasNewStreams() const {
    auto it = std::find_if(opened.begin(), opened.end(),
                            [](StreamManagementElement el){return el.getStatus() == StreamStatus::NEW; });
    if (it == opened.end())
        return false;
    else
        return true;
}


void MasterStreamManagementContext::setStreamStatus(unsigned int key, StreamStatus status) {
    auto it = std::find_if(opened.begin(), opened.end(), [key](StreamManagementElement el){return key == el.getKey(); });
    if (it != opened.end()) {
        it->setStatus(status);
    }
    //TODO: Manage the case in which the stream we want to modify is not present
}

void DynamicStreamManagementContext::receive(const std::vector<StreamManagementElement>& smes) {
    for (auto sme : smes) {
        auto key = sme.getKey();
        if (queue.hasKey(key)) {
            //Overwrite saved sme with updated one
            queue.getByKey(key) = sme;
        } else
            queue.enqueue(key, sme);
    }
}

void DynamicStreamManagementContext::open(StreamManagementElement sme) {
    pending.push_back(sme);
    queue.enqueue(sme.getKey(), sme);
}

void DynamicStreamManagementContext::opened(StreamManagementElement sme) {
    auto it = std::find(pending.begin(), pending.end(), sme);
    if (it == pending.end()) return;
    pending.erase(it);
    auto it2 = queue.getByKey(sme.getKey());
    queue.removeElement(sme.getKey());
}

std::vector<StreamManagementElement> DynamicStreamManagementContext::dequeue(std::size_t count) {
    count = std::min(count, queue.size());
    std::vector<StreamManagementElement> result;
    result.reserve(count);
    for (unsigned i = 0; i < count; i++) {
        auto&& val = queue.dequeue();
        result[i] = val;
        auto it = std::find(pending.begin(), pending.end(), val);
        if (it != pending.end())
            queue.enqueue(it->getKey(), *it);
    }
    return result;
}

} /* namespace mxnet */
