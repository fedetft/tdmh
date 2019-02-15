/***************************************************************************
 *   Copyright (C)  2018 by Federico Amedeo Izzo                           *
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

#include "stream_manager.h"
#include <algorithm>

namespace mxnet {

void StreamCollection::setStreamStatus(StreamId id, StreamStatus status) {
    collection[id].setStatus(status);
}

std::vector<StreamInfo> StreamCollection::getStreams() {
    std::vector<StreamInfo> result;
    for(auto stream : collection)
        result.push_back(stream.second);
    return result;
}

bool isSchedulable(std::pair<StreamId,StreamInfo> stream) {
    StreamStatus status = stream.second.getStatus();
    return (status == StreamStatus::ACCEPTED);
}

bool StreamCollection::hasSchedulableStreams() {
    return std::count_if(collection.begin(), collection.end(), isSchedulable);
}

std::vector<StreamInfo> StreamCollection::getStreamsWithStatus(StreamStatus s) {
    std::vector<StreamInfo> result;
    for (auto& stream: collection) {
        if(stream.second.getStatus() == s)
            result.push_back(stream.second);
    }
    return result;
}

void StreamManager::registerStream(StreamInfo info, Stream* stream) {
    // Mutex lock to access the Stream map from the application thread.
#ifdef _MIOSIX
    miosix::Lock<miosix::Mutex> lck(stream_mutex);
#else
    std::unique_lock<std::mutex> lck(stream_mutex);
#endif
    // Register Stream class pointer in stream map
    clientMap[info.getStreamId()] = stream;
    // Register Stream information and status
    streamMap[info.getStreamId()] = StreamInfo(info, StreamStatus::CONNECT_REQ);
    // Push corresponding SME on the queue
    smeQueue.push(StreamManagementElement(info, StreamStatus::CONNECT));
    // Set flags
    modified_flag = true;
    added_flag = true;
}

void StreamManager::deregisterStream(StreamInfo info) {
    // Mutex lock to access the Stream map from the application thread.
#ifdef _MIOSIX
    miosix::Lock<miosix::Mutex> lck(stream_mutex);
#else
    std::unique_lock<std::mutex> lck(stream_mutex);
#endif
    // Remove Stream class pointer in stream map
    clientMap.erase(info.getStreamId());
    // Mark Stream as closed
    streamMap[info.getStreamId()].setStatus(StreamStatus::CLOSED);
    // Push corresponding SME on the queue
    smeQueue.push(StreamManagementElement(info, StreamStatus::CLOSED));
    // Set flags
    modified_flag = true;
    removed_flag = true;
}

unsigned char StreamManager::getStreamNumber() {
    // Mutex lock to access the shared container StreamMap
#ifdef _MIOSIX
    miosix::Lock<miosix::Mutex> lck(stream_mutex);
#else
    std::unique_lock<std::mutex> lck(stream_mutex);
#endif
    return streamMap.size();
}

StreamStatus StreamManager::getStreamStatus(StreamId id) {
    // Mutex lock to access the shared container StreamMap
#ifdef _MIOSIX
    miosix::Lock<miosix::Mutex> lck(stream_mutex);
#else
    std::unique_lock<std::mutex> lck(stream_mutex);
#endif
    // Check if stream exists
    if (streamMap.find(id) == streamMap.end())
        return StreamStatus::CLOSED;
    else
        return streamMap[id].getStatus();
}

void StreamManager::setStreamStatus(StreamId id, StreamStatus status) {
    // Mutex lock to access the shared container StreamMap
#ifdef _MIOSIX
    miosix::Lock<miosix::Mutex> lck(stream_mutex);
#else
    std::unique_lock<std::mutex> lck(stream_mutex);
#endif
    // Check if stream exists
    if (streamMap.find(id) != streamMap.end())
        streamMap[id].setStatus(status);
}

void StreamManager::addStream(const StreamInfo& stream) {
    // Mutex lock to access the shared container StreamMap
#ifdef _MIOSIX
    miosix::Lock<miosix::Mutex> lck(stream_mutex);
#else
    std::unique_lock<std::mutex> lck(stream_mutex);
#endif
    streamMap[stream.getStreamId()] = stream;
    // Set flags
    modified_flag = true;
    added_flag = true;
}

StreamCollection StreamManager::getSnapshot() {
    // Mutex lock to access the shared container StreamMap
#ifdef _MIOSIX
    miosix::Lock<miosix::Mutex> lck(stream_mutex);
#else
    std::unique_lock<std::mutex> lck(stream_mutex);
#endif
    return StreamCollection(streamMap);
}

unsigned char StreamManager::getNumSME() {
    // Mutex lock to access the shared container StreamMap
#ifdef _MIOSIX
    miosix::Lock<miosix::Mutex> lck(stream_mutex);
#else
    std::unique_lock<std::mutex> lck(stream_mutex);
#endif
    return smeQueue.size();
}

std::vector<StreamManagementElement> StreamManager::dequeueSMEs(unsigned char count) {
    // Mutex lock to access the shared container StreamMap
#ifdef _MIOSIX
    miosix::Lock<miosix::Mutex> lck(stream_mutex);
#else
    std::unique_lock<std::mutex> lck(stream_mutex);
#endif
    unsigned char available = smeQueue.size();
    unsigned char num = std::min(count, available);
    std::vector<StreamManagementElement> result;
    result.reserve(num);
    for(unsigned int i = 0; i < num; i++) {
        result.push_back(smeQueue.front());
        smeQueue.pop();
    }
    return result;
}

void StreamManager::enqueueSMEs(std::vector<StreamManagementElement> smes) {
    // Mutex lock to access the shared container StreamMap
#ifdef _MIOSIX
    miosix::Lock<miosix::Mutex> lck(stream_mutex);
#else
    std::unique_lock<std::mutex> lck(stream_mutex);
#endif
    for(auto& sme: smes) {
        smeQueue.push(sme);
    }
}

} /* namespace mxnet */
