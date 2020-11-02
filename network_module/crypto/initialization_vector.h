/***************************************************************************
 *   Copyright (C)  2020 by Valeria Mazzola                                *
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 *   This program is distributed in the hope that it will be useful,       *
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of        *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
 *   GNU General Public License for more details.                          *
 *                                                                         *
 *   As a special exception, if other files instantiate templates or use   *
 *   macros or inline functions from this file, or you compile this file   *
 *   and link it with other works to produce a work based on this file,    *
 *   this file does not by itself cause the resulting work to be covered   *
 *   by the GNU General Public License. However the source code for this   *
 *   file must still be made available in accordance with the GNU General  *
 *   Public License. This exception does not invalidate any other reasons  *
 *   why a work based on this file might be covered by the GNU General     *
 *   Public License.                                                       *
 *                                                                         *
 *   You should have received a copy of the GNU General Public License     *
 *   along with this program; if not, see <http://www.gnu.org/licenses/>   *
 ***************************************************************************/

#pragma once
#include <string.h>

namespace mxnet {

class IV {
public:
    /**
     * Default constructor: IV = 0
     */
    IV() {
        memset(this->data, 0, 16);
    }

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

    /* 128-bit increment (modulo 2^128) */
    IV& operator++();
    IV operator++(int);

    void clear() { memset(data, 0, 16); }
    const unsigned char* getData() const { return data; }
private:
    unsigned char data[16];
};

} // namespace mxnet
