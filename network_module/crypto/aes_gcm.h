#pragma once
#include "aes.h"
#include "initialization_vector.h"

namespace mxnet {
class AesGcm {
public:
    AesGcm(const void *key) : aes(key) , iv(iv) {
        unsigned char zero[16] = {0};

        memcpy(this->key, key, 16);
        aes.ecbEncrypt(H, zero);
    }

    /* reset all info except key and key-dependent data */
    void reset() {
        //reset iv, slotInfo, length, ectr,
    }

    void rekey(const void *key) {
        unsigned char zero[16] = {0};

        memcpy(this->key, key, 16);
        aes.rekey(key);
        aes.ecbEncrypt(H, zero);
    }

    void setIV(unsigned long slotNumber);
    void setIV(IV iv) { this->iv = iv; }

    void setSlotInfo(unsigned long long slotNumber, unsigned long long masterIndex) {
        auto p = reinterpret_cast<unsigned long long*>(slotInfo);
        p[0] = masterIndex;
        p[1] = slotNumber;
    }

    void setLengthInfo(unsigned authLength, unsigned cryptLength) {
        this->authLength = authLength;
        this->cryptLength = cryptLength;

        auto p = reinterpret_cast<unsigned long long*>(lengthInfo);
        p[0] = (unsigned long long) authLength;
        p[1] = (unsigned long long) cryptLength;
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

    /* get current value of tag */
    void getTag(void *tag) {
        memcpy(tag, workingTag, GCMBlockSize);
    }

    /* Encrypt cryptLength bytes of data at ptx, write to ctx
     * Compute authentication tag using:
     *  - the slotInfo block
     *  - authLength bytes at auth
     *  - the computed ciphertext at ctx
     * 
     * */
    void encryptAndComputeTag(void *tag, void *ctx, const void *ptx,
                              unsigned int cryptLength, const void *auth,
                              unsigned int authLength);

    bool verifyAndDecrypt(const void *tag, void *ptx, const void *ctx,
                          unsigned int cryptLength, const void *auth,
                          unsigned int authLength);

    /* Compute authentication tag using:
     *  - the slotInfo block
     *  - authLength bytes at auth
     * 
     * */
    void computeTag(void *tag, const void *auth, unsigned int authLength);

    void verify(const void *tag, const void *auth, unsigned int authLength);

private:

    /* Multiplication by H in GF(2^128) */
    void multH(void *dst, const void *src);

    void xorBytes(unsigned char *dst, const unsigned char *op1, const unsigned char *op2,
                                                                    unsigned int length) {
        for (unsigned i=0; i<length; i++) {
            dst[i] = op1[i] ^ op2[i];
        }
    }

    const unsigned int GCMBlockSize = 16;
    Aes aes;

    /* Block containing implicit information to be authenticated, not included in packet:
     *  - masterIndex
     *  - slotNumber
     *  */
    unsigned char slotInfo[16];

    /* Block containing lengths of authenticated and encrypted data
     *  - authLength
     *  - cryptLength
     *  */
    unsigned char lengthInfo[16];
    unsigned authLength;
    unsigned cryptLength;


    unsigned char key[16];

    /* Value H for multiplication in finite field.
     * GCM standard prescribes H to be equal to the value zero encrypted with the key */
    unsigned char H[16];
    /* initial IV value */
    IV iv;

    unsigned char firstEctr[16];
    unsigned char workingTag[16];
};

} //namespace mxnet
