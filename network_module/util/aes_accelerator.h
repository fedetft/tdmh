#pragma once
#include <miosix.h>
#include "tiny_aes_c.h"

namespace mxnet {

class AESAccelerator {
public:
    static AESAccelerator& instance();
    static const int AESBlockSize = 16;

    AESAccelerator(const AESAccelerator&) = delete;
    AESAccelerator& operator=(const AESAccelerator&) = delete;

    /* Set key to use with multiple blocks. */
    void aes128_setKey(const void *key);
    void aes128_computeLastRoundKey(void *lrk, const void *key);

    void aes128_clearKey();

    /* Encrypt a single block. Expect buffer to be 16 bytes long.
     * Call setKey before encrypting a block.
     */
    void aes128_ecbEncrypt(void *ctx, const void *ptx) {
#ifdef _MIOSIX
        AES->CTRL &= ~AES_CTRL_DECRYPT;
        processBlock(ctx, ptx);
#else
        AES_ECB_encrypt((const uint8_t*)ptx, key, (uint8_t*)ctx, (const uint32_t)16);
#endif
    }
    /* Decrypt a single block. Expect buffer to be 16 bytes long.
     * Call setKey with the last round key before decrypting a block.
     */
    void aes128_ecbDecrypt(void *ptx, const void *ctx) {
#ifdef _MIOSIX
        AES->CTRL |= AES_CTRL_DECRYPT;
        processBlock(ptx, ctx);
#endif
    }

private:
    AESAccelerator();

    void processBlock(void *dst, const void *src);

#ifndef _MIOSIX
    unsigned char key[16];
#endif

};

}
