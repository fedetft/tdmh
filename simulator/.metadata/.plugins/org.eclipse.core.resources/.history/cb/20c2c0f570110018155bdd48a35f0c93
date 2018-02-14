//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU Lesser General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU Lesser General Public License for more details.
//
// You should have received a copy of the GNU Lesser General Public License
// along with this program.  If not, see http://www.gnu.org/licenses/.
//

#ifndef RECVRESULT_H_
#define RECVRESULT_H_


/**
 * This class is returned by the recv member function of the transceiver
 */
class RecvResult
{
public:
    /**
     * Possible outcomes of a receive operation
     */
    enum ErrorCode
    {
        OK,             ///< Receive succeeded
        TIMEOUT,        ///< Receive timed out
        TOO_LONG,       ///< Packet was too long for the given buffer
        CRC_FAIL,       ///< Packet failed CRC check
        UNINITIALIZED   ///< Receive returned exception
    };

    RecvResult()
        : timestamp(0), rssi(-128), size(0), error(UNINITIALIZED), timestampValid(false) {}

    long long timestamp; ///< Packet timestamp. It is the time point when the
                         ///< first bit of the packet preamble is received
    short rssi;          ///< RSSI of received packet (not valid if CRC disabled)
    short size;          ///< Packet size in bytes (excluding CRC if enabled)
    ErrorCode error;     ///< Possible outcomes of the receive operation
    bool timestampValid; ///< True if timestamp is valid
};



#endif /* RECVRESULT_H_ */
