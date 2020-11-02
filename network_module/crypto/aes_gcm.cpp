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

#include "aes_gcm.h"
#include "initialization_vector.h"
#include "crypto_utils.h"

namespace mxnet {

AesGcm::~AesGcm() {
    secureClearBytes(H, GCMBlockSize);
    secureClearBytes(firstEctr, GCMBlockSize);
    secureClearBytes(workingTag, GCMBlockSize);
}

void AesGcm::encryptAndComputeTag(void *tag, void *ctx, const void *ptx,
                                  unsigned int cryptLength, const void *auth,
                                  unsigned int authLength) { 
    unsigned char *cp = reinterpret_cast<unsigned char*>(ctx);
    const unsigned char *pp = reinterpret_cast<const unsigned char*>(ptx);
    const unsigned char *ap = reinterpret_cast<const unsigned char*>(auth);

    setLengthInfo(authLength, cryptLength);
    start();
    if (authLength != 0) processAuthData(ap);
    if (cryptLength != 0) encryptAndProcessData(cp, pp);
    finish();
    getTag(tag);
}

bool AesGcm::verifyAndDecrypt(const void *tag, void *ptx, const void *ctx,
                              unsigned int cryptLength, const void *auth,
                              unsigned int authLength) {
    unsigned char *pp = reinterpret_cast<unsigned char*>(ptx);
    const unsigned char *cp = reinterpret_cast<const unsigned char*>(ctx);
    const unsigned char *ap = reinterpret_cast<const unsigned char*>(auth);
    const unsigned char *tp = reinterpret_cast<const unsigned char*>(tag);

    unsigned char buffer[16];

    setLengthInfo(authLength, cryptLength);
    start();
    if (authLength != 0) processAuthData(ap);
    if (cryptLength != 0) decryptAndProcessData(pp, cp);
    finish();
    getTag(buffer);
    bool result = true;
    for(unsigned i=0; i<GCMBlockSize; i++) {
        if(tp[i] != buffer[i]) {
            result = false;
            break;
        }
    }
    secureClearBytes(buffer, 16);
    return result;
}

void AesGcm::start() {
    aes.ecbEncrypt(firstEctr, iv.getData());
    // Digest first block containg implicit info to authenticate
    multH(workingTag, slotInfo);
}

void AesGcm::processAuthData(const unsigned char *auth) {
    unsigned dataLength = authLength;
    unsigned blockLength;
    unsigned i=0;
    while(dataLength > 0) {
        blockLength = std::min(dataLength, GCMBlockSize);
        xorBytes(workingTag, workingTag, &auth[i], blockLength);
        multH(workingTag, workingTag);

        i+=blockLength;
        dataLength -= blockLength;
    }
}

void AesGcm::encryptAndProcessData(unsigned char *ctx, const unsigned char *ptx) {
    unsigned dataLength = cryptLength;
    unsigned blockLength;
    unsigned i=0;

    IV ctr(iv);
    aes.ctrXcrypt(++ctr, ctx, ptx, cryptLength);

    while(dataLength > 0) {
        blockLength = std::min(dataLength, GCMBlockSize);
        xorBytes(workingTag, workingTag, &ctx[i], blockLength);
        multH(workingTag, workingTag);

        i+=blockLength;
        dataLength -= blockLength;
    }
}

void AesGcm::decryptAndProcessData(unsigned char *ptx, const unsigned char *ctx) {
    unsigned dataLength = cryptLength;
    unsigned blockLength;
    unsigned i=0;

    while(dataLength > 0) {
        blockLength = std::min(dataLength, GCMBlockSize);
        xorBytes(workingTag, workingTag, &ptx[i], blockLength);
        multH(workingTag, workingTag);

        i+=blockLength;
        dataLength -= blockLength;
    }

    IV ctr(iv);
    aes.ctrXcrypt(++ctr, ptx, ctx, cryptLength);

}

void AesGcm::finish() {
    xorBytes(workingTag, workingTag, lengthInfo, GCMBlockSize);
    multH(workingTag, workingTag);
    xorBytes(workingTag, workingTag, firstEctr, GCMBlockSize);
}

void AesGcm::multH(void *dst, const void *src) {
    /* Field polynomial */
    const unsigned char R = 0xe1;
    unsigned short bitSelect, byteSelect;
    unsigned char mask, bit;
    unsigned char v[16];
    unsigned char z[16];

    memset(z, 0, 16);
    memcpy(v, src, 16);
    for(unsigned i=0; i<128; i++) {
        bitSelect = i % 8;
        byteSelect = i / 8;
        bit = (H[byteSelect] >> (7-bitSelect)) & 1;
        mask = make_mask(bit);
        for(int j=0; j<16; j++) {
            z[j] ^= (mask & v[j]);
        }

        bit = v[15] & 0x01;
        mask = make_mask(bit);
        rightShift(v);
        v[0] ^= (mask & R);
    }
    memcpy(dst, z, 16);
}

} //namespace mxnet
