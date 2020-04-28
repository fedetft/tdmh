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
// For StreamId
#include "stream_management_element.h"
#include "stream.h"
#include "../scheduler/schedule_element.h"
#include "../util/updatable_queue.h"
// For cryptography
#ifdef CRYPTO
#include "../crypto/hash.h"
#include "../crypto/aes_gcm.h"
#include <queue>
#endif
// For thread synchronization
#ifdef _MIOSIX
#include <miosix.h>
#include <kernel/intrusive.h>
#else
#include <mutex>
#include <memory>
#endif
#include <map>
#include <queue>

namespace mxnet {

/**
 * The class StreamManager contains Stream and Server classes related
 * to the node it is running on.
 * The Stream classes represents the endpoints of a connection, while
 * the Server class is used to comunicate to the network the availability
 * of opening streams.
 * Its methods are called by classes UplinkPhase, DataPhase, StreamAPI
 * NOTE: the methods of this class are protected by a mutex,
 * Do not call one method from another! or you will get a deadlock.
 */

class StreamManager {
public:
    StreamManager(const NetworkConfiguration& config, unsigned char myId) : config(config), myId(myId) {
        // Inizialize clientPorts to false (all ports unused)
        clientPorts.reserve(maxPorts);
        for (unsigned int i = 0; i < maxPorts; ++i) {
            clientPorts.push_back(false);
        }
    }

    ~StreamManager() {
        desync();
    }

    /**
     * Update the internal status of the StreamManager after restoring
     * the Timesync synchronization
     */
    void resync() {}

    /**
     * Update the internal status of the StreamManager after losing
     * the Timesync synchronization
     */
    void desync();

    /**
     * The following methods are called by the StreamAPI and ServerAPI functions
     * their function is mainly to pass external events to the right Stream
     * or Server class
     */
    // Creates a new Stream and returns the file-descriptor of the new Stream
    int connect(unsigned char dst, unsigned char dstPort, StreamParameters params);

    // Puts data to be sent to a stream in a buffer, return the number of bytes sent
    int write(int fd, const void* data, int size);

    // Gets data received from a stream, return the number of bytes received
    int read(int fd, void* data, int maxSize);

    // Returns a StreamInfo, containing stream status and parameters
    StreamInfo getInfo(int fd);

    // Closes a Stream or Server on the application side, the Stream/Server
    // is kept in the StreamManager until the master acknowlede the closing.
    // This method enqueues a CLOSED SME
    void close(int fd);

    // Creates a new Server and returns the file-descriptor of the new Server
    int listen(unsigned char port, StreamParameters params);

    // Wait for incoming Streams, if a stream is present return the new Stream file-descriptor
    int accept(int serverfd);

    /**
     * The following methods are called by other TDMH modules,
     * like Uplink, Dataphase, or MACContext
     * they are used to interface the StreamManager with the rest of TDMH
     */
    // Used to trigger an action in all Stream and Server state machines
    // that are currently in a loop (e.g. send SME after timeout)
    void periodicUpdate();

    // Used by the DataPhase class to put received data in the right buffer
    bool receivePacket(StreamId id, const Packet& data);

    // Used by the DataPhase class when an incoming packet is missed
    bool missPacket(StreamId id);

    // Used by the DataPhase class to get data to sent from the right buffer
    // Return true if we have data to send
    bool sendPacket(StreamId id, Packet& data);

    // Used by the DataPhase to apply a received schedule
    void applySchedule(const std::vector<ScheduleElement>& schedule);

    // Used by the DataPhase to apply received info elements
    void applyInfoElements(const std::vector<InfoElement>& infos);

    // Used by DynamicUplinkPhase, gets SMEs to send on the network
    void dequeueSMEs(UpdatableQueue<SMEKey,StreamManagementElement>& queue);

    /**
     * The following methods are called by StreamManager itself,
     * or from the Stream and Server classes contained within.
     */
    // Used by Stream, Server, enqueues an SME to be sent on the network
    void enqueueSME(StreamManagementElement sme);

#ifdef CRYPTO

    AesGcm& getStreamGCM(StreamId id);

    /**
     * Stream keys are derived from the master key as: 
     *      Hash(masterKey||streamId)
     * The master key represents the first block to be digested by the
     * Hash, which uses the same IV for all stream keys. Therefore, the
     * intermediate value of the first block digested, ie:
     *      Hash(masterKey)
     * is common among all stream keys created from the same master key.
     * The secondBlock is used as IV for a new MPHash object, which will be
     * used to digest the second part of the data, ie: the streamId.
     * We have this value precomputed at the creation of the StreamManager
     * by MACContext, by calling initHash once.
     */
    void initHash(const unsigned char masterKey[16]) {
        unsigned char nextIv[16];
        firstBlockStreamHash.digestBlock(nextIv, masterKey);
        secondBlockStreamHash.setIv(nextIv);
        memset(nextIv, 0, 16);
    }
    void startRekeying(const unsigned char masterKey[16]);

