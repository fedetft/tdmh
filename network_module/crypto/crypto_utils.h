#pragma once

namespace mxnet {

inline void xorBytes(void *dst, const void *op1, const void *op2, unsigned int length) {
    auto d = reinterpret_cast<unsigned char*>(dst);
    auto o1 = reinterpret_cast<const unsigned char*>(op1);
    auto o2 = reinterpret_cast<const unsigned char*>(op2);

    for (unsigned i=0; i<length; i++) {
        d[i] = o1[i] ^ o2[i];
    }
}

} //namespace mxnet
