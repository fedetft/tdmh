#include <stdexcept>
#include "aes.h"
#include "../util/aes_accelerator.h"
#include "initialization_vector.h"

using namespace std;

namespace mxnet {

void Aes::ecbEncrypt(void *ctx, const void *ptx, unsigned int length) {
    if(length % AESBlockSize != 0)
        throw range_error("Aes::ecbEncrypt : buffer size is not a multiple of cipher block size.");

    unsigned char *cp = reinterpret_cast<unsigned char*>(ctx);
    const unsigned char *pp = reinterpret_cast<const unsigned char*>(ptx);

    {
#ifdef _MIOSIX
        miosix::Lock<miosix::Mutex> lock(aesMutex);
#else
        std::unique_lock<std::mutex> lock(aesMutex);
#endif
        aesAcc.aes128_setKey(key);
        for (unsigned i=0; i<length; i+=AESBlockSize) {
            aesAcc.aes128_ecbEncrypt(&cp[i], &pp[i]);
        }
    }
}

void Aes::ecbDecrypt(void *ptx, const void *ctx, unsigned int length) {
    if(length % AESBlockSize != 0)
        throw range_error("Aes::ecbDecrypt : buffer size is not a multiple of cipher block size.");

    unsigned char *pp = reinterpret_cast<unsigned char*>(ptx);
    const unsigned char *cp = reinterpret_cast<const unsigned char*>(ctx);

    {
#ifdef _MIOSIX
        miosix::Lock<miosix::Mutex> lock(aesMutex);
#else
        std::unique_lock<std::mutex> lock(aesMutex);
#endif
        aesAcc.aes128_setKey(lrk);
        for (unsigned i=0; i<length; i+=AESBlockSize) {
            aesAcc.aes128_ecbDecrypt(&pp[i], &cp[i]);
        }
    }
}

void Aes::ctrXcrypt(const IV& iv, void *dst, const void *src, unsigned int length) {
    if(length % AESBlockSize != 0)
        throw range_error("Aes::ctrXcrypt : buffer size is not a multiple of cipher block size.");

    unsigned char *dp = reinterpret_cast<unsigned char*>(dst);
    const unsigned char *sp = reinterpret_cast<const unsigned char*>(src);
    unsigned char buffer[AESBlockSize];

    {
#ifdef _MIOSIX
        miosix::Lock<miosix::Mutex> lock(aesMutex);
#else
        std::unique_lock<std::mutex> lock(aesMutex);
#endif
        aesAcc.aes128_setKey(key);
        IV ctr(iv);
        // NOTE: this can be improved by removing the last useless ctr increment
        for(unsigned i=0; i<length; i+= AESBlockSize) {
            aesAcc.aes128_ecbEncrypt(buffer, ctr.getData());
            xorBlock(&dp[i], buffer, &sp[i]);
            ++ctr; //prefix operator is more efficient
        }
    }
    memset(buffer, 0, AESBlockSize);
}

void Aes::xorBlock(void *dst, const void *op1, const void *op2) {
    unsigned int *dp = reinterpret_cast<unsigned int*>(dst);
    const unsigned int *o1 = reinterpret_cast<const unsigned int*>(op1);
    const unsigned int *o2 = reinterpret_cast<const unsigned int*>(op2);
    for (unsigned i=0; i<AESBlockSize/sizeof(unsigned int); i++) {
        dp[i] = o1[i] ^ o2[i];
    }
}

} //namespace mxnet
