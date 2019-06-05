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
    {
        // Lock map_mutex to access the shared Stream/Server map
#ifdef _MIOSIX
        miosix::Lock<miosix::FastMutex> lck(map_mutex);
#else
        std::unique_lock<std::mutex> lck(map_mutex);
#endif
        // Iterate over all streams
        for(auto& pair: streams) {
            auto stream = pair.second;
            bool deleted = stream->desync();
            if(deleted)
                deleteList.push_back(stream->getStreamId());
        }
        // Delete all streams added to deleteList
        for(auto& streamId: deleteList) {
            removeStream(streamId);
        }
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
    StreamInfo streamInfo(streamId, params, StreamStatus::CONNECTING);
    int fd = -1;
    Endpoint* stream;
    {
        // Lock map_mutex to access the shared Stream map
#ifdef _MIOSIX
        miosix::Lock<miosix::FastMutex> lck(map_mutex);
#else
        std::unique_lock<std::mutex> lck(map_mutex);
#endif
        // Check if a stream with these parameters is already present
        if(streams.find(streamId) != streams.end()) {
            freeClientPort(srcPort);
            return -1;
        }
        fd = addStream(streamInfo);
        // Check if a server with these parameters is present
        auto streamit = fdt.find(fd);
        if(streamit == fdt.end()) {
            return -1;
        }
        stream = streamit->second.get();
    }
    // Make the stream wait for a schedule
    // NOTE: Make sure that stream enqueues a CONNECT SME
    int error = stream->connect(this);
    if(error != 0) {
        removeStream(streamId);
        return -1;
    }
    printStreamStatus(streamId, streamInfo.getStatus());
    return fd;
}

int StreamManager::write(int fd, const void* data, int size) {
    Endpoint* stream;
    {
        // Lock map_mutex to access the shared Stream map
#ifdef _MIOSIX
        miosix::Lock<miosix::FastMutex> lck(map_mutex);
#else
        std::unique_lock<std::mutex> lck(map_mutex);
#endif
        // Check if a stream with these parameters is not present
        if(fdt.find(fd) == fdt.end()) {
            return -1;
        }
        stream = fdt[fd].get();
    }
    return stream->write(data, size);
}

int StreamManager::read(int fd, void* data, int maxSize) {
    Endpoint* stream;
    {
        // Lock map_mutex to access the shared Stream map
#ifdef _MIOSIX
        miosix::Lock<miosix::FastMutex> lck(map_mutex);
#else
        std::unique_lock<std::mutex> lck(map_mutex);
#endif
        // Check if a stream with these parameters is not present
        if(fdt.find(fd) == fdt.end()) {
            return -1;
        }
        stream = fdt[fd].get();
    }
    return stream->read(data, maxSize);
}

StreamInfo StreamManager::getInfo(int fd) {
    Endpoint* endpoint;
    {
        // Lock map_mutex to access the shared Stream/Server map
#ifdef _MIOSIX
        miosix::Lock<miosix::FastMutex> lck(map_mutex);
#else
        std::unique_lock<std::mutex> lck(map_mutex);
#endif
        // Check if a stream with these parameters is not present
        if(fdt.find(fd) == fdt.end()) {
            return StreamInfo();
        }
        endpoint = fdt[fd].get();
    }
    return endpoint->getInfo();
}

void StreamManager::close(int fd) {
    Endpoint* endpoint;
    {
        // Lock map_mutex to access the shared Stream/Server map
#ifdef _MIOSIX
        miosix::Lock<miosix::FastMutex> lck(map_mutex);
#else
        std::unique_lock<std::mutex> lck(map_mutex);
#endif
        // Check if a stream with these parameters is not present
        if(fdt.find(fd) == fdt.end()) {
            return;
        }
        endpoint = fdt[fd].get();
    }
    StreamId id = endpoint->getStreamId();
    bool deleted = endpoint->close(this);
    if(deleted) {
        // Lock map_mutex to access the shared Stream/Server map
#ifdef _MIOSIX
        miosix::Lock<miosix::FastMutex> lck(map_mutex);
#else
        std::unique_lock<std::mutex> lck(map_mutex);
#endif
        if(id.isServer()) {
            unsigned char port = id.dstPort;
            removeServer(port);
        }
        else {
            removeStream(id);
        }
    }
}

