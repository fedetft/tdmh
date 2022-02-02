/***************************************************************************
 *   Copyright (C) 2019-2020 by Federico Amedeo Izzo, Valeria Mazzola      *
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
#include "../mac_context.h"

namespace mxnet {

void StreamManager::desync() {
    std::list<StreamId> deleteList;
    {
        // Lock stream_manager_mutex to access the shared Stream/Server map
#ifdef _MIOSIX
        miosix::Lock<miosix::FastMutex> lck(stream_manager_mutex);
#else
        std::unique_lock<std::mutex> lck(stream_manager_mutex);
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
    {
#ifdef _MIOSIX
        miosix::Lock<miosix::FastMutex> lck(sme_mutex);
#else
        std::unique_lock<std::mutex> lck(sme_mutex);
#endif
        // Clear SME queue, otherwise we could be sending old SMEs
        // after resync()
        smeQueue.clear();
    }
}

int StreamManager::wait(int fd)
{
    REF_PTR_EP stream;
    {
        // Lock stream_manager_mutex to access the shared Stream map
#ifdef _MIOSIX
        miosix::Lock<miosix::FastMutex> lck(stream_manager_mutex);
#else
        std::unique_lock<std::mutex> lck(stream_manager_mutex);
#endif
        if(!masterTrusted) { 
            return -10; 
        }
        auto it = fdt.find(fd);
        if(it == fdt.end()) {
            return -1; 
        }
        
        stream = it->second;
    }

    // avoid non-sending streams to wait
    if (stream->getInfo().getDirection() == Direction::RX)
        return -2;

    stream->wait();

    return 0;
}

bool StreamManager::wakeup(StreamId id)
{
    REF_PTR_EP stream;
    {
        // Lock stream_manager_mutex to access the shared Stream map
#ifdef _MIOSIX
        miosix::Lock<miosix::FastMutex> lck(stream_manager_mutex);
#else
        std::unique_lock<std::mutex> lck(stream_manager_mutex);
#endif
        auto streamit = streams.find(id);
        if(streamit == streams.end()) return false;
        stream = streamit->second;
    }

    stream->wakeup();

    return true;
}

unsigned int StreamManager::getWakeupAdvance(StreamId id) {
    REF_PTR_EP stream;
    {
        // Lock stream_manager_mutex to access the shared Stream map
#ifdef _MIOSIX
        miosix::Lock<miosix::FastMutex> lck(stream_manager_mutex);
#else
        std::unique_lock<std::mutex> lck(stream_manager_mutex);
#endif
        auto streamit = streams.find(id);
        if(streamit == streams.end()) return false;
        stream = streamit->second;
    }

    return stream->getWakeupAdvance();
}

int StreamManager::connect(unsigned char dst, unsigned char dstPort, StreamParameters params, unsigned int wakeupAdvance) {
    int fd = -1;
    REF_PTR_EP stream;
    StreamId streamId;

    {
        // Lock stream_manager_mutex to access the shared Stream map
#ifdef _MIOSIX
        miosix::Lock<miosix::FastMutex> lck(stream_manager_mutex);
#else
        std::unique_lock<std::mutex> lck(stream_manager_mutex);
#endif
        if(!masterTrusted) return -10;
        int srcPort = allocateClientPort();
        if(srcPort == -1)
            return -1;
        streamId = StreamId(myId, dst, srcPort, dstPort);
        StreamInfo streamInfo(streamId, params, StreamStatus::CONNECTING);
    
        // Check if a stream with these parameters is already present
        if(streams.find(streamId) != streams.end()) {
            freeClientPort(srcPort);
            return -1;
        }
        auto res = addStream(streamInfo);
        fd = res.first;
        stream = res.second;
    }

    if (wakeupAdvance > 0) {
        // wakeup advance at most equal to the duration of a tile
        if (wakeupAdvance >= (ctx.getSlotsInTileCount() * ctx.getDataSlotDuration())) {
            return -2;
        }
        unsigned int n = wakeupAdvance / ctx.getDataSlotDuration();
        wakeupAdvance = n * ctx.getDataSlotDuration();
        stream->setWakeupAdvance(wakeupAdvance);
    }
    else {
        return -3;
    }

    // Make the stream wait for a schedule
    // NOTE: Make sure that stream enqueues a CONNECT SME
    int error = stream->connect(this);
    if(error != 0) {
        // NOTE: locking to call removeStream safely
#ifdef _MIOSIX
        miosix::Lock<miosix::FastMutex> lck(stream_manager_mutex);
#else
        std::unique_lock<std::mutex> lck(stream_manager_mutex);
#endif
        removeStream(streamId);
        return -1;
    }

    printStreamStatus(streamId, stream->getInfo().getStatus());
    return fd;
}

int StreamManager::write(int fd, const void* data, int size) {
    REF_PTR_EP stream;
    {
        // Lock stream_manager_mutex to access the shared Stream map
#ifdef _MIOSIX
        miosix::Lock<miosix::FastMutex> lck(stream_manager_mutex);
#else
        std::unique_lock<std::mutex> lck(stream_manager_mutex);
#endif
        if(!masterTrusted) return -10;
        auto it = fdt.find(fd);
        if(it == fdt.end()) return -1;
        stream = it->second;
    }

    return stream->write(data, size);
}

int StreamManager::read(int fd, void* data, int maxSize) {
    REF_PTR_EP stream;
    {
        // Lock stream_manager_mutex to access the shared Stream map
#ifdef _MIOSIX
        miosix::Lock<miosix::FastMutex> lck(stream_manager_mutex);
#else
        std::unique_lock<std::mutex> lck(stream_manager_mutex);
#endif
        if(!masterTrusted) return -10;
        auto it = fdt.find(fd);
        if(it == fdt.end()) return -1;
        stream = it->second;
    }

    return stream->read(data, maxSize);
}

StreamInfo StreamManager::getInfo(int fd) {
    REF_PTR_EP endpoint;
    {
        // Lock stream_manager_mutex to access the shared Stream/Server map
#ifdef _MIOSIX
        miosix::Lock<miosix::FastMutex> lck(stream_manager_mutex);
#else
        std::unique_lock<std::mutex> lck(stream_manager_mutex);
#endif
        auto it = fdt.find(fd);
        if(it == fdt.end()) return StreamInfo();
        endpoint = it->second;
    }
    return endpoint->getInfo();
}

void StreamManager::close(int fd) {
    REF_PTR_EP endpoint;
    {
        // Lock stream_manager_mutex to access the shared Stream/Server map
#ifdef _MIOSIX
        miosix::Lock<miosix::FastMutex> lck(stream_manager_mutex);
#else
        std::unique_lock<std::mutex> lck(stream_manager_mutex);
#endif
        auto it = fdt.find(fd);
        if(it == fdt.end()) return;
        endpoint = it->second;
    }
    StreamId id = endpoint->getStreamId();
    bool deleted = endpoint->close(this);
    if(deleted) {
        // Lock stream_manager_mutex to access the shared Stream/Server map
#ifdef _MIOSIX
        miosix::Lock<miosix::FastMutex> lck(stream_manager_mutex);
#else
        std::unique_lock<std::mutex> lck(stream_manager_mutex);
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
    REF_PTR_EP server;
    {
        // Lock stream_manager_mutex to access the shared Server map
#ifdef _MIOSIX
        miosix::Lock<miosix::FastMutex> lck(stream_manager_mutex);
#else
        std::unique_lock<std::mutex> lck(stream_manager_mutex);
#endif
        if(!masterTrusted) return -10;
        // Check if a server with these parameters is already present
        if(servers.find(port) != servers.end()) {
            return -1;
        }
        auto res = addServer(serverInfo);
        fd = res.first;
        server = res.second;
    }

    // Make the server wait for an info element confirming LISTEN status
    int error = server->listen(this);
    if(error != 0) {
#ifdef _MIOSIX
        miosix::Lock<miosix::FastMutex> lck(stream_manager_mutex);
#else
        std::unique_lock<std::mutex> lck(stream_manager_mutex);
#endif
        unsigned char port = serverId.dstPort;
        removeServer(port);
        return -1;
    }
    printServerStatus(serverId, server->getInfo().getStatus());
    return fd;
}

int StreamManager::accept(int serverfd) {
    REF_PTR_EP server,stream;
    int fd = -1;
    {
        // Lock stream_manager_mutex to access the shared Server map
#ifdef _MIOSIX
        miosix::Lock<miosix::FastMutex> lck(stream_manager_mutex);
#else
        std::unique_lock<std::mutex> lck(stream_manager_mutex);
#endif
        if(!masterTrusted) return -10;
        auto serverit = fdt.find(serverfd);
        if(serverit == fdt.end()) return -1;
        server = serverit->second;
    }

    fd = server->accept();
    {
        // Lock stream_manager_mutex to access the shared Stream map
#ifdef _MIOSIX
        miosix::Lock<miosix::FastMutex> lck(stream_manager_mutex);
#else
        std::unique_lock<std::mutex> lck(stream_manager_mutex);
#endif
        auto streamit = fdt.find(fd);
        if(streamit == fdt.end()) return -1;
        stream = streamit->second;
    }
    stream->acceptedStream();
    return fd;
}

bool StreamManager::setSendCallback(int fd, std::function<void(void*,unsigned int*)> sendCallback) {
    if (config.getCallbacksExecutionTime() == 0) {
        print_dbg("[S] Stream::setSendCallback: invalid callback execution time\n");
        return false;
    }
    
    // Lock stream_manager_mutex to access the shared Stream map
#ifdef _MIOSIX
    miosix::Lock<miosix::FastMutex> lck(stream_manager_mutex);
#else
    std::unique_lock<std::mutex> lck(stream_manager_mutex);
#endif
    auto it = fdt.find(fd);
    if(it == fdt.end()) return false;
    auto stream = it->second;
    stream->setSendCallback(sendCallback);

    return true;
}

bool StreamManager::setReceiveCallback(int fd, std::function<void(void*,unsigned int*)> recvCallback) {
    if (config.getCallbacksExecutionTime() == 0) {
        print_dbg("[S] Stream::setReceiveCallback: invalid callback execution time\n");
        return false;
    }
    
    // Lock stream_manager_mutex to access the shared Stream map
#ifdef _MIOSIX
    miosix::Lock<miosix::FastMutex> lck(stream_manager_mutex);
#else
    std::unique_lock<std::mutex> lck(stream_manager_mutex);
#endif
    auto it = fdt.find(fd);
    if(it == fdt.end()) return false;
    auto stream = it->second;
    stream->setReceiveCallback(recvCallback);

    return true;
}

void StreamManager::periodicUpdate() {
    // Lock stream_manager_mutex to access the shared Stream/Server map
#ifdef _MIOSIX
    miosix::Lock<miosix::FastMutex> lck(stream_manager_mutex);
#else
    std::unique_lock<std::mutex> lck(stream_manager_mutex);
#endif
    // Iterate over all streams
    for(auto& stream: streams) {
        REF_PTR_STREAM s = stream.second;
        s->periodicUpdate(this);
    }
    // Iterate over all servers
    for(auto& server: servers) {
        REF_PTR_SERVER s = server.second;
        s->periodicUpdate(this);
    }
}

bool StreamManager::receivePacket(StreamId id, const Packet& data) {
    REF_PTR_STREAM stream;
    {
        // Lock stream_manager_mutex to access the shared Stream map
#ifdef _MIOSIX
        miosix::Lock<miosix::FastMutex> lck(stream_manager_mutex);
#else
        std::unique_lock<std::mutex> lck(stream_manager_mutex);
#endif
        auto streamit = streams.find(id);
        if(streamit == streams.end()) return false;
        stream = streamit->second;
    }
    return stream->receivePacket(data);
}

bool StreamManager::missPacket(StreamId id) {
    REF_PTR_STREAM stream;
    {
        // Lock stream_manager_mutex to access the shared Stream map
#ifdef _MIOSIX
        miosix::Lock<miosix::FastMutex> lck(stream_manager_mutex);
#else
        std::unique_lock<std::mutex> lck(stream_manager_mutex);
#endif
        auto streamit = streams.find(id);
        if(streamit == streams.end()) return false;
        stream = streamit->second;
    }
    return stream->missPacket();
}

bool StreamManager::sendPacket(StreamId id, Packet& data) {
    REF_PTR_STREAM stream;
    {
        // Lock stream_manager_mutex to access the shared Stream map
#ifdef _MIOSIX
        miosix::Lock<miosix::FastMutex> lck(stream_manager_mutex);
#else
        std::unique_lock<std::mutex> lck(stream_manager_mutex);
#endif
        // Check if a stream with these parameters is not present
        auto streamit = streams.find(id);
        if(streamit == streams.end()) return false;
        stream = streamit->second;
    }
    return stream->sendPacket(data);
}

void StreamManager::setSchedule(const std::vector<ScheduleElement>& schedule, unsigned int activationTile) {
#ifdef _MIOSIX
    miosix::Lock<miosix::FastMutex> lck(stream_manager_mutex);
#else
    std::unique_lock<std::mutex> lck(stream_manager_mutex);
#endif
    /**
     * Start computing the vector of old streams that will be removed at
     * schedule application.
     */
    streamsToRemove.clear();
    for (auto it=streams.begin(); it!=streams.end(); ++it)
        streamsToRemove.push_back(it->first);

    nextScheduleStreams.clear();
    for(auto& element : schedule) {
        StreamId streamId = element.getStreamId();
        // NOTE: Ignore streams of which we are not source or destination 
        if(streamId.src != myId && streamId.dst != myId)
            continue;

        nextScheduleStreams.insert(streamId);

        auto streamit = streams.find(streamId);
        if(streamit != streams.end()) {
            streamsToRemove.erase(std::remove(streamsToRemove.begin(),
                                             streamsToRemove.end(),
                                             streamit->first),
                                 streamsToRemove.end());
        } else {
            /**
             * Adding streams that were not present in the previous schedule,
             * which is not being applied yet.
             */
            StreamParameters params = element.getParams();
            StreamId serverId = streamId.getServerId();
            unsigned char port = serverId.dstPort;
            auto serverit = servers.find(port);
            // If the corresponding server is present, create new stream in
            // ACCEPT_WAIT status and register it in corresponding server
            if(serverit != servers.end() &&
               (serverit->second->getInfo().getStatus() == StreamStatus::LISTEN)) {
                StreamInfo streamInfo(streamId, params, StreamStatus::ACCEPT_WAIT);
                auto res = addStream(streamInfo);
                auto server = serverit->second;
                server->addPendingStream(res.second);
            }
            // If the corresponding server is not present,
            else {
                // 1. Create new stream in CLOSE_WAIT status
                StreamInfo streamInfo(streamId, params, StreamStatus::CLOSE_WAIT);
                auto res = addStream(streamInfo);
                // NOTE: Make sure that stream enqueues a CLOSED SME
                res.second->close(this);
                // 2. Create new server in CLOSE_WAIT status
                StreamInfo serverInfo(serverId, params, StreamStatus::CLOSE_WAIT);
                auto res2 = addServer(serverInfo);
                // NOTE: Make sure that the server enqueues a CLOSED SME
                res2.second->close(this);
            }
        }
    }
