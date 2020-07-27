#pragma once
#include <mutex>
#include "initialization_vector.h"
#ifdef _MIOSIX
#include "../util/aes_accelerator.h"
#else
#include "../util/tiny_aes_c.h"
#endif

namespace mxnet {

class Aes {
public:
    ~Aes();
    /**
     * Default constructor: key=0
     */
    Aes() {
        memset(this->key, 0, 16);
        {
#ifdef _MIOSIX
            miosix::Lock<miosix::Mutex> lock(aesMutex);
            aesAcc.aes128_computeLastRoundKey(lrk, key);
#else
            std::unique_lock<std::mutex> lock(aesMutex);
#endif
        }
    }

    Aes(const void *key) {
        memcpy(this->key, key, 16);
        {
#ifdef _MIOSIX
            miosix::Lock<miosix::Mutex> lock(aesMutex);
            aesAcc.aes128_computeLastRoundKey(lrk, key);
#else
            std::unique_lock<std::mutex> lock(aesMutex);
#endif
        }
    }

    void rekey(const void *key) {
        memcpy(this->key, key, 16);
        {
#ifdef _MIOSIX
            miosix::Lock<miosix::Mutex> lock(aesMutex);
            aesAcc.aes128_computeLastRoundKey(lrk, key);
#else
            std::unique_lock<std::mutex> lock(aesMutex);
#endif
        }
    }

    /* Encrypt in ECB mode.
     * Expect length to be a multiple of 16 bytes. */
    void ecbEncrypt(void *ctx, const void *ptx, unsigned int length = 16);

    /* Decrypt in ECB mode.
     * Expect length to be a multiple of 16 bytes. */
    void ecbDecrypt(void *ptx, const void *ctx, unsigned int length = 16);

    /* Counter mode uses the same primitive for encryption and decryption.
     * If length is not a multiple of 16 bytes, encrypt the last partial block
     * discarding part of the encrypted counter.
     * */
    void ctrXcrypt(const IV& iv, void *dst, const void *src, unsigned int length);

private:
    static const unsigned int AESBlockSize;

    /* Mutex to access the AESAccelerator, must be locked before setting the key
     * and unlocked after all blocks are processed that use such key. */
#ifdef _MIOSIX
    static miosix::Mutex aesMutex;
    static AESAccelerator& aesAcc;
#else
    static std::mutex aesMutex;
#endif


    unsigned char key[16];

#ifdef _MIOSIX
    unsigned char lrk[16];
#endif

};

} //namespace mxnet
