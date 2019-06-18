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
#include "../util/debug_settings.h"
#include <algorithm>
#include <set>

namespace mxnet {

void StreamCollection::receiveSMEs(UpdatableQueue<StreamId,
                                   StreamManagementElement>& smes) {
    auto numSME = smes.size();
    for(unsigned int i=0; i < numSME; i++) {
        auto sme = smes.dequeue();
        StreamId id = sme.getStreamId();

        auto it = collection.find(id);
        // If stream/server is present in collection
        if(it != collection.end()) {
            auto& stream = it->second;
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

void StreamCollection::receiveSchedule(const std::list<ScheduleElement>& schedule) {
/* NOTE: we need to make 3 changes to the streams:
   - Set streams that are ACCEPTED in collection and present in schedule as ESTABLISHED
   - Set streams that are ACCEPTED in collection and missing from schedule as REJECTED
   - Remove the streams that are ESTABLISHED in collection and missing from schedule */
    // Create a copy of the collection keys,
    // to get the streams not present in schedule
    std::set<StreamId> streamsNotInSchedule;
    for(auto& pair : collection) {
        streamsNotInSchedule.insert(pair.first);
    }
    // Cycle over schedule
    for(auto& el : schedule) {
        auto id = el.getStreamId();
        // Search stream in collection
        auto it = collection.find(id); 
        // If stream is present in collection
        if(it != collection.end()) {
            auto& streamInfo = it->second;
            if(streamInfo.getStatus() == MasterStreamStatus::ACCEPTED)
                streamInfo.setStatus(MasterStreamStatus::ESTABLISHED);
            // If stream is ESTABLISHED and present in schedule, no action needed
            // Remove id from streamsNotInSchedule
            streamsNotInSchedule.erase(id);
        }
        // If stream is not present in collection, do nothing
    }
    // Cycle over streams not present in schedule
    for(auto& id : streamsNotInSchedule) {
        // Search stream in collection
        auto it = collection.find(id); 
        // If stream is present in collection
        if(it != collection.end()) {
            auto& streamInfo = it->second;
            if(streamInfo.getStatus() == MasterStreamStatus::ACCEPTED) {
                streamInfo.setStatus(MasterStreamStatus::REJECTED);
                // Enqueue STREAM_REJECT info element
                enqueueInfo(id, InfoType::STREAM_REJECT);
            }
            else if(streamInfo.getStatus() == MasterStreamStatus::ESTABLISHED) {
                // Delete stream because it has been closed
                collection.erase(id);
            }
        }
        // If stream is not present in collection, do nothing
    }
}

std::vector<MasterStreamInfo> StreamCollection::getStreams() {
    std::vector<MasterStreamInfo> result;
    for(auto& stream : collection)
        result.push_back(stream.second);
    return result;
}

void StreamCollection::enqueueInfo(StreamId id, InfoType type) {
    InfoElement info(id, type);
    // Add to sending queue
    infoQueue.enqueue(id, info);
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
    if(status == MasterStreamStatus::REJECTED) {
        // Try to open a new stream because the one we have is REJECTED
        createStream(sme);
    }
}

void StreamCollection::updateServer(MasterStreamInfo& server, StreamManagementElement& sme) {
    StreamId id = sme.getStreamId();
    SMEType type = sme.getType();
    auto status = server.getStatus();
    if(status == MasterStreamStatus::LISTEN) {
        if(type == SMEType::CLOSED) {
            if(SCHEDULER_SUMMARY_DBG)
                printf("[SC] Server (%d,%d,%d,%d) Closed\n", id.src,id.dst,id.srcPort,id.dstPort);
            // Delete server because it has been closed by remote node
            collection.erase(id);
            // Enqueue SERVER_CLOSED info element
            enqueueInfo(id, InfoType::SERVER_CLOSED);
        }
        else if(type == SMEType::LISTEN) {
            // Enqueue again SERVER_OPENED info element
            enqueueInfo(id, InfoType::SERVER_OPENED);
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
                if(SCHEDULER_SUMMARY_DBG)
                    printf("[SC] Stream (%d,%d,%d,%d) Rejected: parameters don't match\n", id.src,id.dst,id.srcPort,id.dstPort);
                // Create REJECTED stream
                collection[id] = MasterStreamInfo(id, clientParams, MasterStreamStatus::REJECTED);
                // Enqueue STREAM_REJECT info element
                enqueueInfo(id, InfoType::STREAM_REJECT);
            }
            // Otherwise create new stream
            else {
                if(SCHEDULER_SUMMARY_DBG)
                    printf("[SC] Stream (%d,%d,%d,%d) Accepted\n", id.src,id.dst,id.srcPort,id.dstPort);
                // Negotiate parameters between client and servers
                StreamParameters newParams = negotiateParameters(serverParams, clientParams);
                // Create ACCEPTED stream with new parameters
                collection[id] = MasterStreamInfo(id, newParams, MasterStreamStatus::ACCEPTED);
                // Set flags
                added_flag = true;
                modified_flag = true;
            } 
        }
        // Server absent
        else {
            if(SCHEDULER_SUMMARY_DBG)
                printf("[SC] Stream (%d,%d,%d,%d) Rejected: server missing\n", id.src,id.dst,id.srcPort,id.dstPort);
            // Create REJECTED stream
            collection[id] = MasterStreamInfo(id, clientParams, MasterStreamStatus::REJECTED);
            // Enqueue STREAM_REJECT info element
            enqueueInfo(id, InfoType::STREAM_REJECT);
        }
    }
}

void StreamCollection::createServer(StreamManagementElement& sme) {
    StreamId id = sme.getStreamId();
    SMEType type = sme.getType();
    StreamParameters params = sme.getParams();
    if(type == SMEType::LISTEN) {
        if(SCHEDULER_SUMMARY_DBG)
            printf("[SC] Server (%d,%d,%d,%d) Accepted\n", id.src,id.dst,id.srcPort,id.dstPort);
        // Create server
        collection[id] = MasterStreamInfo(id, params, MasterStreamStatus::LISTEN);
        // Enqueue SERVER_OPENED info element
        enqueueInfo(id, InfoType::SERVER_OPENED);
    }
    if(type == SMEType::CLOSED) {
        // Enqueue again SERVER_CLOSED info element
        enqueueInfo(id, InfoType::SERVER_CLOSED);
    }
}

StreamParameters StreamCollection::negotiateParameters(StreamParameters& serverParams,
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

std::vector<MasterStreamInfo> StreamSnapshot::getStreams() {
    std::vector<MasterStreamInfo> result;
    for(auto& stream : collection)
        result.push_back(stream.second);
    return result;
}

std::vector<MasterStreamInfo> StreamSnapshot::getStreamsWithStatus(MasterStreamStatus s) {
    std::vector<MasterStreamInfo> result;
    for (auto& stream: collection) {
        if(stream.second.getStatus() == s)
            result.push_back(stream.second);
    }
    return result;
}

} // namespace mxnet
