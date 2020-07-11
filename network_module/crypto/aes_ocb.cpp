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

void AesOcb::computeFirstOffset() {
    memcpy(lastNonce, nonce, 16);
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
    unsigned char carry;
    for (int i=15; i>=0; i--) {
        carry = ktop[i+byteshift+1] >> (8-bitshift);
        offset0[i] = (ktop[i+byteshift] << bitshift) || carry ;
    }
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
