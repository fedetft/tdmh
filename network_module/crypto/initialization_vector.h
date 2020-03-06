#pragma once
#include <string.h>

namespace mxnet {

class IV {
public:
    IV(const void *data) {
        memcpy(this->data, data, 16);
    }

    IV(const IV& other) {
        if (this != &other) {
            memcpy(data, other.getData(), 16);
        }
    }

    IV& operator=(const IV& other) {
        if (this != &other) {
            memcpy(data, other.getData(), 16);
        }
        return *this;
    }

    IV& operator++();
    IV operator++(int);

    void clear() { memset(data, 0, 16); }
    const unsigned char* getData() const { return data; }
private:
    unsigned char data[16];
};

} // namespace mxnet
