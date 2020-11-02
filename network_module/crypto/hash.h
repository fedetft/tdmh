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

#include <cstring>
#include "aes.h"

namespace mxnet {

/**
 * Miyaguchi-Preneel Hashing scheme
 */
class MPHash {
public:
    /**
     * Default constructor: use IV=0
     */
    MPHash() : next_aes() {
        memset(iv, 0, 16);
        memset(next_aes_key, 0, 16);
    }

    /**
     * Constructor
     * \param iv the init vector used in Miyaguchi-Preneel scheme
     */
    MPHash(const unsigned char iv[16]) : next_aes(iv) {
        memcpy(this->iv, iv, 16);
        memcpy(this->next_aes_key, iv, 16);
    }

    /**
     * Change IV and reset
     * \param iv the new init vector in Miyaguchi-Preneel scheme
     */
    void setIv(const unsigned char iv[16]) {
        memcpy(this->iv, iv, 16);
        reset();
    }

    /**
     * Reset hash state: only the IV is preserved
     */
    void reset() {
        next_aes.rekey(iv);
        memcpy(next_aes_key, iv, 16);
    }

    /**
     * Digest length bytes of data 
     * \param hash pointer to a buffer of at least 16 free bytes where the digest will be written
     * \param data pointer to a buffer containing the data to hash
     * \param length the number of bytes of data to be hashed
     */
    //void digest(void *hash, const void *data, unsigned length);

    /**
     * Digest 16 bytes of data 
     * \param hash pointer to a buffer of at least 16 free bytes where the digest will be written
     * \param data pointer to a buffer containing the data to hash
     */
    void digestBlock(void *hash, const void *data);
private:
    unsigned char iv[16]; // used as AES key when digesting the first block of data
    Aes next_aes;  // aes to use for processing next block
    unsigned char next_aes_key[16]; // key of next_aes
};

class SingleBlockMPHash {
public:
    /**
     * Default constructor: use IV=0
     */
    SingleBlockMPHash() {
        memset(iv, 0, 16);
    }

    /**
     * Constructor
     * \param iv the init vector used in Miyaguchi-Preneel scheme
     */
    SingleBlockMPHash(const unsigned char iv[16]) : aes(iv) {
        memcpy(this->iv, iv, 16);
    }

    /**
     * Change IV and reset
     * \param iv the new init vector in Miyaguchi-Preneel scheme
     */
    void setIv(const unsigned char iv[16]) {
        memcpy(this->iv, iv, 16);
        aes.rekey(iv);
    }

    /**
     * Digest 16 bytes of data 
     * \param hash pointer to a buffer of at least 16 free bytes where the digest will be written
     * \param data pointer to a buffer containing the data to hash
     */
    void digestBlock(void *hash, const void *data);
private:
    unsigned char iv[16]; // used as AES key when digesting data blocks
    Aes aes;  // aes to use for processing a single data block
};

} //namespace mxnet
