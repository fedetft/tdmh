/***************************************************************************
 *   Copyright (C) 2017-2019 by Polidori Paolo, Terraneo Federico,         *
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

#pragma once

#ifndef _MIOSIX
#include <thread>
#endif
#include "interfaces-impl/transceiver.h"
#include "network_configuration.h"
#include "stream/stream_parameters.h"
#include "util/stackrange.h"

namespace mxnet {

/* These functions are the API for the Streams and Servers */

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

class MACContext;
class UplinkPhase;
class ScheduleDownlinkPhase;
class DataPhase;
class TimesyncDownlink;
class MediumAccessController {
public:
    MediumAccessController() = delete;
    MediumAccessController(const MediumAccessController& orig) = delete;
    virtual ~MediumAccessController();
    /**
     * The method for making the MAC protocol run and start being operative.
     */
    void run();
    void runAsync();
    void stop();
    //5 byte (4 preamble, 1 SFD) * 32us/byte
    static const unsigned int packetPreambleTime = 160000; //TODO move to timesync
    //350us and forced receiverWindow=1 fails, keeping this at minimum
    //This is dependent on the optimization level, i usually use level -O2
    static const unsigned int maxPropagationDelay = 100; //TODO ?
    static const unsigned int receivingNodeWakeupAdvance = 450000; //TODO fine tune and move to timesync
    static const unsigned int sendingNodeWakeupAdvance = 500000; //TODO fine tune and move to timesync
    static const unsigned int downlinkToDataphaseSlack = 500000;
    static const unsigned char maxPktSize = 125;
    // NOTE: control code fills Packet classes up to its max size without other
    // checks, and the max size of Packet is maxPktSize, so setting
    // maxControlPktSize to anything but maxPktSize requires a refactoring
    static const unsigned char maxControlPktSize = maxPktSize;
    static const unsigned char maxDataPktSize = 125;

    MACContext* const getMACContext() const { return ctx; }

protected:
    MediumAccessController(MACContext* const ctx) : ctx(ctx), async(false) {};
    MACContext* const ctx;
#ifdef _MIOSIX
    miosix::Thread* thread;
private:
    static void runLauncher(void* obj) {
        printStackRange("MAC (async)");
        reinterpret_cast<MediumAccessController*>(obj)->run();
    }
#else
    std::thread* thread;
#endif
private:
    bool async;
};

} // namespace mxnet