    void continueRekeying();
    
    void applyRekeying();

#endif

private:

    /**
     * The following methods are called by StreamManager itself,
     */

    // Used by StreamManager::connect, allocates and returns the first available port
    // if there are no available ports return -1
    // NOTE: remember to lock and unlock map_mutex
    int allocateClientPort();

    // Used by StreamManager::removeStream, sets a given port as free
    void freeClientPort(unsigned char port);

    // Performs the operations needed to add a new Stream to the maps
    // It allocates a new fd number, it creates a new stream and adds it
    // to the map streams and fdt
    // returning the fd
    // NOTE: to be called with the mutex locked
    std::pair<int,REF_PTR_STREAM> addStream(StreamInfo streamInfo);

    // Performs the operations needed to add a new Server to the maps
    // It allocates a new fd number, it creates a new server and adds it
    // to the map servers and fdt
    // returning the fd
    // NOTE: to be called with the mutex locked
    std::pair<int,REF_PTR_SERVER> addServer(StreamInfo serverInfo);

    // Closes and removes a given Stream from the maps
    // deleting the actual Stream object
    // Finally calling freeClientPort on the source port
    // NOTE: to be called with the mutex locked
    void removeStream(StreamId id);

    // Closes and removes a Server on a given port from the maps
    // deleting the actual Server object
    // NOTE: to be called with the mutex locked
    void removeServer(unsigned char port);

    // Prints StreamId and status of a given Stream 
    void printStreamStatus(StreamId id, StreamStatus status);

    // Prints StreamId and status of a given Server 
    void printServerStatus(StreamId id, StreamStatus status);

    /* Reference to NetworkConfiguration */
    const NetworkConfiguration& config;
    /* NetworkId of this node */
    unsigned char myId;
    /* Counter used to assign progressive file-descriptors to Streams and Servers*/
    int fdcounter = 1;

    /* Map containing pointers to Stream and Server classes, indexed by file-descriptors */
    std::map<int, REF_PTR_EP> fdt;
    /* Map containing pointers to Stream classes, indexed by StreamId */
    std::map<StreamId, REF_PTR_STREAM> streams;
    /* Map containing pointers to Server classes, indexed by port */
    std::map<unsigned char, REF_PTR_SERVER> servers;

    const unsigned int maxPorts = 16;
    /* Vector containing the current availability of source ports
     * 0= port free, 1= port used */
    std::vector<bool> clientPorts;
    /* UpdatableQueue of SME to send to the network to reach the master node */
    UpdatableQueue<SMEKey, StreamManagementElement> smeQueue;

#ifdef CRYPTO
    /**
     * IV for the Miyaguchi-Preneel Hash used for deriving stream keys from master key.
     * Value for this constant is arbitrary and is NOT secret.
     */
    const unsigned char streamKeyRotationIv[16] = {
                0x73, 0x54, 0x72, 0x45, 0x61, 0x4d, 0x6d, 0x41,
                0x6e, 0x61, 0x47, 0x65, 0x72, 0x49, 0x76, 0x30
        };
    MPHash firstBlockStreamHash = MPHash(streamKeyRotationIv);
    /**
     * Stream keys are derived from the master key as: 
     *      Hash(masterKey||streamId)
     * The master key represents the first block to be digested by the
     * Hash, which uses the same IV for all stream keys. Therefore, the
     * intermediate value of the first block digested, ie:
     *      Hash(masterKey)
     * is common among all stream keys created from the same master key.
     * The secondBlock is used as IV for a new MPHash object, which will be
     * used to digest the second part of the data, ie: the streamId.
     * When rekeying, the next value for this IV is also precomputed and applied.
     * */
    MPHash secondBlockStreamHash;
    MPHash secondBlockStreamHash_next;
    unsigned char nextIv[16] = {0};

    bool rekeyingInProgress = false;
    // TODO: tweak this value
    const unsigned int maxHashesPerSlot = 5;
    /* Sets of streams needed for handling cuncurrency between rekeying process
     * and applications modifying streams map:
     * At the beginning of the rekeying process, a snapshot of the currently
     * existing streams is taken. This queue is scanned during rekeying and its
     * elements are removed from the snapshot.
     * Because applications can create new streams with connect while rekeying
     * is in progress, such new streams will be added to the snapshot, waiting
     * to be rekeyed.
     * */
    std::queue<StreamId> rekeyingSnapshot;
#endif

    /* Thread synchronization */
#ifdef _MIOSIX
    // Mutex to protect access to shared Stream/Server maps
    mutable miosix::FastMutex map_mutex;
    // Mutex to protect access to shared SME queue
    mutable miosix::FastMutex sme_mutex;
#else
    mutable std::mutex map_mutex;
    mutable std::mutex sme_mutex;
#endif
};

} /* namespace mxnet */
