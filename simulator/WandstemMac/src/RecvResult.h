/*
 * RecvResult.h
 *
 *  Created on: Feb 9, 2018
 *      Author: paolo
 */

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