#ifdef CRYPTO
    /**
     * If crypto is on, we start rekeying AFTER adding the new streams from
     * the received schedule. This way doStartRekeying will copy the value of
     * rekeyingSnapshot containing new streams only, and doContinueRekeying
     * will compute new keys for all of them.
     * 
     */
    if (config.getAuthenticateDataMessages()) {
        doStartRekeying();
    }
#endif

    waitScheduler.setScheduleActivationTile(activationTile);
}

void StreamManager::setStreamsWakeupLists(const std::vector<StreamWakeupInfo>& currList,
                                            const std::vector<StreamWakeupInfo>& nextList) {
    waitScheduler.setStreamsWakeupLists(currList, nextList);
}

void StreamManager::applySchedule(const std::vector<ScheduleElement>& schedule) {
#ifdef _MIOSIX
    miosix::Lock<miosix::FastMutex> lck(stream_manager_mutex);
#else
    std::unique_lock<std::mutex> lck(stream_manager_mutex);
#endif

    for(auto& element : schedule) {
        StreamId streamId = element.getStreamId();
        // NOTE: Ignore streams of which we are not source or destination 
        if(streamId.src != myId && streamId.dst != myId)
            continue;
        auto streamit = streams.find(streamId);
        /**
         * Here we handle streams that were already present in the previous
         * schedule, but are changing status or parameters.
         */
        if(streamit != streams.end()) {
            auto stream = streamit->second;
            // NOTE: Update stream parameters,
            // they may have changed after negotiation with server
            stream->addedStream(element.getParams());
        }
    }
    // If stream present in map and not in schedule, call removedStream()
    for(auto& streamId : streamsToRemove) {
        // If return value is true, we can delete the Stream class
        if(streams[streamId]->removedStream())
            removeStream(streamId);
    }
    streamsToRemove.clear();

    // Reset redundancy counters and sequence numbers to avoid errors
    // NOTE: this measure works by assuming that when a new schedule begins
    // all redundancy counters should be set to zero
    for(auto& stream : streams) {
        REF_PTR_STREAM s = stream.second;
        s->resetCounters();
    }

#ifdef CRYPTO
    if(config.getAuthenticateDataMessages()) {
        doApplyRekeying();
    }
#endif
}