int StreamManager::listen(unsigned char port, StreamParameters params) {
    auto serverId = StreamId(myId, myId, 0, port);
    StreamInfo serverInfo(serverId, params, StreamStatus::LISTEN_WAIT);
    int fd = -1;
    Endpoint* server;
    {
        // Lock map_mutex to access the shared Server map
#ifdef _MIOSIX
        miosix::Lock<miosix::FastMutex> lck(map_mutex);
#else
        std::unique_lock<std::mutex> lck(map_mutex);
#endif
        // Check if a server with these parameters is already present
        if(servers.find(port) != servers.end()) {
            return -1;
        }
        fd = addServer(serverInfo);
        // Check if a server with these parameters is present
        auto serverit = fdt.find(fd);
        if(serverit == fdt.end()) {
            return -1;
        }
        server = serverit->second.get();
    }
    // Make the server wait for an info element confirming LISTEN status
    int error = server->listen(this);
    if(error != 0) {
        unsigned char port = serverId.dstPort;
        removeServer(port);
        return -1;
    }
    printServerStatus(serverId, serverInfo.getStatus());
    return fd;
}

int StreamManager::accept(int serverfd) {
    Endpoint* server;
    int fd = -1;
    Endpoint* stream;
    {
        // Lock map_mutex to access the shared Server map
#ifdef _MIOSIX
        miosix::Lock<miosix::FastMutex> lck(map_mutex);
#else
        std::unique_lock<std::mutex> lck(map_mutex);
#endif
        // Check if a server with these parameters is not present
        auto serverit = fdt.find(serverfd);
        if(serverit == fdt.end()) {
            return -1;
        }
        server = serverit->second.get();
    }
    fd = server->accept();
    {
        // Lock map_mutex to access the shared Stream map
#ifdef _MIOSIX
        miosix::Lock<miosix::FastMutex> lck(map_mutex);
#else
        std::unique_lock<std::mutex> lck(map_mutex);
#endif

        // Check if a stream with these parameters is present
        auto streamit = fdt.find(fd);
        if(streamit == fdt.end()) {
            return -1;
        }
        stream = streamit->second.get();
    }
    stream->acceptedStream();
    return fd;
}

void StreamManager::periodicUpdate() {
    // Lock map_mutex to access the shared Stream/Server map
#ifdef _MIOSIX
    miosix::Lock<miosix::FastMutex> lck(map_mutex);
#else
    std::unique_lock<std::mutex> lck(map_mutex);
#endif
    // Iterate over all streams
    for(auto& stream: streams) {
        stream.second->periodicUpdate(this);
    }
    // Iterate over all servers
    for(auto& server: servers) {
        server.second->periodicUpdate(this);
    }
}

void StreamManager::receivePacket(StreamId id, const Packet& data) {
    Stream* stream;
    {
        // Lock map_mutex to access the shared Stream map
#ifdef _MIOSIX
        miosix::Lock<miosix::FastMutex> lck(map_mutex);
#else
        std::unique_lock<std::mutex> lck(map_mutex);
#endif
        // Check if a stream with these parameters is not present
        auto streamit = streams.find(id);
        if(streamit == streams.end()) {
            return;
        }
        stream = streamit->second.get();
    }
    stream->receivePacket(data);
}

void StreamManager::missPacket(StreamId id) {
    Stream* stream;
    {
        // Lock map_mutex to access the shared Stream map
#ifdef _MIOSIX
        miosix::Lock<miosix::FastMutex> lck(map_mutex);
#else
        std::unique_lock<std::mutex> lck(map_mutex);
#endif
        // Check if a stream with these parameters is not present
        auto streamit = streams.find(id);
        if(streamit == streams.end()) {
            return;
        }
        stream = streamit->second.get();
    }
    stream->missPacket();
}

bool StreamManager::sendPacket(StreamId id, Packet& data) {
    Stream* stream;
    {
        // Lock map_mutex to access the shared Stream map
#ifdef _MIOSIX
        miosix::Lock<miosix::FastMutex> lck(map_mutex);
#else
        std::unique_lock<std::mutex> lck(map_mutex);
#endif
        // Check if a stream with these parameters is not present
        auto streamit = streams.find(id);
        if(streamit == streams.end()) {
            return false;
        }
        stream = streamit->second.get();
    }
    return stream->sendPacket(data);
}

