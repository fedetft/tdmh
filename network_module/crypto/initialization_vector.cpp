#include "initialization_vector.h"

namespace mxnet {

IV& IV::operator++() {
    for(unsigned i=15; i>=0; i--) {
        data[i]++;
        if (data[i] != 0) break;
    }
    return *this;
}


IV IV::operator++(int) {
    IV orig(*this);

    for(unsigned i=15; i>=0; i--) {
        data[i]++;
        if (data[i] != 0) break;
    }

    return orig;
}

} // namespace mxnet
