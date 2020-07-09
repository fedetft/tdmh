#include <stdexcept>
#include <mutex>
#include "aes.h"
#include "initialization_vector.h"
#include "crypto_utils.h"
#ifdef _MIOSIX
#include "../util/aes_accelerator.h"
#else
#include "../util/tiny_aes_c.h"
#endif

using namespace std;

namespace mxnet {
#ifdef _MIOSIX
miosix::Mutex Aes::aesMutex;
#else
std::mutex Aes::aesMutex;
#endif

void Aes::ecbEncrypt(void *ctx, const void *ptx, unsigned int length) {
    if((length & (unsigned int)0xf) != 0)
        throw range_error("Aes::ecbEncrypt : buffer size is not a multiple of cipher block size.");

    unsigned char *cp = reinterpret_cast<unsigned char*>(ctx);
    const unsigned char *pp = reinterpret_cast<const unsigned char*>(ptx);

    {
#ifdef _MIOSIX
        miosix::Lock<miosix::Mutex> lock(aesMutex);
        aesAcc.aes128_setKey(key);
        for (unsigned i=0; i<length; i+=AESBlockSize) {
            aesAcc.aes128_ecbEncrypt(&cp[i], &pp[i]);
        }
#else
        std::unique_lock<std::mutex> lock(aesMutex);
        AES_ECB_encrypt(pp, key, cp, length);
#endif
    }
}

void Aes::ecbDecrypt(void *ptx, const void *ctx, unsigned int length) {
    if((length & (unsigned int)0xf) != 0)
        throw range_error("Aes::ecbDecrypt : buffer size is not a multiple of cipher block size.");

    unsigned char *pp = reinterpret_cast<unsigned char*>(ptx);
    const unsigned char *cp = reinterpret_cast<const unsigned char*>(ctx);

    {
#ifdef _MIOSIX
        miosix::Lock<miosix::Mutex> lock(aesMutex);
        aesAcc.aes128_setKey(lrk);
        for (unsigned i=0; i<length; i+=AESBlockSize) {
            aesAcc.aes128_ecbDecrypt(&pp[i], &cp[i]);
        }
#else
        std::unique_lock<std::mutex> lock(aesMutex);
        AES_ECB_decrypt(cp, key, pp, length);
#endif
    }
}

void Aes::ctrXcrypt(const IV& iv, void *dst, const void *src, unsigned int length) {
    unsigned dataLength = length;
    unsigned short blockLength;

    unsigned char *dp = reinterpret_cast<unsigned char*>(dst);
    const unsigned char *sp = reinterpret_cast<const unsigned char*>(src);
    unsigned char buffer[AESBlockSize];

    {
#ifdef _MIOSIX
        miosix::Lock<miosix::Mutex> lock(aesMutex);
        aesAcc.aes128_setKey(key);
#else
        std::unique_lock<std::mutex> lock(aesMutex);
#endif
        IV ctr(iv);

        unsigned i=0;
        while(dataLength > 0) {
            blockLength = std::min(dataLength, AESBlockSize);
#ifdef _MIOSIX
            aesAcc.aes128_ecbEncrypt(buffer, ctr.getData());
#else
            AES_ECB_encrypt(ctr.getData(), key, buffer, 16);
#endif
            xorBytes(&dp[i], buffer, &sp[i], blockLength);
            ++ctr; //prefix operator is more efficient
            i+=blockLength;
            dataLength -= blockLength;
        }
    }
    memset(buffer, 0, AESBlockSize);
}


} //namespace mxnet
