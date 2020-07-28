/***************************************************************************
 *   Copyright (C) 2019 by Federico Amedeo Izzo                            *
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

#include "../network_configuration.h"
#include "../tdmh.h"
#include "../util/packet.h"
#include "stream_management_element.h"
#ifdef CRYPTO
#include "../crypto/hash.h"
#include "../crypto/aes_ocb.h"
#endif
#include <list>
#ifdef _MIOSIX
#include <miosix.h>
#include <kernel/intrusive.h>
#else
#include <mutex>
#include <condition_variable>
#include <memory>

// Enable this to check that second and third redundant packet received
// are equal to the first one
// NOTE: This is a heavy debug check, disable if not necessary
#define REDUNDANCY_DEBUG_CHECK

// Implement IntrusiveRefCounted using shared_ptr
namespace miosix {
class IntrusiveRefCounted {};
}
#endif

namespace mxnet {

class StreamManager;
class Endpoint;
class Stream;
class Server;

#ifdef _MIOSIX
using REF_PTR_EP     = miosix::intrusive_ref_ptr<Endpoint>;
using REF_PTR_STREAM = miosix::intrusive_ref_ptr<Stream>;
using REF_PTR_SERVER = miosix::intrusive_ref_ptr<Server>;
#else
using REF_PTR_EP     = std::shared_ptr<Endpoint>;
using REF_PTR_STREAM = std::shared_ptr<Stream>;
using REF_PTR_SERVER = std::shared_ptr<Server>;
#endif

/**
 * The class Endpoint is the parent class of Stream and Server
 * Classes of this type are reference counted, must be allocated on the heap
 * and managed through intrusive_ref_ptr<FileBase>
 */

class Endpoint : public miosix::IntrusiveRefCounted {
public:
    Endpoint(const NetworkConfiguration& config,
             int fd, StreamInfo info) : fd(fd), info(info), smeTimeout(smeTimeoutMax),
                                        failTimeout(failTimeoutMax) {};
    virtual ~Endpoint() {};

    // Used by derived class Stream 
    virtual int connect(StreamManager* mgr) {
        //This method should never be called on the base class
        return -1;
    }
    // Used by derived class Stream 
    virtual int write(const void* data, int size) {
        //This method should never be called on the base class
        return -1;
    }
    // Used by derived class Stream 
    virtual int read(void* data, int maxSize) {
        //This method should never be called on the base class
        return -1;
    }
    // TODO: The base class implementation of these functions should throw an error?
    // Used by derived class Stream 
    virtual bool receivePacket(const Packet& data) { return false; }
    // Used by derived class Stream
    virtual bool missPacket() { return false; }
    // Used by derived class Stream 
    virtual bool sendPacket(Packet& data) { return false; }
    // Used by derived class Stream 
    virtual void addedStream(StreamParameters newParams) {}
    // Used by derived class Stream
    virtual void acceptedStream() {}
    // Used by derived class Stream 
    virtual bool removedStream() { return false; }
    // Used by derived class Stream 
    virtual void rejectedStream() {}
    // Used by derived class Stream 
    virtual void closedServer(StreamManager* mgr) {}
    // Used by derived class Server
    virtual int listen(StreamManager* mgr) {
        //This method should never be called on the base class
        return -1;
    }
    // Used by derived class Server
    virtual int accept() {
        //This method should never be called on the base class
        return -1;
    }
    // Used by derived class Server
    virtual void acceptedServer() {}
    // Used by derived class Server
    virtual void rejectedServer() {}
    // Used by derived class Stream and Server 
    StreamId getStreamId() {
        return info.getStreamId();
    }
    // Used by StreamManager
    // NOTE: do not use in Stream or Server to avoid double mutex lock
    StreamInfo getInfo() {
        // Lock mutex for concurrent access at StreamInfo
#ifdef _MIOSIX
        miosix::Lock<miosix::FastMutex> lck(status_mutex);
#else
        std::unique_lock<std::mutex> lck(status_mutex);
#endif
        return info;
    }
    // Used by StreamManager removeServer() and removeStream()
    int getFd() {
        return fd;
    }
    // Used by derived class Stream and Server
    virtual bool close(StreamManager* mgr) = 0;
    // Used by derived class Stream and Server
    virtual void periodicUpdate(StreamManager* mgr) = 0;
    // Used by derived class Stream and Server
    virtual bool desync() = 0;

protected:
    // Change the status saved in StreamInfo
    void setStatus(StreamStatus status) {
        info.setStatus(status);
        // NOTE: Reset the sme and fail timeouts after state change
        resetTimeouts();
    }
    // Used by derived class Stream and Server
    void resetTimeouts() {
        smeTimeout = smeTimeoutMax;
        failTimeout = failTimeoutMax;
    }

