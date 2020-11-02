/***************************************************************************
 *   Copyright (C)  2019 by Valeria Mazzola                                *
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

#ifdef _MIOSIX

#include "aes_accelerator.h"
#include <miosix.h>
#include <stdio.h>
#ifdef UNITTEST
#include <cstring>
#endif

namespace mxnet {

AESAccelerator& AESAccelerator::instance() {
    static AESAccelerator singleton;
    return singleton;
}

void AESAccelerator::aes128_setKey(const void *key) {
    volatile auto *kp = reinterpret_cast<const unsigned int*>(key);
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

    /*
     * volatile here is to fix what may be considered a compiler bug.
     * The ARM Cortex CPUs are said to support unaligned memory access, thus
     * casting from void* to unsigned int* is supposed to be safe, as ldr
     * and str instructions can work with unaligned addresses.
     * Well, almost, as ldmia and stmia instructions do not support unaligned
     * memory access, and the compiler in some uncommon corner cases, such as
     * the code below decides to "optimize" by using an stmia, with the effect
     * of causing an unaligned memory acces UsageFault the first time an
     * unaligned pointer is passed.
     */
    volatile auto *kp = reinterpret_cast<unsigned int*>(lrk);
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
    volatile auto *sp = reinterpret_cast<const unsigned int*>(src);
    AES->DATA = sp[0];
    AES->DATA = sp[1];
    AES->DATA = sp[2];
    AES->DATA = sp[3];

    /**
     * EFM32GG errata sheet mentions that, when BYTEORDER is set,
     * AES_STATUS_RUNNING is set one clock cycle late and suggests using
     * a NOP before reafind AES_STATUS when polling. In our experience, one
     * NOP was not enough, but two worked.
     */
    AES->CMD = AES_CMD_START;
    __NOP();
    __NOP();
    while(AES->STATUS & AES_STATUS_RUNNING) ;

    volatile auto *dp = reinterpret_cast<unsigned int*>(dst);
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

#endif // #ifdef _MIOSIX