void StreamManager::applySchedule(const std::vector<ScheduleElement>& schedule) {
    // Lock map_mutex to access the shared Stream/Server map
#ifdef _MIOSIX
    miosix::Lock<miosix::FastMutex> lck(map_mutex);
#else
    std::unique_lock<std::mutex> lck(map_mutex);
#endif
    // Create vector containing StreamId of all streams in map
    // We will remove from this vector the streams present in the schedule,
    // To get the remaining streams present in map but not in schedule.
    std::vector<StreamId> removedStreams;
    for (auto it=streams.begin(); it!=streams.end(); ++it)
        removedStreams.push_back(it->first);

    // Iterate over new schedule
    for(auto& element : schedule) {
        StreamId streamId = element.getStreamId();
        // NOTE: Ignore streams of which we are not source or destination 
        if(streamId.src != myId && streamId.dst != myId)
            continue;
        auto streamit = streams.find(streamId);
        // If stream in schedule is present in map, call addedStream()
        if(streamit != streams.end()) {
            auto stream = streamit->second;
            // NOTE: Update stream parameters,
            // they may have changed after negotiation with server
            stream->addedStream(element.getParams());
            // Remove streamId from removedStreams
            removedStreams.erase(std::remove(removedStreams.begin(),
                                             removedStreams.end(),
                                             streamit->first),
                                 removedStreams.end());
        }
        // If stream in schedule is not present in map
        else {
            StreamParameters params = element.getParams();
            StreamId serverId = streamId.getServerId();
            unsigned char port = serverId.dstPort;
            auto serverit = servers.find(port);
            // If the corresponding server is present, create new stream in
            // ACCEPT_WAIT status and register it in corresponding server
            if(serverit != servers.end() &&
               (serverit->second->getInfo().getStatus() == StreamStatus::LISTEN)) {
                StreamInfo streamInfo(streamId, params, StreamStatus::ACCEPT_WAIT);
                int fd = addStream(streamInfo);
                auto server = serverit->second;
                server->addPendingStream(fd);
            }
            // If the corresponding server is not present,
            else {
                // 1. Create new stream in CLOSE_WAIT status
                StreamInfo streamInfo(streamId, params, StreamStatus::CLOSE_WAIT);
                int fd = addStream(streamInfo);
                // NOTE: Make sure that stream enqueues a CLOSED SME
                fdt[fd]->close(this);
                // 2. Create new server in CLOSE_WAIT status
                StreamInfo serverInfo(serverId, params, StreamStatus::CLOSE_WAIT);
                fd = addServer(serverInfo);
                // NOTE: Make sure that the server enqueues a CLOSED SME
                fdt[fd]->close(this);
            }
        }
    }
    // If stream present in map and not in schedule, call removedStream()
    for(auto& streamId : removedStreams) {
        // If return value is true, we can delete the Stream class
        if(streams[streamId]->removedStream())
            removeStream(streamId);
    }
    // Reset redundancy counters to avoid errors
    // NOTE: this measure works by assuming that when a new schedule begins
    // all redundancy counters should be set to zero
    for(auto& stream : streams) {
        stream.second->resetCounters();
    }
}

void StreamManager::applyInfoElements(const std::vector<InfoElement>& infos) {
    // Lock map_mutex to access the shared Stream/Server map
#ifdef _MIOSIX
    miosix::Lock<miosix::FastMutex> lck(map_mutex);
#else
    std::unique_lock<std::mutex> lck(map_mutex);
#endif
    // Iterate over info elements
    for(auto& info : infos) {
        StreamId id = info.getStreamId();
        // NOTE: Ignore streams of which we are not source or destination 
        if(id.src != myId && id.dst != myId)
            continue;
        if(id.isServer()) {
            unsigned char port = id.dstPort;
            auto serverit = servers.find(port);
            // If server is present in map
            if(serverit != servers.end()) {
                auto server = serverit->second;
                InfoType type = info.getType();
                if(type==InfoType::SERVER_OPENED)
                    server->acceptedServer();
                else if(type==InfoType::SERVER_CLOSED)
                    server->rejectedServer();
            }
            // If server is not present
            else if(id.isServer() && info.getType() == InfoType::SERVER_OPENED) {
                // Create server in CLOSE_WAIT to warn the master node
                // that this server is actually closed
                StreamInfo serverInfo(id, info.getParams(), StreamStatus::CLOSE_WAIT);
                int fd = addServer(serverInfo);
                // NOTE: Make sure that the server enqueues a CLOSED SME
                fdt[fd]->close(this);
            }
        }
        else {
            auto streamit = streams.find(id);
            // If stream is present in map
            if(streamit != streams.end()) {
                auto stream = streamit->second;
                InfoType type = info.getType();
                if(type==InfoType::STREAM_REJECT)
                    stream->rejectedStream();
            }
        }
    }
}

