#include "aes.h"
#include <miosix.h>
#include <stdio.h>

namespace mxnet {

AESAccelerator& AESAccelerator::instance() {
    static AESAccelerator singleton;
    return singleton;
}

void AESAccelerator::aes128_setKey(const void *key) {
    auto *kp = reinterpret_cast<const unsigned int*>(key);
    AES->KEYHA = kp[0];
    AES->KEYHA = kp[1];
    AES->KEYHA = kp[2];
    AES->KEYHA = kp[3];
}

void AESAccelerator::aes128_computeLastRoundKey(void *lrk, const void *key) {
    //Encrypt first, read last round key from keybuffer
    unsigned char buffer[16] = {0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0};
    aes128_setKey(key);
    aes128_ecbEncrypt(buffer, buffer);

    auto *kp = reinterpret_cast<unsigned int*>(lrk);
    kp[0] = AES->KEYLA;
    kp[1] = AES->KEYLA;
    kp[2] = AES->KEYLA;
    kp[3] = AES->KEYLA;
}

void AESAccelerator::aes128_clearKey() {
    for(int i=0; i<4; i++) {
        AES->KEYHA = 0;
    }
}

void AESAccelerator::processBlock(void *dst, const void *src) {
    auto *sp = reinterpret_cast<const unsigned int*>(src);
    AES->DATA = sp[0];
    AES->DATA = sp[1];
    AES->DATA = sp[2];
    AES->DATA = sp[3];

    AES->CMD = AES_CMD_START;
    while(AES->STATUS & AES_STATUS_RUNNING) ;

    auto *dp = reinterpret_cast<unsigned int*>(dst);
    dp[0] = AES->DATA;
    dp[1] = AES->DATA;
    dp[2] = AES->DATA;
    dp[3] = AES->DATA;
}

AESAccelerator::AESAccelerator() {
    {
        miosix::FastInterruptDisableLock lock;
        CMU->HFCORECLKEN0 |= CMU_HFCORECLKEN0_AES;
    }
    // Use AES128 mode
    // Set KEYBUFEN to reuse key buffer for bulk encryption/decryption
    // This implies that the key should be written into KEYH only once
    // before encryption/decryption.
    AES->CTRL |= AES_CTRL_KEYBUFEN;
    AES->CTRL |= AES_CTRL_BYTEORDER;
}

}
