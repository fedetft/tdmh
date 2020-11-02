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

#pragma once
#include <miosix.h>

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
        AES->CTRL &= ~AES_CTRL_DECRYPT;
        processBlock(ctx, ptx);
    }
    /* Decrypt a single block. Expect buffer to be 16 bytes long.
     * Call setKey with the last round key before decrypting a block.
     */
    void aes128_ecbDecrypt(void *ptx, const void *ctx) {
        AES->CTRL |= AES_CTRL_DECRYPT;
        processBlock(ptx, ctx);
    }

private:
    AESAccelerator();

    void processBlock(void *dst, const void *src);
};

}

#endif // #ifdef _MIOSIX
