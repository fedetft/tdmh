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

void StreamManager::registerStream(StreamInfo info, Stream* stream) {
    // Mutex lock to access the Stream map from the application thread.
#ifdef _MIOSIX
    miosix::Lock<miosix::Mutex> lck(stream_mutex);
#else
    std::unique_lock<std::mutex> lck(stream_mutex);
#endif
    // Register stream in stream map
    streamMap[info.getStreamId()] = stream;
    // register SME in request list
    streamList.push_back(info);
}

bool isRequest(StreamInfo stream) {
    StreamStatus status = stream.getStatus();
    return (status == StreamStatus::LISTEN_REQ) ||
           (status == StreamStatus::CONNECT_REQ);
}



unsigned char StreamManager::getNumSME() {
    // Mutex lock to access the shared container StreamList
#ifdef _MIOSIX
    miosix::Lock<miosix::Mutex> lck(stream_mutex);
#else
    std::unique_lock<std::mutex> lck(stream_mutex);
#endif
    return std::count_if(streamList.begin(), streamList.end(), isRequest);
}


std::vector<StreamManagementElement> StreamManager::getSMEs(unsigned char count) {
    // Mutex lock to access the shared container StreamList
#ifdef _MIOSIX
    miosix::Lock<miosix::Mutex> lck(stream_mutex);
#else
    std::unique_lock<std::mutex> lck(stream_mutex);
#endif
    unsigned char available = std::count_if(streamList.begin(), streamList.end(), isRequest);
    unsigned char num = std::min(count, available);
    std::vector<StreamManagementElement> result;
    result.reserve(num);
    // Generate SME for every StreamInfo that needs it
    for (auto& stream: streamList) {
        StreamStatus status = stream.getStatus();
        if(stream.getStatus() == StreamStatus::LISTEN_REQ) {
            StreamManagementElement sme(stream, StreamStatus::LISTEN);
            result.push_back(sme);
        }
        else if(stream.getStatus() == StreamStatus::CONNECT_REQ) {
            StreamManagementElement sme(stream, StreamStatus::CONNECT);
            result.push_back(sme);
        }
    }
    return result;
}

bool isSchedulable(StreamInfo stream) {
    StreamStatus status = stream.getStatus();
    return (status == StreamStatus::ACCEPTED);
}

bool MasterStreamManager::hasNewStreams() {
    // Mutex lock to access the shared container StreamList
#ifdef _MIOSIX
    miosix::Lock<miosix::Mutex> lck(stream_mutex);
#else
    std::unique_lock<std::mutex> lck(stream_mutex);
#endif
    return std::count_if(streamList.begin(), streamList.end(), isSchedulable);
}

}

