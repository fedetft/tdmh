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

#include "stream_collection.h"
#include <algorithm>
#include <set>

namespace mxnet {

void StreamCollection::receiveSMEs(ReceiveUplinkMessage& msg) {
    auto numSME = msg.getNumPacketSMEs();
    for(int i=0; i < numSME; i++) {
        auto sme = msg.getSME();
        StreamId id = sme.getStreamId();

        auto it = collection.find(id);
        // If stream/server is present in collection
        if(it != collection.end()) {
            auto stream = it->second;
            // SME belongs to stream
            if(id.isStream())
                updateStream(stream, sme);
            // SME belongs to server
            else
                updateServer(stream, sme);
        }
        // If stream/server is not present in collection
        else {
            // SME belongs to stream
            if(id.isStream())
                createStream(sme);
            // SME belongs to server
            else
                createServer(sme);
        }
    }
}

void StreamCollection::streamEstablished(StreamId id) {
    // Check that this is a stream
    if(id.isServer())
        return;
    auto it = collection.find(id);
    // If stream is present in collection
    if(it != collection.end()) {
        MasterStreamInfo& stream = it->second;
        if(stream.getStatus() == MasterStreamStatus::ACCEPTED)
            stream.setStatus(MasterStreamStatus::ESTABLISHED);
    }
}

void StreamCollection::streamRejected(StreamId id) {
    // Check that this is a stream
    if(id.isServer())
        return;
    auto it = collection.find(id);
    // If stream is present in collection
    if(it != collection.end()) {
        MasterStreamInfo& stream = it->second;
        if(stream.getStatus() == MasterStreamStatus::ACCEPTED) {
            stream.setStatus(MasterStreamStatus::REJECTED);
            // Enqueue STREAM_REJECT info element
            infoQueue.enqueue(id, InfoElement(id, InfoType::STREAM_REJECT));
        }
    }
}

std::vector<MasterStreamInfo> StreamCollection::getStreams() {
    std::vector<MasterStreamInfo> result;
    for(auto& stream : collection)
        result.push_back(stream.second);
    return result;
}

bool isSchedulable(std::pair<StreamId,MasterStreamInfo> stream) {
    MasterStreamStatus status = stream.second.getStatus();
    return (status == MasterStreamStatus::ACCEPTED);
}

bool StreamCollection::hasSchedulableStreams() {
    return std::count_if(collection.begin(), collection.end(), isSchedulable);
}

std::vector<MasterStreamInfo> StreamCollection::getStreamsWithStatus(MasterStreamStatus s) {
    std::vector<MasterStreamInfo> result;
    for (auto& stream: collection) {
        if(stream.second.getStatus() == s)
            result.push_back(stream.second);
    }
    return result;
}

std::vector<InfoElement> StreamCollection::dequeueInfo(unsigned int num) {
    std::vector<InfoElement> result;
    unsigned int i=0;
    while(i<num && !infoQueue.empty()) {
        result.push_back(infoQueue.dequeue());
        i++;
    }
    return result;
}

void StreamCollection::updateStream(MasterStreamInfo& stream, StreamManagementElement& sme) {
    StreamId id = sme.getStreamId();
    SMEType type = sme.getType();
    auto status = stream.getStatus();
    if(status == MasterStreamStatus::ESTABLISHED && type == SMEType::CLOSED) {
        // Delete stream because it has been closed
        collection.erase(id);
        // Set flags
        removed_flag = true;
        modified_flag = true;
    }
}

void StreamCollection::updateServer(MasterStreamInfo& server, StreamManagementElement& sme) {
    StreamId id = sme.getStreamId();
    SMEType type = sme.getType();
    auto status = server.getStatus();
    if(status == MasterStreamStatus::LISTEN) {
        if(type == SMEType::CLOSED) {
            // Delete server because it has been closed by remote node
            collection.erase(id);
            // Enqueue SERVER_CLOSED info element
            infoQueue.enqueue(id, InfoElement(id, InfoType::SERVER_CLOSED));
        }
        else if(type == SMEType::LISTEN) {
            // Enqueue again SERVER_OPENED info element
            infoQueue.enqueue(id, InfoElement(id, InfoType::SERVER_OPENED));
        }
    }
}

void StreamCollection::createStream(StreamManagementElement& sme) {
    StreamId id = sme.getStreamId();
    SMEType type = sme.getType();
    auto clientParams = sme.getParams();
    if(type == SMEType::CONNECT) {
        // Check for corresponding Server
        auto serverId = id.getServerId();
        // Server present (can only be in LISTEN status)
        auto serverit = collection.find(serverId);
        if(serverit != collection.end()) {
            auto server = serverit->second;
            auto serverParams = server.getParams();
            // If the direction of client and server don't match, reject stream
            if(serverParams.direction != clientParams.direction) {
                // Create REJECTED stream
                collection[id] = MasterStreamInfo(id, clientParams, MasterStreamStatus::REJECTED);
                // Enqueue STREAM_REJECT info element
                infoQueue.enqueue(id, InfoElement(id, InfoType::STREAM_REJECT));
            }
            // Otherwise create new stream
            else {
                // Negotiate parameters between client and servers
                auto newParams = negotiateParameters(serverParams, clientParams);
                // Create ACCEPTED stream with new parameters
                collection[id] = MasterStreamInfo(id, newParams, MasterStreamStatus::ACCEPTED);
                // Set flags
                added_flag = true;
                modified_flag = true;
            } 
        }
        // Server absent
        else {
            // Create REJECTED stream
            collection[id] = MasterStreamInfo(id, clientParams, MasterStreamStatus::REJECTED);
            // Enqueue STREAM_REJECT info element
            infoQueue.enqueue(id, InfoElement(id, InfoType::STREAM_REJECT));
        }
    }
}

void StreamCollection::createServer(StreamManagementElement& sme) {
    StreamId id = sme.getStreamId();
    SMEType type = sme.getType();
    StreamParameters params = sme.getParams();
    if(type == SMEType::LISTEN) {
        // Create server
        collection[id] = MasterStreamInfo(id, params, MasterStreamStatus::LISTEN);
        // Enqueue SERVER_OPENED info element
        infoQueue.enqueue(id, InfoElement(id, InfoType::SERVER_OPENED));
    }
    if(type == SMEType::CLOSED) {
        // Enqueue again SERVER_CLOSED info element
        infoQueue.enqueue(id, InfoElement(id, InfoType::SERVER_CLOSED));
    }
}

StreamParameters negotiateParameters(StreamParameters& serverParams,
                                     StreamParameters& clientParams) {
    /* NOTE: during the negotiation we compare the "unsigned int" parameters
     * only converting the resulting parameters, this to avoid double conversions. */
    // Pick the lowest redundancy level between Client and Server
    Redundancy redundancy = static_cast<Redundancy>(std::min(serverParams.redundancy,
                                                             clientParams.redundancy));
    // Pick the highest period between Client and Server
    Period period = static_cast<Period>(std::max(serverParams.period,
                                                 clientParams.period));
    // Pick the lowest payloadSize between Client and Server
    unsigned short payloadSize = std::min(serverParams.payloadSize,
                                          clientParams.payloadSize);
    // We assume that direction is the same between Client and Server,
    // Because we checked it in createStream(). So just copy it from one of them
    Direction direction = clientParams.getDirection();
    // Create resulting StreamParameters struct
    StreamParameters newParams(redundancy, period, payloadSize, direction);
    return newParams;
}

} // namespace mxnet
