#include "aes_gcm.h"
#include "initialization_vector.h"

namespace mxnet {

void AesGcm::encryptAndComputeTag(void *tag, void *ctx, const void *ptx,
                                  unsigned int cryptLength, const void *auth,
                                  unsigned int authLength) { 
    unsigned char *cp = reinterpret_cast<unsigned char*>(ctx);
    const unsigned char *pp = reinterpret_cast<const unsigned char*>(ptx);
    const unsigned char *ap = reinterpret_cast<const unsigned char*>(auth);

    setLengthInfo(authLength, cryptLength);
    start();
    processAuthData(ap);
    encryptAndProcessData(cp, pp);
    finish();
    getTag(tag);
}

bool AesGcm::verifyAndDecrypt(const void *tag, void *ptx, const void *ctx,
                              unsigned int cryptLength, const void *auth,
                              unsigned int authLength) {
    unsigned char *pp = reinterpret_cast<unsigned char*>(ptx);
    const unsigned char *cp = reinterpret_cast<const unsigned char*>(ctx);
    const unsigned char *ap = reinterpret_cast<const unsigned char*>(auth);
    const unsigned char *tp = reinterpret_cast<const unsigned char*>(tag);

    unsigned char buffer[16];

    setLengthInfo(authLength, cryptLength);
    start();
    processAuthData(ap);
    decryptAndProcessData(pp, cp);
    finish();
    getTag(buffer);
    bool result = true;
    for(unsigned i=0; i<GCMBlockSize; i++) {
        if(tp[i] != buffer[i]) {
            result = false;
            break;
        }
    }
    return result;
}

void AesGcm::start() {
    aes.ecbEncrypt(firstEctr, iv.getData());
    // Digest first block containg implicit info to authenticate
    multH(workingTag, slotInfo);
}

void AesGcm::processAuthData(const unsigned char *auth) {
    unsigned dataLength = authLength;
    unsigned blockLength;
    unsigned i=0;
    while(dataLength > 0) {
        blockLength = std::min(dataLength, GCMBlockSize);
        xorBytes(workingTag, workingTag, &auth[i], blockLength);
        multH(workingTag, workingTag);

        i+=blockLength;
        dataLength -= blockLength;
    }
}

void AesGcm::encryptAndProcessData(unsigned char *ctx, const unsigned char *ptx) {
    unsigned dataLength = cryptLength;
    unsigned blockLength;
    unsigned i=0;

    IV ctr(iv);
    aes.ctrXcrypt(++ctr, ctx, ptx, cryptLength);

    while(dataLength > 0) {
        blockLength = std::min(dataLength, GCMBlockSize);
        xorBytes(workingTag, workingTag, &ctx[i], blockLength);
        multH(workingTag, workingTag);

        i+=blockLength;
        dataLength -= blockLength;
    }
}

void AesGcm::decryptAndProcessData(unsigned char *ptx, const unsigned char *ctx) {
    unsigned dataLength = cryptLength;
    unsigned blockLength;
    unsigned i=0;

    while(dataLength > 0) {
        blockLength = std::min(dataLength, GCMBlockSize);
        xorBytes(workingTag, workingTag, &ptx[i], blockLength);
        multH(workingTag, workingTag);

        i+=blockLength;
        dataLength -= blockLength;
    }

    IV ctr(iv);
    aes.ctrXcrypt(++ctr, ptx, ctx, cryptLength);

}

void AesGcm::finish() {
    xorBytes(workingTag, workingTag, lengthInfo, GCMBlockSize);
    multH(workingTag, workingTag);
    xorBytes(workingTag, workingTag, firstEctr, GCMBlockSize);
}

} //namespace mxnet
