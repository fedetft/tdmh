/***************************************************************************
 *   Copyright (C)  2020 by Valeria Mazzola                                *
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
#include "aes.h"
#include "initialization_vector.h"
#include "crypto_utils.h"

namespace mxnet {

/**
 * Implementation of authenticated encryption/decryption using AES block cipher
 * in Galois/Counter Mode of operation (GCM), NIST SP 800-38D :
 * https://csrc.nist.gov/publications/detail/sp/800-38d/final
 *
 * Usage for authenticated encryption/decryption
 *  - setIV
 *  - encryptAndComputeTag / verifyAndDecrypt
 *
 * NOTE (1): this implementation presents a small difference with respect to
 * the standard's prescriptions. The standard prescribes support for an IV of
 * variable length, which is then expanded into a 128-bit value to use with
 * counter mode, called J0, via Galois Field multiplication. Because this
 * operation is expensive, we instead compute J0 directly by encrypting the
 * slotInfo block with the GCM key. Thus this code refers to the
 * standard-defined value J0 as IV.
 *
 * NOTE (2): to avoid replay attacks, our message authentication always
 * includes implicit information about the time when the message is being sent.
 * This information is uniquely identified by the tuple
 * <tileOrFrameNumber, sequenceNumber, masterIndex> and encoded into the
 * slotInfo block, which is always authenticated together with the buffers.
 *
 */
class AesGcm {
public:
    ~AesGcm();

    /**
     * Default constructor: IV=0, key=0
     */
    AesGcm() {
        unsigned char zero[16] = {0};
        aes.ecbEncrypt(H, zero);
        memset(lengthInfo, 0, 16);
        memset(firstEctr, 0, 16);
        memset(workingTag, 0, 16);
        memset(slotInfo, 0, 16);
    }

    /**
     * Constructor with blank IV. Use 0 as IV.
     * */
    AesGcm(const void *key) : aes(key) {
        unsigned char zero[16] = {0};
        aes.ecbEncrypt(H, zero);
        memset(lengthInfo, 0, 16);
        memset(firstEctr, 0, 16);
        memset(workingTag, 0, 16);
        memset(slotInfo, 0, 16);
    }

    /**
     * Constructor. Initializes key-dependent data and IV
     * */
    AesGcm(const void *key, IV& iv) : aes(key) , iv(iv) {
        unsigned char zero[16] = {0};
        aes.ecbEncrypt(H, zero);
        memset(lengthInfo, 0, 16);
        memset(firstEctr, 0, 16);
        memset(workingTag, 0, 16);
        memset(slotInfo, 0, 16);
    }

    /**
     * Reset all info except key and key-dependent data
     * */
    void reset() {
        memset(lengthInfo, 0, 16);
        memset(firstEctr, 0, 16);
        memset(workingTag, 0, 16);
        memset(slotInfo, 0, 16);
    }

    /**
     * Change key and key-derived information (H)
     * \param key a buffer of 16 bytes containing the new key
     */
    void rekey(const void *key) {
        aes.rekey(key);
        unsigned char zero[16] = {0};
        aes.ecbEncrypt(H, zero);
    }

    /**
     * Set IV for counter mode as AES_Encrypt(key, slotNumber)
     * */
    void setIV(unsigned int tileOrFrameNumber,
            unsigned long long sequenceNumber, unsigned int masterIndex) {
#ifndef UNITTEST
        setSlotInfo(tileOrFrameNumber, sequenceNumber, masterIndex);
#endif
        unsigned char ivData[16];
        aes.ecbEncrypt(ivData, slotInfo);
        iv = IV(ivData);
        secureClearBytes(ivData, 16);
    }

#ifdef UNITTEST
    /**
     * Set the slotInfo block directly. Only used for testing.
     * */
    void setSlotInfo(unsigned char data[16]) {
        memcpy(slotInfo, data, 16);
    }

    /**
     * Set the IV directly. Only used for testing.
     */
    void setIV(IV iv) { this->iv = iv; }
#endif

    /**
     * Get current value of the tag being computed.
     * \param tag a buffer of at least 16 free bytes where the value of the tag is written
     */
    void getTag(void *tag) {
        memcpy(tag, workingTag, GCMBlockSize);
    }

    /**
     * Run GCM authenticated encryption.
     * Encrypt cryptLength bytes of data at ptx, write to ctx
     * Encrypt with AES in counter mode.
     * Compute authentication tag using:
     *  - the slotInfo block
     *  - authLength bytes at auth
     *  - cryptLength bytes at ctx
     * 
     * \param tag a buffer of at least 16 free bytes where the value of the tag is written
     * \param ctx a buffer where cryptLength bytes of ciphertext are written
     * \param ptx a buffer where cryptLength bytes of plaintext are read 
     * \param cryptLength length in bytes of data to encrypt. Can have any non negative value
     * \param auth a buffer where authLength bytes of additional data are read
     * \param authLength length in bytes of additional data to authenticate. Can have any non negative value
     * */
    void encryptAndComputeTag(void *tag, void *ctx, const void *ptx,
                              unsigned int cryptLength, const void *auth,
                              unsigned int authLength);