    // TODO:Integrate these constants in NetworkConfiguration
    /* Constants defining the maximum value of the sme and fail timeout
     * In number of tiles */
    const int smeTimeoutMax = 600;
    const int failTimeoutMax = 1800;

    /* Copy of the file descriptor number, useful for deleting object from fdt */
    int fd;
    /* Contains information of the endpoint and state machine status */
    StreamInfo info;
    /* Used to send SME every N periodic updates */
    int smeTimeout;
    int failTimeout;
        /* Thread synchronization */
#ifdef _MIOSIX
    // Protects concurrent access at StreamInfo
    miosix::FastMutex status_mutex;
#else
    std::mutex status_mutex;
#endif
};

/**
 * The class Stream represents one of the two endpoint of a stream connection
 */
class Stream : public Endpoint {
public:
#ifdef CRYPTO
    Stream(const NetworkConfiguration& config, int fd, StreamInfo info,
                                                 const unsigned char key[16]) :
            Endpoint(config, fd, info), panId(config.getPanId()),
            ocb(key), authData(config.getAuthenticateDataMessages())
    {
        updateRedundancy();
    }

    Stream(const NetworkConfiguration& config, int fd, StreamInfo info) :
            Endpoint(config, fd, info), panId(config.getPanId()),
            authData(config.getAuthenticateDataMessages()) 
    {
        updateRedundancy();
    }
#else
    Stream(const NetworkConfiguration& config, int fd, StreamInfo info) :
            Endpoint(config, fd, info), panId(config.getPanId())
    {
        updateRedundancy();
    }
#endif

    // Called by StreamManager after creation,
    // used to send CONNECT SME and wait for addedStream()
    int connect(StreamManager* mgr) override;

    // Called by StreamAPI, to put in sendBuffer data to be sent
    int write(const void* data, int size) override;

    // Called by StreamAPI, to get from recvBuffer received data
    int read(void* data, int maxSize) override;

    // Called by StreamManager, to put data to recvBuffer
    // Return true at the end of each period
    bool receivePacket(const Packet& data) override;

    // Called by StreamManager, when we missed an inbound packet
    // Return true if we have data to send
    bool missPacket() override;

    // Called by StreamManager, to get data from sendBuffer
    // Return true if we have data to send
    bool sendPacket(Packet& data) override;

    // Called by StreamManager when this stream is present in a received schedule
    void addedStream(StreamParameters newParams) override;

    // Called by StreamManager to change status of stream, after it has been accepted
    void acceptedStream() override;

    // Called by StreamManager when this stream is NOT present in a received schedule
    // Returns true if the Stream class can be deleted
    bool removedStream() override;

    // Called by StreamManager when a STREAM_REJECT info element is received
    void rejectedStream() override;

    // Called by StreamManager when the corresponding server is closed
    // It puts the status in CLOSE_WAIT if the stream was in ACCEPT_WAIT
    void closedServer(StreamManager* mgr) override;

    // Called by StreamAPI, to close the stream on the user side
    // Returns true if the Stream class can be deleted
    bool close(StreamManager* mgr) override;

    // Called by StreamManager, in a periodic way to allow resending SME
    void periodicUpdate(StreamManager* mgr) override;

    // Called by StreamManager after applying a new schedule
    // Resets the redundancy counters to avoid errors
    void resetCounters() {
        txCount = 0;
        rxCount = 0;
    }

    void resetSequenceNumber() {
        seqNo = 1;
    }

