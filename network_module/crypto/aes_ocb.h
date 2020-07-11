#pragma once
#include "aes.h"
#include "crypto_utils.h"

namespace mxnet {

class AesOcb {
public:
    AesOcb(const unsigned char key[16]) : aes(key) {
        compute_l_values();
    }

    AesOcb() : aes() {
        compute_l_values();
    }

    void rekey(const unsigned char key[16]) {
        aes.rekey(key);
        compute_l_values();
    }

    void setNonce(unsigned int tileOrFrameNumber,
                  unsigned long long sequenceNumber,
                  unsigned int masterIndex) {
#ifndef UNITTEST
        setSlotInfo(tileOrFrameNumber, sequenceNumber, masterIndex);
#endif
        /**
         * We set the 15 bytes of the nonce in a similar value to slotInfo,
         * but we discard the most significat  byte of the sequence number.
         */
        auto lp = reinterpret_cast<unsigned long long*>(&slotInfo[8]);
        lp[0] = sequenceNumber;
        auto ip = reinterpret_cast<unsigned int *>(&slotInfo[1]);
        ip[0] = masterIndex;
        ip[1] = tileOrFrameNumber;
    }

#ifdef UNITTEST
    /**
     * Set the slotInfo block directly. Only used for testing.
     * */
    void setSlotInfo(unsigned char data[16]) {
        memcpy(slotInfo, data, 16);
    }

    /**
     * Set the Nonce directly. Only used for testing.
     * \param nonce pointer to a buffer of 12 bytes, containing a 12 byte
     * nonce (we only test nonces of this length).
     */
    void setNonce(unsigned char nonce[12]) {
        this->nonce[1] = 0x0;
        this->nonce[2] = 0x0;
        this->nonce[3] = 0x1;
        memcpy(this->nonce + 4 ; nonce ; 12);
    }
#endif

    void getTag(void *tag) {
    }

    /**
     * */
    void encryptAndComputeTag(void *tag, void *ctx, const void *ptx,
                              unsigned int cryptLength, const void *auth,
                              unsigned int authLength);

    /**
     * */
    bool verifyAndDecrypt(const void *tag, void *ptx, const void *ctx,
                          unsigned int cryptLength, const void *auth,
                          unsigned int authLength);


private:

    /**
     * Set the information uniquely identifying the time at which the message
     * will be sent.  This information is always implicitly authenticated.
     * Different messages types can use these numbers with different semantics.
     * \param tileOrFrameNumber sequential number identifying the tile or
     * frame, used to identify the time at which the message is sent
     * \param sequenceNumber addictional number to ensure slotInfo unicity, in
     * case the tileOrFrameNumber is not sufficient to uniquely identify a
     * message
     * \param masterIndex the incremental number that identifies the current
     * master key in the hash chain rekeying mechanism
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
     * Compute the value of the first offset used foe encryption/decryption
     */
    void computeFirstOffset() {
        memcpy(lastNonce, nonce, 16);
        // select the last 6 bits
        unsigned char bottom = nonce[15] & 0x3f;
        // clear the last 6 bits
        nonce[15] = nonce[15] & 0xc0;
        unsigned char ktop[24];
        aes.ecbEncrypt(ktop, nonce);
        memcpy(ktop + 16, ktop, 8);
        xorBytes(ktop + 16, ktop + 16, ktop + 1, 8);

        unsigned char bitshift = bottom % 8;
        unsigned char byteshift = bottom / 8;
        unsigned char carry;
        for (int i=15; i>=0; i--) {
            carry = ktop[i+byteshift+1] >> (8-bitshift);
            offset0[i] = (ktop[i+byteshift] << bitshift) || carry ;
        }
    }

    /**
     * Precompute values of offset increments, key dependent.
     */
    void compute_l_values() {
        unsigned char zero[16] = {0};
        aes.ecbEncrypt(l_star, zero);
        gfDouble(l_dollar, l_star);
        gfDouble(&l[0][0], l_dollar);
        for (unsigned i=1; i<maxCachedL; i++) {
            gfDouble(&l[0][i], &l[0][i-1]);
        }
    }

    /**
     * Given a GF(2^128) polynomial in src buffer, p(x), compute p(x)*x
     * and write the result into dst buffer.
     */
    void gfDouble(unsigned char dst[16], const unsigned char src[16]);

    static const unsigned int maxBlocks = 7;
    static const unsigned int maxCachedL = 3;
    static const unsigned int blockSize = 16;
    static const unsigned char poly = 0x87;
    static constexpr unsigned char ntz[maxBlocks] = { 0, 1, 0, 2, 0, 1, 0 };
    unsigned char l_star[16];
    unsigned char l_dollar[16];
    unsigned char l[blockSize][maxCachedL];

    /**
     * OCB3 specification prescribes the nonce to be a 128-bit vector:
     * - first 7 bits are set to tag length (in bits) mod 128
     * - the needed amount of bits set to 0
     * - one bit set to 1
     * - the input nonce N occupying the rightmost part of the vector.
     * Because we want our input N to have 120 bits, the maximum allowed by
     * the specification, our nonce vector will have a fixed first byte of
     * value 1, and the remaining 15 bytes set to N.
     */
    unsigned char nonce[16] = {0x1, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0,
                               0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0};

    unsigned char lastNonce[16];

    unsigned char offset0[16];
    unsigned char checksum[16];

    /**
     * Block containing implicit information to be authenticated,
     * not included in packet:
     * - tileOrFrameNumber
     * - sequenceNumber
     * - masterIndex
     **/
    unsigned char slotInfo[16];
    Aes aes;
};

} //namespace mxnet
