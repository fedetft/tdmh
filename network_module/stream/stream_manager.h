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

    /**
     * Used by ScheduleDistribution to prepare for schedule application.
     * This method is meant to be called with the same schedule as applySchedule.
     * This method adds new streams that were not present in the previous
     * schedule, but leaves existing streams untouched. When crypto is on,
     * it prepares data structures needed for rekeying operations.
     */
    void setSchedule(const std::vector<ScheduleElement>& schedule);

    /**
     * Used by ScheduleDistribution to apply a received schedule.
     * This method is meant to be called with the same schedule as setSchedule.
     * This method applies changes to existing streams where the new schedule
     * differs, while also deleting old streams that are no longer present in the
     * new schedule. When crypto is on, it applies new keys to all streams. Such
     * keys are meant to be precomputed before applying the schedule, by calling
     * continueRekeying() the appropriate number of times.
     */
    void applySchedule(const std::vector<ScheduleElement>& schedule);

    // Used by ScheduleDistribution to apply received info elements
    void applyInfoElements(const std::vector<InfoElement>& infos);

    // Used by DynamicUplinkPhase, gets SMEs to send on the network
    void dequeueSMEs(UpdatableQueue<SMEKey,StreamManagementElement>& queue);

    /**
     * The following methods are called by StreamManager itself,
     * or from the Stream and Server classes contained within.
     */
    // Used by Stream, Server, enqueues an SME to be sent on the network
    void enqueueSME(StreamManagementElement sme);

    /**
     * Reset sequence numbers of all known streams.
     * Called by dataphase at the start of a new data superframe.
     */
    void resetSequenceNumbers();

    /**
     * Get the current sequence number of a stream.
     * Called by dataphase to use the sequence number for crypto.
     */
    unsigned long long getSequenceNumber(StreamId id);

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
     * We let the KeyManager compute this value and set it when
     * KeyManager::startRekeying is called.
     * The secondBlock is used as IV for a new MPHash object, which will be
     * used to digest the second part of the data, ie: the streamId.
     * */
    void setSecondBlockHash(const unsigned char nextIv[16]) {
        secondBlockStreamHash.setIv(nextIv);
    }

    /**
     * Called directly by ScheduleDistribution phase when a schedule is resent unchanged.
     */
    void startRekeying();

    /**
     * Called directly by ScheduleDistribution phase in downlink tiles reserved for
     * rekeying.
     */
    void continueRekeying();
    
    /**
     * Called directly by ScheduleDistribution phase when a schedule is resent unchanged.
     */
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

    /**
     * Set of streams present in the last schedule but not in the next one. These
     * streams will be removed from the map when the next schedule is applied.
     */
    std::vector<StreamId> streamsToRemove;
    /**
     * Set of streams present in the schedule that is currently being applied. A copy
     * of this structure is taken when starting rekeying, in order to rekey only the
     * streams that don't need to be deleted. This is not only a matter of avoiding
     * unnecessary computations, but also needed to comply to real time constraints.
     * The schedule activation tile is computed by reserving a number of downlink tiles
     * for schedule distribution and rekeying, and this number is sized on the maximum
     * number of streams a node will need to rekey. Therefore, if many streams are rekeyed
     * that were present in the previous schedule but not in the next one, therefore not
     * accounted for when the activation tile was decided, the time necessary to compute
     * all keys will exceed the given time, resulting in real time contraint violations.
     */
    std::queue<StreamId> nextScheduleStreams;

#ifdef CRYPTO

    void doStartRekeying();

    void doContinueRekeying();

    void doApplyRekeying();

    /**
     * Stream keys are derived from the master key as: 
     *      Hash(masterKey||streamId)
     * The master key represents the first block to be digested by the
     * Hash, which uses the same IV for all stream keys. Therefore, the
     * intermediate value of the first block digested, ie:
     *      Hash(masterKey)
     * is common among all stream keys created from the same master key.
     * We let the KeyManager compute this value and set it when
     * KeyManager::startRekeying is called.
     * The secondBlock is used as IV for a new MPHash object, which will be
     * used to digest the second part of the data, ie: the streamId.
     * */
    SingleBlockMPHash secondBlockStreamHash;

    bool rekeyingInProgress = false;
    // TODO: tweak this value
    const unsigned int maxHashesPerSlot = 5;

    /**
     * Set of streams that need rekeying. At the beginning of the rekeying process,
     * the snapshot is taken by copying nextScheduleStreams. If this happens while
     * setting up a new schedule, such set is computed by setSchedule. If the rekeying
     * is happening without schedule change, it means the user (ScheduleDistribution)
     * is calling startRekeying() directly, and the snapshot will be identical to the
     * one computed when the schedule was first received.
     * Elements of the snapshot are gradually removed as continueRekeying() is called
     * to rekey streams. The rekeying process finishes once this queue is empty.
     */
    std::queue<StreamId> rekeyingSnapshot;

    /* Return this GCM to dataphase for safety in case dataphase asks for the GCM
     * of a stream that does not exist */
    AesGcm emptyGCM;
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
