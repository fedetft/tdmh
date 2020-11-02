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
