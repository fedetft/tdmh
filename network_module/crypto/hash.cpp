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
    memset(buffer, 0, 16);
}

void SingleBlockMPHash::digestBlock(void *hash, const void *data) {
    unsigned char buffer[16];
    aes.ecbEncrypt(buffer, data);
    xorBytes(buffer, iv, buffer, 16);
    xorBytes(hash, buffer, data, 16);
    memset(buffer, 0, 16);
}
} // namespace mxnet
