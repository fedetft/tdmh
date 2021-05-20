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
#include "aes_ocb.h"

namespace mxnet {

const unsigned int AesOcb::blockSize;
const unsigned char AesOcb::ntz[maxBlocks] = { 0, 1, 0, 2, 0, 1, 0, 3 };

AesOcb::~AesOcb() {
    secureClearBytes(l_star, blockSize);
    secureClearBytes(l_dollar, blockSize);
    secureClearBytes(l, maxCachedL*blockSize);
}

void AesOcb::encryptAndComputeTag(void *tag, void *ctx, const void *ptx,
                                  unsigned int cryptLength, const void *auth,
                                  unsigned int authLength) {

    processAdditionalData(auth, authLength);

    memset(checksum, 0, blockSize);
    unsigned cryptBlocks = cryptLength/blockSize;
    unsigned clen_aligned = cryptBlocks*blockSize;
    if(cryptLength % blockSize > 0) cryptBlocks++;

    if(cryptBlocks > maxBlocks) {
        throw std::range_error("AesOcb: data to encrypt too long");
    }

    unsigned char *op = reinterpret_cast<unsigned char*>(offsetBuffer);
    const unsigned char *pp = reinterpret_cast<const unsigned char*>(ptx);
    unsigned int clen = cryptLength;

    computeFirstOffset();
    unsigned i = 0;
    while (clen >= blockSize) {
        // compute each offset from previous one in offsetBuffer
        xorBytes(op + blockSize, op, &l[ntz[i]][0], blockSize);

        // digest input data into checksum
        xorBytes(checksum, checksum, pp, blockSize);

        i++;
        op += blockSize;
        pp += blockSize;
        clen -= blockSize;
    }
    // handle last partial block if present
    if (clen > 0) {
        // compute the last delta_star and put it in the cryptBuffer directly
        xorBytes(op + blockSize, op, l_star, blockSize);
        memcpy(cryptBuffer + clen_aligned, op + blockSize, blockSize);
        // digest last partial block into checksum and flip one bit
        xorBytes(checksum, checksum, pp, clen);
        checksum[clen] ^= 0x80;
        op += blockSize;
    }

    /* First 16 bytes of offsetBuffer still contain offset0 : not used directly */
    xorBytes(cryptBuffer, offsetBuffer + blockSize, ptx, clen_aligned);
    aes.ecbEncrypt(cryptBuffer, cryptBuffer, cryptBlocks*blockSize);
    xorBytes(ctx, cryptBuffer, offsetBuffer + blockSize, clen_aligned);
    // Last block uses input data only after block cipher call
    pp = reinterpret_cast<const unsigned char*>(ptx);
    auto cp = reinterpret_cast<unsigned char*>(ctx);
    xorBytes(cp + clen_aligned, pp + clen_aligned, cryptBuffer + clen_aligned, clen);

    unsigned char *tp = reinterpret_cast<unsigned char*>(tag);
    finishTag(tp, op);

    secureClearBytes(authBuffer, (maxBlocks+1)*blockSize);
    secureClearBytes(cryptBuffer, (maxBlocks)*blockSize);
    secureClearBytes(offsetBuffer, (maxBlocks+1)*blockSize);
}

bool AesOcb::verifyAndDecrypt(const void *tag, void *ptx, const void *ctx,
                              unsigned int cryptLength, const void *auth,
                              unsigned int authLength) {
    processAdditionalData(auth, authLength);

    memset(checksum, 0, blockSize);
    unsigned cryptBlocks = cryptLength/blockSize;
    unsigned clen_aligned = cryptBlocks*blockSize;
    if(cryptLength % blockSize > 0) cryptBlocks++;

    if(cryptBlocks > maxBlocks) {
        throw std::range_error("AesOcb: data to decrypt too long");
    }

    unsigned char *op = reinterpret_cast<unsigned char*>(offsetBuffer);
    unsigned char *pp = reinterpret_cast<unsigned char*>(ptx);
    const unsigned char *cp = reinterpret_cast<const unsigned char*>(ctx);
    unsigned int clen = cryptLength;

    computeFirstOffset();
    unsigned i = 0;
    while (clen >= blockSize) {
        // compute each offset from previous one in offsetBuffer
        xorBytes(op + blockSize, op, &l[ntz[i]][0], blockSize);

        i++;
        op += blockSize;
        clen -= blockSize;
    }
    // handle last partial block if present
    if (clen > 0) {
        // compute the last delta_star
        xorBytes(op + blockSize, op, l_star, blockSize);
        op += blockSize;
    }

    xorBytes(cryptBuffer, ctx, offsetBuffer + blockSize, clen_aligned);
    aes.ecbDecrypt(cryptBuffer, cryptBuffer, clen_aligned);
    xorBytes(ptx, cryptBuffer, offsetBuffer + blockSize, clen_aligned);
    if (clen > 0) {
        // encrypt delta_star into the last block of cryptBuffer
        aes.ecbEncrypt(cryptBuffer + clen_aligned, op, blockSize);
        xorBytes(pp + clen_aligned, cp + clen_aligned, cryptBuffer + clen_aligned, clen);
    }

    // compute checksum
    unsigned int plen = cryptLength;
    while (plen >= blockSize) {
        xorBytes(checksum, checksum, pp, blockSize);
        pp += blockSize;
        plen -= blockSize;
    }
    if (plen > 0) {
        xorBytes(checksum, checksum, pp, plen);
        checksum[plen] ^= 0x80;
    }

    const unsigned char *tp = reinterpret_cast<const unsigned char*>(tag);
    unsigned char received_tag[16];
    bool valid = true;
    finishTag(received_tag, op);
    
    for (unsigned j=0; j<blockSize; j++) {
        if (received_tag[j] != tp[j]) {
            valid = false;
            //no break; constant time check
        }
    }

    secureClearBytes(authBuffer, (maxBlocks+1)*blockSize);
    secureClearBytes(cryptBuffer, (maxBlocks)*blockSize);
    secureClearBytes(offsetBuffer, (maxBlocks+1)*blockSize);
    secureClearBytes(received_tag, blockSize);

    return valid;
}

void AesOcb::processAdditionalData(const void* auth, unsigned int authLength) {
    // always add one block for slotInfo
    unsigned authLen = authLength + blockSize;
    unsigned authBlocks = authLen/blockSize;
    if(authLen % blockSize > 0) authBlocks++;

    /* When checking length of buffer, also consider the slotInfo block */
    if(authBlocks > maxBlocks) {
        throw std::range_error("AesOcb: additional data too long");
    }

    /* initialize offsets: compute delta values into offsetBuffer */
    unsigned int alen = authLen;
    unsigned char *op = reinterpret_cast<unsigned char*>(offsetBuffer);

    memcpy(op, &l[ntz[0]][0], blockSize);
    alen -= std::min(alen, blockSize);
    unsigned i = 1;
    while (alen >= blockSize) {
        // write delta values starting from second block (i=1)
        xorBytes(op + blockSize, op, &l[ntz[i]][0], blockSize);

        i++;
        op += blockSize;
        alen -= blockSize;
    }

    // handle last partial block if present
    if (alen > 0) {
        xorBytes(op + blockSize, op, l_star, blockSize);
        /* flip one bit at the end of the last partial block, as specified */
        op[blockSize + alen] ^= 0x80;
    }

    /* xor all deltas with all data, and ECB-encrypt in bulk. The first data block is
     * the slotInfo block. */
    xorBytes(offsetBuffer, offsetBuffer, slotInfo, blockSize);
    xorBytes(offsetBuffer + blockSize, offsetBuffer + blockSize, auth, authLength);
    aes.ecbEncrypt(authBuffer, offsetBuffer, authBlocks*blockSize);

    /* compute sum */
    memset(sum, 0, blockSize);

#ifndef _MIOSIX
//#define TEST_EMPTY_AUTH_DATA
#endif
#ifndef TEST_EMPTY_AUTH_DATA
    for (i=0; i<authLen; i+=blockSize) {
        xorBytes(sum, sum, &authBuffer[i], blockSize);
    }
#endif
}

void AesOcb::finishTag(unsigned char tag[16], const unsigned char offset[16]) {

    xorBytes(checksum, checksum, offset, blockSize);
    xorBytes(checksum, checksum, l_dollar, blockSize);
    aes.ecbEncrypt(checksum, checksum);
    xorBytes(tag, checksum, sum, blockSize);
    secureClearBytes(checksum, blockSize);
    secureClearBytes(sum, blockSize);
}

void AesOcb::computeFirstOffset() {
    // select the last 6 bits
    unsigned char bottom = nonce[15] & 0x3f;
    // clear the last 6 bits
    nonce[15] = nonce[15] & 0xc0;
    unsigned char ktop[24];
    aes.ecbEncrypt(ktop, nonce);
    memcpy(ktop + 16, ktop, 8);
    xorBytes(ktop + 16, ktop + 16, ktop + 1, 8);
    unsigned char bitshift = bottom % 8;
    unsigned char byteshift = bottom / 8;
    for (int i=15; i>=0; i--) {
        unsigned char rightpart = ktop[i+byteshift+1] >> (8-bitshift);
        unsigned char leftpart = ktop[i+byteshift] << bitshift;
        offsetBuffer[i] = rightpart | leftpart ;
    }
    secureClearBytes(ktop, 24);
}

void AesOcb::gfDouble(unsigned char dst[16], const unsigned char src[16]) {
    // Select MSB
    unsigned char msb = (src[0] & 0x80) >> 7;
    unsigned char carry = (src[blockSize-1] & 0x80) >> 7;
    dst[blockSize-1] = (src[blockSize-1] << 1) ^ (msb*poly);
    for (int i=blockSize-2 ; i>=0 ; i--) {
        dst[i] = (src[i] << 1) | carry;
        carry = (src[i] & 0x80) >> 7;
    }
}

} // namespace mxnet