void StreamManager::dequeueSMEs(UpdatableQueue<StreamId,StreamManagementElement>& queue) {
    // Lock map_mutex to access the shared SME queue
#ifdef _MIOSIX
    miosix::Lock<miosix::FastMutex> lck(sme_mutex);
#else
    std::unique_lock<std::mutex> lck(sme_mutex);
#endif
    while(!smeQueue.empty()) {
        auto temp = smeQueue.dequeue();
        StreamId tempId = temp.getStreamId();
        queue.enqueue(tempId, std::move(temp));
    }
}

void StreamManager::enqueueSME(StreamManagementElement sme) {
    StreamId id = sme.getStreamId();
    {
        // Lock map_mutex to access the shared SME queue
#ifdef _MIOSIX
        miosix::Lock<miosix::FastMutex> lck(sme_mutex);
#else
        std::unique_lock<std::mutex> lck(sme_mutex);
#endif
        smeQueue.enqueue(id, sme);
    }
}

void StreamManager::closedServer(int fd) {
    Endpoint* stream;
    {
        // Lock map_mutex to access the shared Stream map
#ifdef _MIOSIX
        miosix::Lock<miosix::FastMutex> lck(map_mutex);
#else
        std::unique_lock<std::mutex> lck(map_mutex);
#endif
        // Check if a stream with these parameters is not present
        if(fdt.find(fd) == fdt.end()) {
            return;
        }
        stream = fdt[fd].get();
    }
    stream->closedServer(this);
}

int StreamManager::allocateClientPort() {
    // Lock map_mutex to access the shared port vector
#ifdef _MIOSIX
    miosix::Lock<miosix::FastMutex> lck(map_mutex);
#else
    std::unique_lock<std::mutex> lck(map_mutex);
#endif
    for(unsigned int i = 0; i < maxPorts; i++) {
        if(clientPorts[i] == false) {
            clientPorts[i] = true;
            return i;
        }
    }
    // No free port found
    return -1;
}

void StreamManager::freeClientPort(unsigned char port) {
    // Invalid port number provided
    if(port < 0 || port > (maxPorts-1))
        return;
    clientPorts[port] = false;
}

int StreamManager::addStream(StreamInfo streamInfo) {
    int fd = fdcounter++;
#ifdef _MIOSIX
    miosix::intrusive_ref_ptr<Stream> stream(new Stream(config, fd, streamInfo));
#else
    std::shared_ptr<Stream> stream(new Stream(config, fd, streamInfo));
#endif
    StreamId streamId = streamInfo.getStreamId();
    streams[streamId] = stream;
    fdt[fd] = stream;
    return fd;
}

int StreamManager::addServer(StreamInfo serverInfo) {
    int fd = fdcounter++;
#ifdef _MIOSIX
    miosix::intrusive_ref_ptr<Server> server(new Server(config, fd, serverInfo));
#else
    std::shared_ptr<Server> server(new Server(config, fd, serverInfo));
#endif
    unsigned char port = serverInfo.getStreamId().dstPort;
    servers[port] = server;
    fdt[fd] = server;
    return fd;
}

void StreamManager::removeStream(StreamId id) {
    auto streamit = streams.find(id);
    if(streamit == streams.end())
        return;

    auto stream = streamit->second;
    int fd = stream->getFd();
    streams.erase(id);
    fdt.erase(fd);
    freeClientPort(id.srcPort);
}

void StreamManager::removeServer(unsigned char port) {
    auto serverit = servers.find(port);
    if(serverit == servers.end())
        return;

    auto server = serverit->second;
    int fd = server->getFd();
    servers.erase(port);
    fdt.erase(fd);
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
