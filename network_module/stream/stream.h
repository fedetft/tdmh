/***************************************************************************
 *   Copyright (C)  2019 by Federico Amedeo Izzo                           *
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

#pragma once

#include "../tdmh.h"
#include "../util/packet.h"
#include "stream_management_element.h"
#include <set>
#ifdef _MIOSIX
#include <miosix.h>
#include <kernel/intrusive.h>
#else
#include <memory>
#include <mutex>
#include <condition_variable>
// Implement IntrusiveRefCounted using shared_ptr
class IntrusiveRefCounted {};
#define intrusive_ref_ptr shared_ptr
#endif

namespace mxnet {

class StreamManager;

/**
 * The class Endpoint is the parent class of Stream and Server
 * Classes of this type are reference counted, must be allocated on the heap
 * and managed through intrusive_ref_ptr<FileBase>
 */

class Endpoint : public IntrusiveRefCounted {
    Endpoint(StreamInfo info) : info(info);

    // Used by derived class Stream 
    int connect(StreamManager* mgr) {
        //This method should never be called on the base class
        return -1;
    }
    // Used by derived class Stream 
    int write(const void* data, int size) {
        //This method should never be called on the base class
        return -1;
    }
    // Used by derived class Stream 
    int read(void* data, int maxSize) {
        //This method should never be called on the base class
        return -1;
    }
    // Used by derived class Stream 
    void putPacket(const Packet& data) = 0;
    // Used by derived class Stream 
    void getPacket(Packet& data) = 0;
    // Used by derived class Stream 
    void addedStream() = 0;
    // Used by derived class Stream 
    bool removedStream() = 0;
    // Used by derived class Stream 
    void rejectedStream() = 0;
    // Used by derived class Server
    int listen() {
        //This method should never be called on the base class
        return -1;
    }
    // Used by derived class Server
    int accept() {
        //This method should never be called on the base class
        return -1;
    }
    // Used by derived class Server
    int addPendingStream(Stream* stream) {
        //This method should never be called on the base class
        return -1;
    }
    // Used by derived class Server
    int acceptedServer() {
        //This method should never be called on the base class
        return -1;
    }
    // Used by derived class Server
    int rejectedServer() {
        //This method should never be called on the base class
        return -1;
    }
    // Used by derived class Stream and Server 
    StreamId getStreamId() {
        return info.getStreamId();
    }
    // Used by derived class Stream and Server 
    StreamId getStatus() {
        return info.getStatus();
    }
    // Used by derived class Stream and Server
    StreamInfo getInfo() {
        return info;
    }
    // Used by derived class Stream and Server
    bool close(StreamManager* mgr) = 0;
    // Used by derived class Stream and Server
    void periodicUpdate(StreamManager* mgr) = 0;
    // Used by derived class Stream and Server
    bool desync() = 0;

private:
    // Change the status saved in StreamInfo
    void setStatus(StreamStatus status) {
        info.setStatus(status);
    }

    const int maxTimeout = 3;
    /* Contains information of the endpoint and state machine status */
    StreamInfo info;
    /* Used to send SME every N periodic updates */
    int timeout;
}

/**
 * The class Stream represents one of the two endpoint of a stream connection
 */
class Stream {
    Stream(StreamInfo info) : Endpoint(info);

    // Called by StreamManager after creation,
    // used to send CONNECT SME and wait for addedStream()
    int connect(StreamManager* mgr);

    // Called by StreamAPI, to put in sendBuffer data to be sent
    int write(const void* data, int size);

    // Called by StreamAPI, to get from recvBuffer received data
    int read(void* data, int maxSize);

    // Called by StreamManager, to put data to recvBuffer
    int putPacket(const Packet& data);

    // Called by StreamManager, to get data from sendBuffer
    int getPacket(Packet& data);

    // Called by StreamManager when this stream is present in a received schedule
    void addedStream();

    // Called by StreamManager when this stream is NOT present in a received schedule
    // Returns true if the Stream class can be deleted
    bool removedStream();

    // Called by StreamManager when a STREAM_REJECT info element is received
    void rejectedStream();

    void closedServer();

    // Called by StreamAPI, to close the stream on the user side
    // Returns true if the Stream class can be deleted
    bool close(StreamManager* mgr);

    // Called by StreamManager, in a periodic way to allow resending SME
    void periodicUpdate(StreamManager* mgr);

    // Called by StreamManager when the Timesync desynchronizes, used to
    // close the stream system-side in certain conditions
    // Returns true if the Stream class can be deleted
    bool desync();

    Server* myServer;
    Packet sendBuffer;
    Packet recvBuffer;
    /* Redundancy Info */
    unsigned char timesSent = 0;
    unsigned char timesRecv = 0;
    /* Thread synchronization */
#ifdef _MIOSIX
    // Protects concurrent access at StreamInfo
    miosix::FastMutex status_mutex;
    // Protects concurrent access at sendBuffer
    miosix::FastMutex send_mutex;
    // Protects concurrent access at recvBuffer
    miosix::FastMutex recv_mutex;
    miosix::ConditionVariable connect_cv;
    miosix::ConditionVariable send_cv;
    miosix::ConditionVariable recv_cv;
#else
    std::mutex status_mutex;
    std::mutex send_mutex;
    std::mutex recv_mutex;
    std::condition_variable connect_cv;
    std::condition_variable send_cv;
    std::condition_variable recv_cv;
#endif
};

/**
 * The class Server is created to comunicate the possibility to accept incoming
 * Streams
 */
class Server {
    Server(StreamInfo info) : Endpoint(info);

    // Called by StreamManager after creation,
    // used to send LISTEN SME and wait for acceptedServer()
    int listen(StreamManager* mgr);

    // Called by StreamAPI, to get or wait for a new incoming stream
    int accept();

    // Called by StreamManager, used to add a Stream to the list of
    // streams waiting for an accept
    int addPendingStream(int fd);

    // Called by StreamManager when a SERVER_ACCEPT info element is received
    int acceptedServer();

    // Called by StreamManager when a SERVER_REJECT info element is received
    int rejectedServer();

    // Called by StreamAPI, to close the server on the user side
    // Returns true if the Server class can be deleted
    // NOTE: closing the server also removes the pending streams
    bool close(StreamManager* mgr);

    // Called by StreamManager, in a periodic way to allow resending SME
    void periodicUpdate(StreamManager* mgr);

    // Called by StreamManager when the Timesync desynchronizes, used to
    // close the server system-side in certain conditions
    // Returns true if the Server class can be deleted
    void desync();

    // Contains fd of streams not yet accepted
    // It's a set to avoid duplicates
    std::set<fd> pendingAccept;
    /* Thread synchronization */
#ifdef _MIOSIX
    miosix::FastMutex status_mutex;
    miosix::ConditionVariable listen_cv;
#else
    std::mutex status_mutex;
    std::condition_variable listen_cv;
#endif

}

} /* namespace mxnet */
