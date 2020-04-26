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
    MPHash() : next_aes({0}) {
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

} //namespace mxnet