    /**
     * Run GCM authenticated decryption and verification.
     * Decrypt cryptLength bytes of data at ctx, write to ptx
     * Decrypt with AES in counter mode.
     * Compute authentication tag using:
     *  - the slotInfo block
     *  - authLength bytes at auth
     *  - cryptLength bytes at ctx
     * 
     * \param tag a buffer of length 16 bytes containing the tag to verify
     * \param ptx a buffer where cryptLength bytes of plaintext are written 
     * \param ctx a buffer where cryptLength bytes of ciphertext are read
     * \param cryptLength length in bytes of data to decrypt. Can have any non negative value
     * \param auth a buffer where authLength bytes of additional data are read
     * \param authLength length in bytes of additional data to authenticate. Can have any non negative value
     * @return true if verification is data is authentic, false otherwise
     * */
    bool verifyAndDecrypt(const void *tag, void *ptx, const void *ctx,
                          unsigned int cryptLength, const void *auth,
                          unsigned int authLength);


private:

    /**
     * Set the information uniquely identifying the time at which the message will be sent.
     * This information is always implicitly authenticated.
     * Different messages types can use these numbers with different semantics.
     * \param tileOrFrameNumber sequential number identifying the tile or frame, used to identify the time at which the message is sent
     * \param sequenceNumber addictional number to ensure slotInfo unicity, in case the tileOrFrameNumber is not sufficient to uniquely identify a message
     * \param masterIndex the incremental number that identifies the current master key in the hash chain rekeying mechanism
     */
    void setSlotInfo(unsigned int tileOrFrameNumber,
            unsigned long long sequenceNumber, unsigned int masterIndex) {
        auto ip = reinterpret_cast<unsigned int *>(slotInfo);
        ip[0] = masterIndex;
        ip[1] = tileOrFrameNumber;
        auto lp = reinterpret_cast<unsigned long long*>(&slotInfo[8]);
        lp[0] = sequenceNumber;
    }

    /**
     * Save lengths of authenticated and encrypted data and initialize lengthInfo block
     * */
    void setLengthInfo(unsigned authLength, unsigned cryptLength) {
        this->authLength = authLength;
        this->cryptLength = cryptLength;

        // authenticated-only data length must include the slotInfo block
        auto p = reinterpret_cast<uint16_t*>(lengthInfo);
        p[3] = reverseShortBytes((uint16_t) 8*(authLength + GCMBlockSize));
        p[7] = reverseShortBytes((uint16_t) 8*cryptLength);

    }

    /* Start GCM:
     *  - compute E(iv)
     *  - digest slotInfo block
     */
    void start();

    /* Digest authLength bytes at auth */
    void processAuthData(const unsigned char *auth);

    /* Encrypt cryptLength bytes and digest ciphertext */
    void encryptAndProcessData(unsigned char *ctx, const unsigned char *ptx);

    /* Decrypt cryptLength bytes and digest ciphertext */
    void decryptAndProcessData(unsigned char *ptx, const unsigned char *ctx);

    /* Digest length info and compute tag */
    void finish();


    /* Multiplication by H in GF(2^128) */
    void multH(void *dst, const void *src);

    unsigned char make_mask(unsigned char bit) {
        unsigned char mask = bit;
        for(int i=0; i<8; i++) {
           mask |= mask << 1;
        }
        return mask;
    }

    void rightShift(unsigned char buf[16]) {
        unsigned char carry = 0;
        unsigned char carry_next;
        for(int i=0; i<16; i++) {
            carry_next = (buf[i] & 0x01) << 7;
            buf[i] = (buf[i] >> 1) | carry;
            carry = carry_next;
        }
    }

    unsigned short reverseShortBytes(unsigned short n) {
        return ((n & 0xff00) >> 8 ) | ((n & 0x00ff) << 8) ;
    }

    const unsigned int GCMBlockSize = 16;
    Aes aes;

    /* Block containing implicit information to be authenticated, not included in packet:
     *  - tileOrFrameNumber
     *  - sequenceNumber
     *  - masterIndex
     *  */
    unsigned char slotInfo[16];

    /* Block containing lengths of authenticated and encrypted data
     *  - authLength
     *  - cryptLength
     *  */
    unsigned char lengthInfo[16];
    unsigned authLength;
    unsigned cryptLength;

    /* Value H for multiplication in finite field.
     * GCM standard prescribes H to be equal to the value zero encrypted with the key */
    unsigned char H[16];


    /* initial IV value */
    IV iv;

    unsigned char firstEctr[16];
    unsigned char workingTag[16];
};

} //namespace mxnet
