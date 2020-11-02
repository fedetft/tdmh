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

#include <stdexcept>
#include <mutex>
#include "aes.h"
#include "initialization_vector.h"
#include "crypto_utils.h"
#ifdef _MIOSIX
#include "../util/aes_accelerator.h"
#else
#include "../util/tiny_aes_c.h"
#endif

using namespace std;

namespace mxnet {
const unsigned int Aes::AESBlockSize = 16;

#ifdef _MIOSIX
miosix::Mutex Aes::aesMutex;
AESAccelerator& Aes::aesAcc = AESAccelerator::instance();
#else
std::mutex Aes::aesMutex;
#endif

Aes::~Aes() {
    secureClearBytes(key, AESBlockSize);
#ifdef _MIOSIX
    secureClearBytes(lrk, AESBlockSize);
#endif
}

void Aes::ecbEncrypt(void *ctx, const void *ptx, unsigned int length) {
    if(length == 0) return;
    if((length & (unsigned int)0xf) != 0)
        throw range_error("Aes::ecbEncrypt : buffer size is not a multiple of cipher block size.");

    unsigned char *cp = reinterpret_cast<unsigned char*>(ctx);
    const unsigned char *pp = reinterpret_cast<const unsigned char*>(ptx);

    {
#ifdef _MIOSIX
        miosix::Lock<miosix::Mutex> lock(aesMutex);
        aesAcc.aes128_setKey(key);
        for (unsigned i=0; i<length; i+=AESBlockSize) {
            aesAcc.aes128_ecbEncrypt(&cp[i], &pp[i]);
        }
#else
        std::unique_lock<std::mutex> lock(aesMutex);
        for (unsigned i=0; i<length; i+=AESBlockSize) {
            AES_ECB_encrypt(&pp[i], key, &cp[i]);
        }
#endif
    }
}

void Aes::ecbDecrypt(void *ptx, const void *ctx, unsigned int length) {
    if(length == 0) return;
    if((length & (unsigned int)0xf) != 0)
        throw range_error("Aes::ecbDecrypt : buffer size is not a multiple of cipher block size.");

    unsigned char *pp = reinterpret_cast<unsigned char*>(ptx);
    const unsigned char *cp = reinterpret_cast<const unsigned char*>(ctx);

    {
#ifdef _MIOSIX
        miosix::Lock<miosix::Mutex> lock(aesMutex);
        aesAcc.aes128_setKey(lrk);
        for (unsigned i=0; i<length; i+=AESBlockSize) {
            aesAcc.aes128_ecbDecrypt(&pp[i], &cp[i]);
        }
#else
        std::unique_lock<std::mutex> lock(aesMutex);
        for (unsigned i=0; i<length; i+=AESBlockSize) {
            AES_ECB_decrypt(&cp[i], key, &pp[i]);
        }
#endif
    }
}

void Aes::ctrXcrypt(const IV& iv, void *dst, const void *src, unsigned int length) {
    if(length == 0) return;
    unsigned dataLength = length;
    unsigned short blockLength;

    unsigned char *dp = reinterpret_cast<unsigned char*>(dst);
    const unsigned char *sp = reinterpret_cast<const unsigned char*>(src);
    unsigned char buffer[AESBlockSize];

    {
#ifdef _MIOSIX
        miosix::Lock<miosix::Mutex> lock(aesMutex);
        aesAcc.aes128_setKey(key);
#else
        std::unique_lock<std::mutex> lock(aesMutex);
#endif
        IV ctr(iv);

        unsigned i=0;
        while(dataLength > 0) {
            blockLength = std::min(dataLength, AESBlockSize);
#ifdef _MIOSIX
            aesAcc.aes128_ecbEncrypt(buffer, ctr.getData());
#else
            AES_ECB_encrypt(ctr.getData(), key, buffer);
#endif
            xorBytes(&dp[i], buffer, &sp[i], blockLength);
            ++ctr; //prefix operator is more efficient
            i+=blockLength;
            dataLength -= blockLength;
        }
    }
    secureClearBytes(buffer, AESBlockSize);
}


} //namespace mxnet
