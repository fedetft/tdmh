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

// For StreamId
#include "stream_management_element.h"
#include "stream.h"
#include "../scheduler/schedule_element.h"
#include "../util/updatable_queue.h"
// For thread synchronization
#ifdef _MIOSIX
#include <miosix.h>
#else
#include <mutex>
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
    StreamManager(unsigned char myId) : myId(myId) {};

    ~StreamManager() {
        disconnect();
    }

    /**
     * Update the internal status of the StreamManager after restoring
     * the Timesync synchronization
     */
    void resync() {};

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
    void putPacket(StreamId id, const Packet& data);

    // Used by the DataPhase class to get data to sent from the right buffer
    void getPacket(StreamId id, Packet& data);

    // Used by the DataPhase to apply a received schedule
    void applySchedule(const std::vector<ScheduleElement>& schedule);

    // Used by the DataPhase to apply received info elements
    void applyInfoElements(const std::vector<InfoElement>& infos);

    // Used by DynamicUplinkPhase, gets SMEs to send on the network
    void dequeueSMEs(UpdatableQueue<StreamId,StreamManagementElement>& queue);

    /**
     * The following methods are called by StreamManager itself,
     * or from the Stream and Server classes contained within.
     */
    // Used by Stream, Server, enqueues an SME to be sent on the network
    void enqueueSME(StreamManagementElement sme);

    // Used by Server, to closed pending streams after server close
    void closedServer(int fd);
private:

    /**
     * The following methods are called by StreamManager itself,
     */

    // Used by StreamManager::connect, allocates and returns the first available port
    // if there are no available ports return -1
    // NOTE: remember to lock and unlock map_mutex
    int allocateClientPort();

    // Used by StreamManager::removeStream, sets a given port as free
    void freeClientPort(int port);

    // Closes and removes a Server on a given port from the maps
    // deleting the actual Server object
    // NOTE: remember to lock and unlock map_mutex
    void removeServer(unsigned char port);

    // Closes and removes a given Stream from the maps
    // deleting the actual Stream object
    // Finally calling freeClientPort on the source port
    // NOTE: remember to lock and unlock map_mutex
    void removeStream(StreamId id);

    // Prints StreamId and status of a given Stream 
    void printStreamStatus(StreamId id, StreamStatus status);

    // Prints StreamId and status of a given Server 
    void printServerStatus(StreamId id, StreamStatus status);

    /* NetworkId of this node */
    unsigned char myId;
    /* Counter used to assign progressive file-descriptors to Streams and Servers*/
    int fdcounter = 1;
    /* Map containing pointers to Stream and Server classes, indexed by file-descriptors */
    std::map<int, intrusive_ref_ptr<Endpoint>> fdt;
    /* Map containing pointers to Stream classes, indexed by StreamId */
    std::map<StreamId, intrusive_ref_ptr<Stream>> streams;
    /* Map containing pointers to Server classes, indexed by port */
    std::map<unsigned char, intrusive_ref_ptr<Server>> servers;
    /* Vector containing the current availability of source ports */
    std::vector<bool> clientPorts;
    /* UpdatableQueue of SME to send to the network to reach the master node */
    UpdatableQueue<StreamId, StreamManagementElement> smeQueue;
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
