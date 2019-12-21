#pragma once
#include <miosix.h>
#include <stdio.h>

namespace mxnet {

class AESAccelerator {
public:
    static AESAccelerator& instance();

    AESAccelerator(const AESAccelerator&) = delete;
    AESAccelerator& operator=(const AESAccelerator&) = delete;

    /* Set key to use with multiple blocks. */
    void aes128_setKey(const void *key);
    void aes128_computeLastRoundKey(void *lrk, const void *key);

    void aes128_clearKey();

    /* Encrypt/decrypt a single block. Expect all buffers to be 16 bytes long. */
    void aes128_ecbEncrypt(void *ctx, const void *ptx) {
        AES->CTRL &= ~AES_CTRL_DECRYPT;
        processBlock(ctx, ptx);
    }
    void aes128_ecbDecrypt(void *ptx, const void *ctx) {
        AES->CTRL |= AES_CTRL_DECRYPT;
        processBlock(ptx, ctx);
    }


private:
    AESAccelerator();

    void processBlock(void *dst, const void *src);

};

}