void StreamManager::applyInfoElements(const std::vector<InfoElement>& infos) {
    // Lock stream_manager_mutex to access the shared Stream/Server map
#ifdef _MIOSIX
    miosix::Lock<miosix::FastMutex> lck(stream_manager_mutex);
#else
    std::unique_lock<std::mutex> lck(stream_manager_mutex);
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
                InfoType type = info.getInfoType();
                if(type==InfoType::SERVER_OPENED)
                    server->acceptedServer();
                else if(type==InfoType::SERVER_CLOSED)
                    server->rejectedServer();
            }
            // If server is not present
            else if(id.isServer() && info.getInfoType() == InfoType::SERVER_OPENED) {
                // Create server in CLOSE_WAIT to warn the master node
                // that this server is actually closed
                StreamInfo serverInfo(id, info.getParams(), StreamStatus::CLOSE_WAIT);
                auto res = addServer(serverInfo);
                // NOTE: Make sure that the server enqueues a CLOSED SME
                res.second->close(this);
            }
        }
        else {
            auto streamit = streams.find(id);
            // If stream is present in map
            if(streamit != streams.end()) {
                auto stream = streamit->second;
                InfoType type = info.getInfoType();
                if(type==InfoType::STREAM_REJECT)
                    stream->rejectedStream();
            }
        }
    }
}

