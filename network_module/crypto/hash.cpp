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

#include "hash.h"
#include "crypto_utils.h"

namespace mxnet {

void MPHash::digestBlock(void *hash, const void *data) {
    unsigned char buffer[16];
    next_aes.ecbEncrypt(buffer, data);
    xorBytes(next_aes_key, next_aes_key, buffer, 16);
    xorBytes(next_aes_key, next_aes_key, data, 16);
    next_aes.rekey(next_aes_key);
    memcpy(hash, next_aes_key, 16);
    secureClearBytes(buffer, 16);
}

void SingleBlockMPHash::digestBlock(void *hash, const void *data) {
    unsigned char buffer[16];
    aes.ecbEncrypt(buffer, data);
    xorBytes(buffer, iv, buffer, 16);
    xorBytes(hash, buffer, data, 16);
    secureClearBytes(buffer, 16);
}

} // namespace mxnet
