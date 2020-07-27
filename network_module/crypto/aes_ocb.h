#pragma once
#include "aes.h"
#include "crypto_utils.h"

namespace mxnet {

/**
 * Implementation of the OCB3 mode of authenticated encryption (RFC 7253) using AES.
 *
 * Usage for authenticated encryption/decryption
 *  - setNonce
 *  - encryptAndComputeTag / verifyAndDecrypt
 *
 * NOTE (1): this implementation presents a small difference with respect to
 * the standard's prescriptions. The standard prescribes support for a nonce of
 * variable length. We have chosen to always use a nonce of the maximum possible
 * specified length, that is 12 bytes.
 *
 * NOTE (2): to avoid replay attacks, our message authentication always
 * includes implicit information about the time when the message is being sent.
 * This information is uniquely identified by the tuple
 * <tileOrFrameNumber, sequenceNumber, masterIndex> and encoded into the
 * slotInfo block, which is always authenticated together with the buffers.
 *
 */
class AesOcb {
public:
    ~AesOcb();

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
        auto lp = reinterpret_cast<unsigned long long*>(&nonce[8]);
        lp[0] = sequenceNumber;
        auto ip = reinterpret_cast<unsigned int *>(&nonce[1]);
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
        this->nonce[0] = 0x0;
        this->nonce[1] = 0x0;
        this->nonce[2] = 0x0;
        this->nonce[3] = 0x1;
        memcpy(this->nonce + 4,  nonce, 12);
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

    void processAdditionalData(const void* auth, unsigned int authLength);

    void finishTag(unsigned char tag[16], const unsigned char offset[16]);

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
     * Compute the value of the first offset used for encryption/decryption
     */
    void computeFirstOffset();

    /**
     * Precompute values of offset increments, key dependent.
     */
    void compute_l_values() {
        unsigned char zero[16] = {0};
        aes.ecbEncrypt(l_star, zero);
        gfDouble(l_dollar, l_star);
        gfDouble(&l[0][0], l_dollar);
        for (unsigned i=1; i<maxCachedL; i++) {
            gfDouble(&l[i][0], &l[i-1][0]);
        }
    }

    /**
     * Given a GF(2^128) polynomial in src buffer, p(x), compute p(x)*x
     * and write the result into dst buffer.
     */
    void gfDouble(unsigned char dst[16], const unsigned char src[16]);

    /**
     * NOTE on supported buffer size:
     * This implementation only uses precomputed values of L. These values are computed
     * when the key is set by using the values in table ntz. Therefore, if support
     * for longer packets is desired in the future, either maxBlocks is changed and the
     * ntz table extended, or additional code should be added to compute ntz values on
     * the fly. The latter solution can always be avoided if the packet size is bounded.
     *
     */
    static const unsigned int maxBlocks = 8;
    static const unsigned int maxCachedL = 3;
    static const unsigned int blockSize = 16;
    static const unsigned char poly = 0x87;
    static const unsigned int poly_int = 0x87;
    static constexpr unsigned char ntz[maxBlocks] = { 0, 1, 0, 2, 0, 1, 0, 3 };
    unsigned char l_star[blockSize];
    unsigned char l_dollar[blockSize];
    unsigned char l[maxCachedL][blockSize];

    /* buffers used to process all data at once, so that we can call aes only
     * a few times to optimize use of accelerator */
    unsigned char authBuffer[(maxBlocks + 1)*blockSize];
    unsigned char cryptBuffer[maxBlocks*blockSize];
    unsigned char offsetBuffer[(maxBlocks + 1)*blockSize];

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

    unsigned char sum[16] = {0};
    unsigned char checksum[16] = {0};

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