void StreamManager::dequeueSMEs(UpdatableQueue<SMEKey,StreamManagementElement>& queue) {
    // Lock stream_manager_mutex to access the shared SME queue
#ifdef _MIOSIX
    miosix::Lock<miosix::FastMutex> lck(sme_mutex);
#else
    std::unique_lock<std::mutex> lck(sme_mutex);
#endif
    while(!smeQueue.empty()) {
        auto temp = smeQueue.dequeue();
        SMEKey tempKey = temp.getKey();
        queue.enqueue(tempKey, std::move(temp));
    }
}

void StreamManager::enqueueSME(StreamManagementElement sme) {
    SMEKey key = sme.getKey();
#ifdef WITH_SME_SEQNO
    if (ENABLE_STREAM_MGR_INFO_DBG) 
        print_dbg("Enqueueing %s, seq=%d\n", smeTypeToString(sme.getType()), sme.getSeqNo());
#endif
    {
        // Lock stream_manager_mutex to access the shared SME queue
#ifdef _MIOSIX
        miosix::Lock<miosix::FastMutex> lck(sme_mutex);
#else
        std::unique_lock<std::mutex> lck(sme_mutex);
#endif
        smeQueue.enqueue(key, sme);
    }
}

void StreamManager::resetSequenceNumbers() {
#ifdef _MIOSIX
    miosix::Lock<miosix::FastMutex> lck(stream_manager_mutex);
#else
    std::unique_lock<std::mutex> lck(stream_manager_mutex);
#endif

    for(auto& s : streams) {
        REF_PTR_STREAM stream = s.second;
        stream->resetSequenceNumber();
    }
}

