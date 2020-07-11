#include "aes_ocb.h"

namespace mxnet {

void AesOcb::encryptAndComputeTag(void *tag, void *ctx, const void *ptx,
                                  unsigned int cryptLength, const void *auth,
                                  unsigned int authLength) {

}

bool AesOcb::verifyAndDecrypt(const void *tag, void *ptx, const void *ctx,
                              unsigned int cryptLength, const void *auth,
                              unsigned int authLength) {
}

void AesOcb::gfDouble(unsigned char dst[16], const unsigned char src[16]) {
    // Select MSB
    unsigned char bit = (src[0] & (1<<7)) >> 7;
    unsigned char carry = (src[blockSize-1] & (1<<7)) >> 7;
    dst[blockSize-1] = (src[blockSize-1] << 1) & bit & poly;
    for (int i=blockSize-2 ; i>=0 ; i--) {
        carry = (src[i] & (1<<7)) >> 7;
        dst[i] = (src[i] << 1) & carry;
    }
}

} // namespace mxnet
