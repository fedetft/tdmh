/***************************************************************************
 *   Copyright (C) 2012, 2013, 2014, 2015, 2016 by Terraneo Federico and   *
 *      Luigi Rinaldi                                                      *
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

#ifndef TRANSCEIVER_H_
#define TRANSCEIVER_H_

const unsigned int KIND_TIMEOUT=15;

#include "NodeBase.h"
#include "TransceiverConfiguration.h"
#include <string>
#include <boost/crc.hpp>
#include <mutex>

class Transceiver {
public:
    virtual ~Transceiver();
    explicit Transceiver(NodeBase& node);

    enum Unit{
        TICK,
        NS
    };

    enum Correct{
        CORR,
        UNCORR
    };

    static const int minFrequency=2405; ///< Minimum supported frequency (MHz)
    static const int maxFrequency=2480; ///< Maximum supported frequency (MHz)

    /**
     * Turn the transceiver ON (i.e: bring it out of deep sleep)
     */
    void turnOn() { isOn = true; }

    /**
     * Turn the transceiver OFF (i.e: bring it to deep sleep)
     */
    void turnOff() { isOn = false; }

    /**
     * \return true if the transceiver is turned on
     */
    bool isTurnedOn() const { return isOn; }

    /**
     * Put the transceiver to idle state.
     * This function is meant to be called after sending or receiving data to
     * make sure the transceiver is set to the idle state to save some power.
     * Its use is not required for the operation of the transceiver, and if not
     * used the transceiver may remain in receive mode more than necessary.
     */
    void idle() {}

    /**
     * Configure the transceiver
     * \param config transceiver configuration class
     */
    void configure(const TransceiverConfiguration& config);

    /**
     * Send a packet without checking for a clear channel. Packet transmission
     * time point is managed in software and as such is not very time
     * deterministic
     * \param pkt pointer to the packet bytes
     * \param size packet size in bytes. If CRC is enabled the maximum size is
     * 125 bytes (the packet must not contain the CRC, which is appended
     * by this class). If CRC is disabled, maximum length is 127 bytes
     * \throws exception in case of errors
     */
    void sendNow(const void *pkt, int size, std::string pktNAme = "sendNow");

    /**
     * Send a packet checking for a clear channel. If the receiver is not turned
     * on, at least 192us+128us is spent going in receive mode and checking the
     * radio channel. Then, another 192us are spent going in TX mode.
     * Packet transmission time point is managed in software and as such is not
     * very time deterministic
     * \param pkt pointer to the packet bytes
     * \param size packet size in bytes. If CRC is enabled the maximum size is
     * 125 bytes (the packet must not contain the CRC, which is appended
     * by this class). If CRC is disabled, maximum length is 127 bytes
     * \throws exception in case of errors
     */
    bool sendCca(const void *pkt, int size);

    /**
     * Send a packet at a precise time point. This function needs to be called
     * at least 192us before the time when the packet needs to be sent, because
     * the transceiver takes this time to transition to the TX state.
     * \param pkt pointer to the packet bytes
     * \param size packet size in bytes. If CRC is enabled the maximum size is
     * 125 bytes (the packet must not contain the CRC, which is appended
     * by this class). If CRC is disabled, maximum length is 127 bytes
     * \param when the time point when the first bit of the preamble of the
     * packet is to be transmitted on the wireless channel
     * \throws exception in case of errors
     */
    void sendAt(const void *pkt, int size, long long when, std::string pktName = "sendAt", Unit = Unit::NS);

    /**
     * \param pkt pointer to a buffer where the packet will be stored
     * \param size size of the buffer
     * \param timeout timeout absolute time after which the function returns
     * \param unit the unit in which timeout is specified and packet is timestamped
     * \param c if the returned timestamp will be corrected by VT or not
     * \return a RecvResult class with the information about the operation
     * outcome
     * \throws exception in case of errors
     *
     * NOTE: that even if the timeout is in the past, if there is a packet
     * already received, it is returned. So if the caller code has a loop
     * getting packets with recv() and processing, if the processing time is
     * larger than the time it takes to receive a packet, this situation will
     * cause the loop not to end despite the timeout. If it is strictly
     * necessary to stop processing packets received after the timeout, it is
     * responsibility of the caller to check the timeout and discard the
     * packet.
     */
    RecvResult recv(void *pkt, int size, long long timeout, Unit unit=Unit::NS, Correct c=Correct::CORR);

    /**
     * Read the RSSI of the currently selected channel
     * \return RSSI value in dBm
     */
    short readRssi() { return 5; }

private:
    Transceiver(const Transceiver&)=delete;
    Transceiver& operator= (const Transceiver&)=delete;
    NodeBase& parentNode;
    bool isOn = false;
    TransceiverConfiguration cfg;
    cMessage timeoutMsg;
    static const std::string timeoutPktName;
    boost::crc_ccitt_type crc;
    std::mutex crcMutex;

    uint16_t computeCrc(const void* data, int size);
};

#endif /* TRANSCEIVER_H_ */