unsigned long long StreamManager::getSequenceNumber(StreamId id) {
    REF_PTR_STREAM stream;

#ifdef _MIOSIX
    miosix::Lock<miosix::FastMutex> lck(stream_manager_mutex);
#else
    std::unique_lock<std::mutex> lck(stream_manager_mutex);
#endif

    auto it = streams.find(id);
    if(it == streams.end()) {
        print_dbg("BUG: stream does not exist!\n");
        return 0;
    } else {
        stream = it->second;
        return stream->getSequenceNumber();
    }
}

#ifdef CRYPTO
AesOcb& StreamManager::getStreamOCB(StreamId id) {
    REF_PTR_STREAM stream;

#ifdef _MIOSIX
    miosix::Lock<miosix::FastMutex> lck(stream_manager_mutex);
#else
    std::unique_lock<std::mutex> lck(stream_manager_mutex);
#endif
    auto it = streams.find(id);
    if(it == streams.end()) {
        printf("BUG: Stream not present in StreamManager!\n");
        return emptyOCB;
    }
    stream = it->second;
    return it->second->getOCB();
}

void StreamManager::startRekeying() {
    if (config.getAuthenticateDataMessages()) {
#ifdef _MIOSIX
        miosix::Lock<miosix::FastMutex> lck(stream_manager_mutex);
#else
        std::unique_lock<std::mutex> lck(stream_manager_mutex);
#endif
        doStartRekeying();
    }
}

