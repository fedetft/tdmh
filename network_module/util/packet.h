/***************************************************************************
 *   Copyright (C) 2018-2019 by Federico Terraneo, Federico Amedeo Izzo    *
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
#include "interfaces-impl/transceiver.h"
#include "miosix.h"
#include "debug_settings.h"
#include <array>
#include <limits>
#include <stdexcept>
#include <functional>

namespace mxnet {

const int panHeaderSize = 5; //panHeader size is 5 bytes

class PacketOverflowException : public std::range_error {
public:
    PacketOverflowException(const std::string& err) : range_error(err) {}
};

class PacketUnderflowException : public std::range_error {
public:
    PacketUnderflowException(const std::string& err) : range_error(err) {}
};

/** 
 * This class can be used to send a packet or receive a packet:
 * send methods:
 * - put()
 * - available()
 * - send()
 * receive methods:
 * - recv()
 * - size()
 * - get()
 * - discard()
 */
class Packet {
public:
    Packet() : dataSize(0), dataStart(0) {}

    void clear() {
        dataSize = 0;
        dataStart = 0;
    }

    void put(const void* data, unsigned int size);

    void get(void* data, unsigned int size);

    /** 
     * When reading a packet, ignore "size" bytes
     */
    void discard(unsigned int size);
    
    /**
     * \return true if there are no bytes for Packet::get()
     */
    bool empty() const { return dataSize == dataStart; }

    /** 
     * \return how many bytes are stored in the packet and are available
     * for Packet::get()
     */
    unsigned int size() const { return (dataSize - dataStart); }

    /** 
     * \return the free space left in the packet, which is the number of bytes
     * available for Packet::put()
     */
    unsigned int available() const { return (maxSize() - dataSize); }

    /** 
     * \return the maximum number of bytes a Packet can contain
     */
    static unsigned int maxSize() { return MediumAccessController::maxPktSize; }

    void print() const {
        /* Check that immediately after receive, dataStart is equal to 0 */
        assert (dataStart == 0);
        print_dbg("Packet Dump, size:%d\n", dataSize);
        miosix::memDump(packet.data(), dataSize);
    }

    void send(MACContext& ctx, long long sendTime) const;

    miosix::RecvResult recv(MACContext& ctx, long long tExpected) {
        std::function<bool (const Packet& p, miosix::RecvResult r)> f;
        f = [](const Packet&, miosix::RecvResult){ return true; };
        return recv(ctx, tExpected, f);
    }

    /*
     * The recv methods accepts an optional parameter, and keeps receiving packets
     * as long as the condition provided is false
     * This is useful to avoid calculating time offsets multiple times.
     */
    miosix::RecvResult recv(MACContext& ctx, long long tExpected,
                            std::function<bool (const Packet& p, miosix::RecvResult r)> pred,
                            miosix::Transceiver::Correct = miosix::Transceiver::Correct::CORR);

    /*
     * The operator[] can be used to get the value of a given byte in the packet
     * Note, it cannot be used to write value or a Segmentation Fault will happen
     */
    unsigned char operator[](int index) const {
        if(index > dataSize)
            throw std::range_error("Packet::operator[] const: Overflow!");
        return packet[index];
    }

    unsigned char& operator[](int index) {
        if(index > dataSize)
            throw std::range_error("Packet::operator[] const: Overflow!");
        return packet[index];
    }

    /**
     * This operator compares byte by byte the two Packets
     */
    bool operator==(const Packet& rhs);

    bool operator!=(const Packet& rhs) {
        return !(*this == rhs);
    }

    /**
     * This method Adds to the packet an IEEE 802.15.4 header, containing
     * a given panId, this is useful to distinguish TDMH packets from
     * generic ZigBee or other IEEE 802.15.4 packets
     */
    void putPanHeader(unsigned short panId);

    /**
     * Checks the IEEE 802.15.4 header of the current packet,
     * @return true if current packet is an UplinkPacket, false otherwise
     */
    bool checkPanHeader(unsigned short panId);

    /**
     * Removes the IEEE 802.15.4 header from the current packet,
     */
    void removePanHeader() {
        // Remove panHeader from packet
        discard(panHeaderSize);
    }

private:
    std::array<unsigned char, MediumAccessController::maxPktSize> packet;
    // If this assert fail, increase size of dataSize and dataStart
    static_assert(MediumAccessController::maxPktSize <= std::numeric_limits<unsigned char>::max(), "");
    unsigned char dataSize;
    unsigned char dataStart;
};

} /* namespace mxnet */
