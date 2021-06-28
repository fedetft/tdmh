/***************************************************************************
 *   Copyright (C) 2017-2019 by Terraneo Federico, Polidori Paolo,         *
 *   Federico Amedeo Izzo                                                  *
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

#include "tdmh.h"
#include "mac_context.h"
#ifdef _MIOSIX
/* Global variable containing a pointer to StreamManager, to use the StreamAPI */
static mxnet::StreamManager* sm = nullptr;
static inline mxnet::StreamManager* getStreamManager() { return sm; }
static inline void setStreamManager(mxnet::StreamManager* streamManager) {
    sm = streamManager;
}
#endif

namespace mxnet {

// NOTE: This Stream API can be used only in Miosix, not in simulation
// because in simulation the various nodes will end up using a single
// StreamManager because of the global variable shared between threads
#ifdef _MIOSIX
int waitForMasterTrusted() {
    StreamManager* streamManager = getStreamManager();
    if(streamManager == nullptr)
        return -1;
    streamManager->waitForMasterTrusted();
    return 0;
}

int connect(unsigned char dst, unsigned char dstPort, StreamParameters params) {
    StreamManager* streamManager = getStreamManager();
    if(streamManager == nullptr)
        return -1;
    return streamManager->connect(dst, dstPort, params);
}

int connect(unsigned char dst, unsigned char dstPort, StreamParameters params,
                       std::function<void(void*,unsigned int*)> sendCallback) {
    StreamManager* streamManager = getStreamManager();
    if(streamManager == nullptr)
        return -1;
    return streamManager->connect(dst, dstPort, params, sendCallback);
}

int write(int fd, const void* data, int size) {
    StreamManager* streamManager = getStreamManager();
    if(streamManager == nullptr)
        return -1;
    return streamManager->write(fd, data, size);
}

int read(int fd, void* data, int maxSize) {
    StreamManager* streamManager = getStreamManager();
    if(streamManager == nullptr)
        return -1;
    return streamManager->read(fd, data, maxSize);
}

StreamInfo getInfo(int fd) {
    StreamManager* streamManager = getStreamManager();
    if(streamManager == nullptr)
        return StreamInfo();
    return streamManager->getInfo(fd);
}

void close(int fd) {
    StreamManager* streamManager = getStreamManager();
    if(streamManager != nullptr)
        streamManager->close(fd);
}

int listen(unsigned char port, StreamParameters params) {
    StreamManager* streamManager = getStreamManager();
    if(streamManager == nullptr)
        return -1;
    return streamManager->listen(port, params);
}

int accept(int serverfd) {
    StreamManager* streamManager = getStreamManager();
    if(streamManager == nullptr)
        return -1;
    return streamManager->accept(serverfd);
}

int accept(int serverfd, std::function<void(void*,unsigned int*)> recvCallback) {
    StreamManager* streamManager = getStreamManager();
    if(streamManager == nullptr)
        return -1;
    return streamManager->accept(serverfd, recvCallback);
}
#endif

MediumAccessController::~MediumAccessController() {
    delete ctx;
}

void MediumAccessController::run() {
    try{
        async = false;
#ifdef _MIOSIX
        // Set StreamManager* global variable
        setStreamManager(ctx->getStreamManager());
#endif
        ctx->run();
    }
    catch(...){
        throw;
    }
}

void MediumAccessController::runAsync() {
    async = true;
#ifdef _MIOSIX
    thread = miosix::Thread::create(MediumAccessController::runLauncher, 2048, miosix::PRIORITY_MAX-1, this, miosix::Thread::JOINABLE);
#else
    thread = new std::thread(&MediumAccessController::run, this);
#endif
}

void MediumAccessController::stop() {
    ctx->stop();
    if (async) thread->join();
}
}
