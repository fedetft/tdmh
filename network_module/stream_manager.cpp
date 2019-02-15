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
    // Register Stream information
    streamMap[info.getStreamId()]= info;
}

bool isRequest(std::pair<StreamId, StreamInfo> stream) {
    StreamStatus status = stream.second.getStatus();
    return (status == StreamStatus::LISTEN_REQ) ||
           (status == StreamStatus::CONNECT_REQ);
}

unsigned char StreamManager::getNumSME() {
    // Mutex lock to access the shared container StreamMap
#ifdef _MIOSIX
    miosix::Lock<miosix::Mutex> lck(stream_mutex);
#else
    std::unique_lock<std::mutex> lck(stream_mutex);
#endif
    return std::count_if(streamMap.begin(), streamMap.end(), isRequest);
}


std::vector<StreamManagementElement> StreamManager::getSMEs(unsigned char count) {
    // Mutex lock to access the shared container StreamMap
#ifdef _MIOSIX
    miosix::Lock<miosix::Mutex> lck(stream_mutex);
#else
    std::unique_lock<std::mutex> lck(stream_mutex);
#endif
    unsigned char available = std::count_if(streamMap.begin(), streamMap.end(), isRequest);
    unsigned char num = std::min(count, available);
    std::vector<StreamManagementElement> result;
    result.reserve(num);
    // Generate SME for every StreamInfo that needs it
    for (auto& stream: streamMap) {
        StreamStatus status = stream.second.getStatus();
        if(status == StreamStatus::LISTEN_REQ) {
            StreamManagementElement sme(stream.second, StreamStatus::LISTEN);
            result.push_back(sme);
        }
        else if(status == StreamStatus::CONNECT_REQ) {
            StreamManagementElement sme(stream.second, StreamStatus::CONNECT);
            result.push_back(sme);
        }
    }
    return result;
}

//TODO: Finish algorithm and move in scheduler, this code is too important to be hidden
/*void StreamManager::putSMEs(const std::vector<StreamManagementElement>& smes) {
    smeId = sme.getStreamId();
    for(auto& sme: smes) {
        std::map<StreamId, StreamInfo>::iterator it;
        it = streamMap.find(sme.getStreamId());
        // If stream already present
        if(it != streamMap.end()) {
            // If old_stream = LISTEN and new_stream = CONNECT: stream ACCEPTED
            if((it.second.getStatus() == StreamStatus::LISTEN) &&
               (sme.getStatus() == StreamStatus::CONNECT))
                streamMap[smeId] = StreamInfo(sme, StreamStatus::ACCEPTED);
            if((it.second.getStatus() == StreamStatus::CONNECT) &&
               (sme.getStatus() == StreamStatus::CONNECT))
                streamMap[smeId] = StreamInfo(sme, StreamStatus::ACCEPTED);
 
            
        }
        // Else add it
        else
            streamMap[smeId] = StreamInfo(sme, sme.getStatus())
    }
}*/

void StreamManager::addStream(const StreamInfo& stream) {
    streamMap[stream.getStreamId()] = stream;
}

} /* namespace mxnet */
