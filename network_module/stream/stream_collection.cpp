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

#include <algorithm>
#include <set>

namespace mxnet {

StreamStatus StreamCollection::receiveSMEs(ReceiveUplinkMessage& msg) {
    auto numSME = msg.getNumPacketSMEs();
    for(int i=0; i < numSME; i++) {
        auto sme = msg.getSME();
        StreamId id = sme.getStreamId();

        auto it = collection.find(id);
        // If stream/server is present in collection
        if(it != collection.end()) {
            // SME belongs to stream
            if(id.isStream())
                updateStream(*it, sme);
            // SME belongs to server
            else
                updateServer(*it, sme);
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

StreamStatus StreamCollection::streamEstablished(StreamId id) {
    // Check that this is a stream
    if(id.isServer())
        return;

    auto it = collection.find(id);
    // If stream is present in collection
    if(it != collection.end()) {
        auto status = it->second->getStatus();
        
    }
}

std::vector<StreamInfo> StreamCollection::getStreams() {
    std::vector<StreamInfo> result;
    for(auto& stream : collection)
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

void StreamCollection::updateStream(StreamInfo& stream, StreamManagementElement& sme) {
    StreamId id = sme.getStreamId();
    SMEType type = sme.getType();
    auto status = stream.getStatus();
    if(status == StreamStatus::ESTABLISHED && type == SMEType::CLOSED) {
        // Delete stream because it has been closed
        map.erase(id);
    }
}

void StreamCollection::updateServer(StreamInfo& server, StreamManagementElement& sme) {
    StreamId id = sme.getStreamId();
    SMEType type = sme.getType();
    auto status = server.getStatus();
    if(status == StreamStatus::LISTEN) {
        if(type == SMEType::CLOSED) {
            // Delete server because it has been closed by remote node
            map.erase(id);
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
        if(collection.find(serverId) != collection.end()) {
            auto serverParams = collection[serverId].getParams();
            // If the direction of client and server don't match, reject stream
            if(toInt(serverParams.direction) != toInt(clientParams.direction)) {
                // Create REJECTED stream
                collection[id] = StreamInfo(id, clientParams, StreamStatus::REJECTED);
                // Enqueue STREAM_REJECT info element
                infoQueue.enqueue(id, InfoElement(id, InfoType::STREAM_REJECT));
            }
            // Otherwise create new stream
            else {
                // Negotiate parameters between client and servers
                auto newParams = negotiateParameters(serverParams, clientParams);
                // Create ACCEPTED stream with new parameters
                collection[id] = StreamInfo(id, newParams, StreamStatus::ACCEPTED);
            } 
        }
        // Server absent
        else {
            // Create REJECTED stream
            collection[id] = StreamInfo(id, clientParams, StreamStatus::REJECTED);
            // Enqueue STREAM_REJECT info element
            infoQueue.enqueue(id, InfoElement(id, InfoType::STREAM_REJECT));
        }
    }
}

void StreamCollection::createServer(StreamManagementElement& sme) {
    SMEType type = sme.getType();
    StreamParameters params = sme.getParams();
    if(type == SMEType::LISTEN) {
        // Create server
        collection[id] = StreamInfo(id, params, StreamStatus::LISTEN);
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
    // Pick the lowest redundancy level between Client and Server
    Redundancy redundancy = std::min(toInt(serverParams.redundancy),
                                     toInt(clientParams.redundancy));
    // Pick the highest period between Client and Server
    Period period = std::max(toInt(serverParams.period),
                             toInt(clientParams.period));
    // Pick the lowest payloadSize between Client and Server
    unsigned short payloadSize = std::min(toInt(serverParams.payloadSize),
                                          toInt(clientParams.payloadSize));
    // We assume that direction is the same between Client and Server,
    // Because we checked it in createStream(). So just copy it from one of them
    Direction direction = clientParams.direction;
    // Create resulting StreamParameters struct
    StreamParameters newParams = {static_cast<unsigned int>(redundancy),
                                  static_cast<unsigned int>(period),
                                  payloadSize,
                                  statis_cast<unsigned int>(direction)};
    return newParams;
}