    unsigned long long getSequenceNumber() { return seqNo; }

    // Called by StreamManager when the Timesync desynchronizes, used to
    // close the stream system-side in certain conditions
    // Returns true if the Stream class can be deleted
    bool desync() override;

#ifdef CRYPTO
    void setNewKey(const unsigned char newKey[16]) {
        AesOcb tmp(newKey);
        ocb_next = std::move(tmp);
    }

    void applyNewKey() { 
        ocb = std::move(ocb_next);
    }

    AesOcb& getOCB() { return ocb; }
#endif


private:
    const unsigned short panId;

    Packet txPacket;
    Packet rxPacket;
    /* Cached Redundancy Info */
    Redundancy redundancy;
    unsigned int redundancyCount = 0;
    /* Redundancy Counters */
    unsigned char txCount = 0;
    unsigned char rxCount = 0;
    bool received = false;
    bool txPacketReady = false;
    /* Variables shared with the application thread */
    bool receivedShared = false;
    // NOTE: make sure that the first read waits for data to be present
    bool alreadyReceivedShared = true;
    bool nextTxPacketReady = false;
    Packet rxPacketShared;
    Packet nextTxPacket;


    unsigned long long seqNo = 1;

#ifdef CRYPTO
    AesOcb ocb;
    AesOcb ocb_next;
    const bool authData;
#endif

    /* Thread synchronization */
#ifdef _MIOSIX
    // Protects concurrent access at sendBuffer
    miosix::FastMutex tx_mutex;
    // Protects concurrent access at recvBuffer
    miosix::FastMutex rx_mutex;
    miosix::ConditionVariable connect_cv;
    miosix::ConditionVariable tx_cv;
    miosix::ConditionVariable rx_cv;
#else
    std::mutex tx_mutex;
    std::mutex rx_mutex;
    std::condition_variable connect_cv;
    std::condition_variable tx_cv;
    std::condition_variable rx_cv;
#endif

    // Called by Stream itself, used to update cached redundancy info
    void updateRedundancy();
    // Called by Stream itself, when the stream status changes and we need to wake up
    // the write and read methods
    void wakeWriteRead();
    // Called by Stream::receivedPacket(), Stream::missedPacket(),
    // Used to update internal variables every stream period
    // Return true at the end of each period
    bool updateRxPacket();
    // Called by Stream::sendPacket() on the first period, and
    // at the end of every period.
    // Used to update txPacket, the packet being sent
    void updateTxPacket();
};

/**
 * The class Server is created to comunicate the possibility to accept incoming
 * Streams
 */
class Server : public Endpoint {
public:
    Server(const NetworkConfiguration& config,
           int fd, StreamInfo info) : Endpoint(config, fd, info) {};

    // Called by StreamManager after creation,
    // used to send LISTEN SME and wait for acceptedServer()
    int listen(StreamManager* mgr) override;

    // Called by StreamAPI, to get or wait for a new incoming stream
    int accept() override;

    // Called by StreamManager, used to add a Stream to the list of
    // streams waiting for an accept
    void addPendingStream(REF_PTR_STREAM stream);

    // Called by StreamManager when a SERVER_ACCEPT info element is received
    void acceptedServer() override;

    // Called by StreamManager when a SERVER_REJECT info element is received
    void rejectedServer() override;

    // Called by StreamAPI, to close the server on the user side
    // Returns true if the Server class can be deleted
    // NOTE: closing the server also removes the pending streams
    bool close(StreamManager* mgr) override;

    // Called by StreamManager, in a periodic way to allow resending SME
    void periodicUpdate(StreamManager* mgr) override;

    // Called by StreamManager when the Timesync desynchronizes, used to
    // close the server system-side in certain conditions
    // Returns true if the Server class can be deleted
    bool desync() override;

private:
    // Contains streams not yet accepted
    std::list<REF_PTR_STREAM> pendingAccept;
    /* Thread synchronization */
#ifdef _MIOSIX
    miosix::ConditionVariable listen_cv;
#else
    std::condition_variable listen_cv;
#endif

};

} /* namespace mxnet */
