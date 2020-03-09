#pragma once
#include "../util/aes_accelerator.h"
#include "initialization_vector.h"

namespace mxnet {

class Aes {
public:
    Aes(const void *key) {
        memcpy(this->key, key, 16);
        {
#ifdef _MIOSIX
            miosix::Lock<miosix::Mutex> lock(aesMutex);
#else
            std::unique_lock<std::mutex> lock(aesMutex);
#endif
            aesAcc.aes128_computeLastRoundKey(lrk, key);
        }
    }

    void rekey(const void *key) {
        memcpy(this->key, key, 16);
        {
#ifdef _MIOSIX
            miosix::Lock<miosix::Mutex> lock(aesMutex);
#else
            std::unique_lock<std::mutex> lock(aesMutex);
#endif
            aesAcc.aes128_computeLastRoundKey(lrk, key);
        }
    }

    /* Encrypt in ECB mode.
     * Expect length to be a multiple of 16 bytes. */
    void ecbEncrypt(void *ctx, const void *ptx, unsigned int length = 16);

    /* Decrypt in ECB mode.
     * Expect length to be a multiple of 16 bytes. */
    void ecbDecrypt(void *ptx, const void *ctx, unsigned int length = 16);

    /* Counter mode uses the same primitive for encryption and decryption.
     * Expect length to be a multiple of 16 bytes. */
    void ctrXcrypt(const IV& iv, void *dst, const void *src, unsigned int length);

private:
    const unsigned int AESBlockSize = 16;

    /* Mutex to access the AESAccelerator, must be locked before setting the key
     * and unlocked after all blocks are processed that used such key. */
#ifdef _MIOSIX
    static miosix::Mutex aesMutex;
#else
    static std::mutex aesMutex;
#endif

    AESAccelerator& aesAcc = AESAccelerator::instance();

    /* XOR 16 bytes */
    void xorBlock(void *dst, const void *op1, const void *op2);

    unsigned char key[16];
    unsigned char lrk[16];

};

} //namespace mxnet
