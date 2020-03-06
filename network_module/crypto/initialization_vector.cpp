#include "initialization_vector.h"

namespace mxnet {

IV& IV::operator++() {
    /* 128-bit increment (modulo 2^128)
     * NOTE: this implementation relies on little-endianness
     * NOTE: this implementation is not constant time
     * */
    auto *intData = reinterpret_cast<unsigned int*>(data);
    for (unsigned i=0; i<16/sizeof(unsigned int) ; i++) {
        intData[i]++;
        if (intData[i] != 0) break;
    }
    return *this;
}


IV IV::operator++(int) {
    IV orig(*this);

    auto *intData = reinterpret_cast<unsigned int*>(data);
    for (unsigned i=0; i<16/sizeof(unsigned int) ; i++) {
        intData[i]++;
        if (intData[i] != 0) break;
    }

    return orig;
}

} // namespace mxnet
