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
#include "../util/debug_settings.h"
#include <algorithm>
#include <set>

namespace mxnet {

void StreamManager::desync() {
    std::vector<StreamId> deleteList;

    // Lock map_mutex to access the shared Stream/Server map
    map_mutex.lock();

    // Iterate over all streams
    for(auto& stream: streams) {
        bool deleted = stream->desync();
        if(deleted)
            deleteList.push_back(stream->getStreamId());
    }
    map_mutex.unlock();

    // Delete all streams added to deleteList
    for(auto& streamId: deleteList) {
        removeStream(streamId);
    }

    // Clear SME queue, otherwise we could be sending old SMEs
    // after resync()
    smeQueue.clear();
}

int StreamManager::connect(unsigned char dst, unsigned char dstPort, StreamParameters params) {
    int srcPort = allocateClientPort();
    if(srcPort == -1)
        return -1;

    auto streamId = StreamId(myId, dst, srcPort, dstPort);
    auto streamInfo = StreamInfo(streamId, params, StreamStatus::CONNECTING);
    auto* stream = new Stream(streamInfo);

    // Lock map_mutex to access the shared Stream map
    map_mutex.lock();

    // Check if a stream with these parameters is already present
    if(streams.find(streamId) != streams.end()) {
        map_mutex.unlock();
        delete stream;
        freeClientPort(srcPort);
        return -1;
    }
    streams[streamId] = stream;
    int fd = fdcounter++;
    fdt[fd] = stream;
    map_mutex.unlock();

    printStreamStatus(streamId, streamInfo.getStatus());
    // Make the stream wait for a schedule
    // NOTE: Make sure that stream enqueues a CONNECT SME
    int error = stream->connect(this);
    if(error != 0) {
        removeStream(stream);
        return -1;
    }
    return fd;
}

int StreamManager::write(int fd, const void* data, int size) {
    // Lock map_mutex to access the shared Stream map
    map_mutex.lock();

    // Check if a stream with these parameters is not present
    if(fdt.find(fd) == fdt.end()) {
        map_mutex.unlock();
        return -1;
    }
    auto* stream = fdt[fd];
    map_mutex.unlock();

    return stream->write(data, size);
}

int StreamManager::read(int fd, void* data, int maxSize) {
    // Lock map_mutex to access the shared Stream map
    map_mutex.lock();

    // Check if a stream with these parameters is not present
    if(fdt.find(fd) == fdt.end()) {
        map_mutex.unlock();
        return -1;
    }
    auto* stream = fdt[fd];
    map_mutex.unlock();

    return stream->read(data, maxSize);
}

StreamInfo StreamManager::getInfo(int fd) {
    // Lock map_mutex to access the shared Stream map
    map_mutex.lock();

    // Check if a stream with these parameters is not present
    if(fdt.find(fd) == fdt.end()) {
        map_mutex.unlock();
        return -1;
    }
    auto* stream = fdt[fd];
    map_mutex.unlock();

    return stream->getInfo(this);
}

void StreamManager::close(int fd) {
    // Lock map_mutex to access the shared Stream/Server map
    map_mutex.lock();

    // Check if a stream with these parameters is not present
    if(fdt.find(fd) == fdt.end()) {
        map_mutex.unlock();
        return -1;
    }
    auto* endpoint = fdt[fd];
    map_mutex.unlock();

    StreamId id = endpoint->getStreamId();
    bool deleted = endpoint->close(this);
    if(deleted && id.src != id.dst) { 
        removeStream(stream);
    }
}

int StreamManager::listen(unsigned char port, StreamParameters params) {
    auto serverId = StreamId(myId, myId, 0, port);
    auto serverInfo = StreamInfo(serverId, params, StreamStatus::LISTEN_WAIT);
    auto* server = new Server(serverInfo);

    // Lock map_mutex to access the shared Server map
    map_mutex.lock();

    // Check if a server with these parameters is already present
    if(servers.find(serverId) != servers.end()) {
        map_mutex.unlock();
        delete server;
        return -1;
    }
    servers[port] = server;
    int fd = fdcounter++;
    fdt[fd] = server;
    map_mutex.unlock();

    printServerStatus(serverId, serverInfo.getStatus());
    // Make the server wait for an info element confirming LISTEN status
    int error = server->listen(this);
    if(error != 0) {
        removeServer(server);
        return -1;
    }
    return fd;
}

int StreamManager::accept(int serverfd) {
    // Lock map_mutex to access the shared Server map
    map_mutex.lock();

    // Check if a server with these parameters is not present
    if(fdt.find(fd) == fdt.end()) {
        map_mutex.unlock();
        return -1;
    }
    auto* server = fdt[fd];
    map_mutex.unlock();

    return server->accept();
}

void StreamManager::periodicUpdate() {
    // Lock map_mutex to access the shared Stream/Server map
    map_mutex.lock();

    // Iterate over all streams
    for(auto& stream: streams) {
        stream->periodicUpdate();
    }

    // Iterate over all servers
    for(auto& server: servers) {
        server->periodicUpdate();
    }

    map_mutex.unlock();
}

void StreamManager::putPacket(StreamId id, const Packet& data) {
    // Lock map_mutex to access the shared Stream map
    map_mutex.lock();

    // Check if a stream with these parameters is not present
    if(fdt.find(fd) == fdt.end()) {
        map_mutex.unlock();
        return;
    }
    auto* stream = fdt[fd];
    map_mutex.unlock();

    stream->putPacket(data);
}

void StreamManager::getPacket(StreamId id, Packet& data) {
    // Lock map_mutex to access the shared Stream map
    map_mutex.lock();

    // Check if a stream with these parameters is not present
    if(fdt.find(fd) == fdt.end()) {
        map_mutex.unlock();
        return;
    }
    auto* stream = fdt[fd];
    map_mutex.unlock();

    stream->getPacket(data);
}

void StreamManager::applySchedule(const std::vector<ScheduleElement>& schedule) {
    // Lock map_mutex to access the shared Stream map
    map_mutex.lock();

    // Create vector containing StreamId of all streams in map
    // We will remove from this vector the streams present in the schedule,
    // To get the remaining streams present in map but not in schedule.
    std::vector<StreamId> removedStreams;
    for (auto it=streams.begin(); it!=streams.end(); ++it)
        removedStreams.push_back(it->first);

    // Iterate over new schedule
    for(auto& element : schedule) {
        StreamId streamId = element.getStreamId();
        auto streamit = streams.find(streamId);
        // If stream present in schedule and map call addedStream()
        if(streamit != streams.end()) {
            auto* stream = streamit->second;
            stream->addedStream();
            // Remove streamId from removedStreams
            removedStreams.erase(std::remove(removedStreams.begin(),
                                             removedStreams.end(),
                                             streamit->first),
                                 removedStreams.end());
        }
        // If stream present in schedule and not in map,
        else {
            StreamParameters params = element.getStreamParams();
            StreamId serverId = streamId.getServerId();
            auto serverit = servers.find(serverId);
            // If the corresponding server is present, create new stream in
            // ACCEPT_WAIT status and register it in corresponding server
            if(serverit != servers.end()) {
                auto* server = serverit->second;
                auto streamInfo = StreamInfo(streamId, params, StreamStatus::ACCEPT_WAIT);
                auto* stream = new Stream(streamInfo);
                streams[streamId] = stream;
                int fd = fdcounter++;
                fdt[fd] = stream;
                server->addPendingStream(stream);
            }
            // If the corresponding server is not present, create new stream in
            // CLOSE_WAIT status
            else {
                auto streamInfo = StreamInfo(streamId, params, StreamStatus::CLOSE_WAIT);
                auto* stream = new Stream(streamInfo);
                streams[streamId] = stream;
                int fd = fdcounter++;
                fdt[fd] = stream;
                // NOTE: Make sure that stream enqueues a CLOSED SME
                stream->close(this);
            }
        }
    }
    // If stream present in map and not in schedule, call removedStream()
    for(auto& streamId : removedStreams) {
        // If return value is true, we can delete the Stream class
        if(streams[streamId]->removedStream())
            removeStream(streamId);
    }
}

void StreamManager::applyInfoElements(const std::vector<InfoElement>& infos) {
    // Iterate over info elements
    for(auto& info : infos) {
        StreamId id = info.getStreamId();
        if (streamId.isServer()) {
            auto serverit = servers.find(id);
            // If server is present in map
            if(serverit != servers.end()) {
                auto* server = server->second;
                InfoType type = info.getType();
                if(type==InfoType::SERVER_ACCEPT)                             
                    server->acceptedServer();
                else if(type==InfoType::SERVER_REJECT)
                    server->rejectedServer();
            }
        }
        else {
            auto streamit = streams.find(streamId);
            // If stream is present in map
            if(streamit != streams.end()) {
                auto* stream = streamit->second;
                InfoType type = info.getType();
                if(type==InfoType::STREAM_REJECT)
                    stream->rejectedStream();
            }
        }
    }
}

void StreamManager::dequeueSMEs(UpdatableQueue<StreamId,StreamManagementElement>& queue) {
    // Lock sme_mutex to access the shared SME queue
    sme_mutex.lock();

    while(!smeQueue.empty()) {
        auto temp = std::move(smeQueue.front());
        smeQueue.pop();
        StreamId tempId = temp.getStreamId();
        queue.enqueue(tempId, std::move(temp));
    }

    sme_mutex.unlock();
}

void StreamManager::enqueueSME(StreamManagementElement sme) {
    StreamId id = sme.getStreamId();

    // Lock sme_mutex to access the shared SME queue
    sme_mutex.lock();
    smeQueue.enqueue(id, sme);
    sme_mutex.unlock();
}

int StreamManager::allocateClientPort() {
    for(int i=0; i<)    
}

void StreamManager::printStreamStatus(StreamId id, StreamStatus status) {
    if(!ENABLE_STREAM_MGR_INFO_DBG)
        return;
    print_dbg("[SM] Stream (%d,%d,%d,%d): ", id.src,id.dst,
              id.srcPort,
              id.dstPort);
    switch(status){
    case StreamStatus::CONNECTING:
        printf("CONNECTING");
        break;
    case StreamStatus::CONNECT_FAILED:
        printf("CONNECT_FAILED");
        break;
    case StreamStatus::ACCEPT_WAIT:
        printf("ACCEPT_WAIT");
        break;
    case StreamStatus::ESTABLISHED:
        printf("ESTABLISHED");
        break;
    case StreamStatus::REMOTELY_CLOSED:
        printf("REMOTELY_CLOSED");
        break;
    case StreamStatus::REOPENED:
        printf("REOPENED");
        break;
    case StreamStatus::CLOSE_WAIT:
        printf("CLOSE_WAIT");
        break;
    default:
        printf("INVALID!");
    }
    printf("\n");
}

void StreamManager::printServerStatus(StreamId id, StreamStatus status) {
    if(!ENABLE_STREAM_MGR_INFO_DBG)
        return;
    print_dbg("[SM] Server (%d,%d): ", id.dst,id.dstPort);
    switch(status){
    case StreamStatus::LISTEN_WAIT:
        printf("LISTEN_WAIT");
        break;
    case StreamStatus::LISTEN_FAILED:
        printf("LISTEN_FAILED");
        break;
    case StreamStatus::LISTEN:
        printf("LISTEN");
        break;
    case StreamStatus::REMOTELY_CLOSED:
        printf("REMOTELY_CLOSED");
        break;
    case StreamStatus::REOPENED:
        printf("REOPENED");
        break;
    case StreamStatus::CLOSE_WAIT:
        printf("CLOSE_WAIT");
        break;
    default:
        printf("INVALID!");
    }
    printf("\n");
}

} /* namespace mxnet */