void StreamManager::continueRekeying() {
    if (config.getAuthenticateDataMessages()) {
#ifdef _MIOSIX
        miosix::Lock<miosix::FastMutex> lck(stream_manager_mutex);
#else
        std::unique_lock<std::mutex> lck(stream_manager_mutex);
#endif
        doContinueRekeying();
    }
}

void StreamManager::applyRekeying() {
    if (config.getAuthenticateDataMessages()) {
#ifdef _MIOSIX
        miosix::Lock<miosix::FastMutex> lck(stream_manager_mutex);
#else
        std::unique_lock<std::mutex> lck(stream_manager_mutex);
#endif
        doApplyRekeying();
    }
}

bool StreamManager::needToContinueRekeying() {
    // No need to lock mutex as these bools are never changed by other threads
    if(!rekeyingInProgress) return false;
    else return !rekeyingSnapshot.empty();
}

#endif //ifdef CRYPTO

int StreamManager::allocateClientPort() {
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

std::pair<int,REF_PTR_STREAM> StreamManager::addStream(StreamInfo streamInfo) {
    int fd = fdcounter++;
    REF_PTR_STREAM stream = REF_PTR_STREAM (new Stream(config, fd, streamInfo)); 
    StreamId streamId = streamInfo.getStreamId();

    streams[streamId] = stream;
    fdt[fd] = stream;

    return std::make_pair(fd,stream);
}

std::pair<int,REF_PTR_SERVER> StreamManager::addServer(StreamInfo serverInfo) {
    int fd = fdcounter++;
    REF_PTR_SERVER server = REF_PTR_SERVER (new Server(config, fd, serverInfo));
    unsigned char port = serverInfo.getStreamId().dstPort;
    servers[port] = server;
    fdt[fd] = server;
    return std::make_pair(fd,server);
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
    printf("[SM] Stream (%d,%d,%d,%d): ", id.src,id.dst,
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
    printf("[SM] Server (%d,%d): ", id.dst,id.dstPort);
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

#ifdef CRYPTO
void StreamManager::doStartRekeying() {
    if (rekeyingInProgress) {
        print_dbg("N=%d BUG: call to doStartRekeying while rekeying is still in progress\n",
                  myId);
    }
    rekeyingInProgress = true;
    rekeyingSnapshot = std::queue<StreamId>();
    for (auto s : nextScheduleStreams) {
        rekeyingSnapshot.push(s);
    }
}

void StreamManager::doContinueRekeying() {
    if (!rekeyingInProgress) {
        print_dbg("N=%d BUG: call to doContinueRekeying without starting rekeying first\n",
                  myId);
    }
    unsigned i=0;
    while (i < maxHashesPerSlot && !rekeyingSnapshot.empty()) {
        StreamId id = rekeyingSnapshot.front();
        rekeyingSnapshot.pop();
        /* precompute rekeying for this stream */
        auto it = streams.find(id);
        if (it != streams.end()) {
            unsigned char streamIdBlock[16] = {0};
            unsigned char newKey[16];
            memcpy(streamIdBlock, &id, sizeof(StreamId));
            secondBlockStreamHash.digestBlock(newKey, streamIdBlock);

            REF_PTR_STREAM stream = it->second;
            it->second->setNewKey(newKey);
            i++; 
        }
    }
}

void StreamManager::doApplyRekeying() {
    if (!rekeyingInProgress) {
        print_dbg("N=%d BUG: call to doApplyRekeying without starting rekeying first\n",
                  myId);
    }

    /* reset all sequence numbers */
    for(auto& s : streams) {
        REF_PTR_STREAM stream = s.second;
        stream->resetSequenceNumber();
    }
    /* at this point, all streams have had their new key computed */
    for (auto& s: streams) {
        REF_PTR_STREAM stream = s.second;
        s.second->applyNewKey();
    }
    rekeyingInProgress = false;
}
#endif // #ifdef CRYPTO

} /* namespace mxnet */
