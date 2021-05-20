/***************************************************************************
 *   Copyright (C)  2020 by Valeria Mazzola, Federico Terraneo             *
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

#include <cassert>
#include "crypto_utils.h"

namespace mxnet {

void xorBytes(void *dst, const void *op1, const void *op2, unsigned int length) {
    auto d = reinterpret_cast<unsigned int*>(dst);
    auto o1 = reinterpret_cast<const unsigned int*>(op1);
    auto o2 = reinterpret_cast<const unsigned int*>(op2);

    auto l = length / 4;
    for (unsigned i=0; i<l; i++) {
        *d++ = *o1++ ^ *o2++;
    }

    auto db = reinterpret_cast<unsigned char*>(d);
    auto o1b = reinterpret_cast<const unsigned char*>(o1);
    auto o2b = reinterpret_cast<const unsigned char*>(o2);
    
    l = length & 0b11;
    for (unsigned i=0; i<l; i++) {
        *db++ = *o1b++ ^ *o2b++;
    }
}

void secureClearBytes(void *bytes, unsigned int length) {
    auto p = reinterpret_cast<volatile unsigned int*>(bytes);

    auto l = length / 4;
    for (unsigned i=0; i<l; i++) {
        *p++ = 0;
    }
    
    auto pb = reinterpret_cast<volatile unsigned char*>(p);
    l = length & 0b11;
    for (unsigned i=0; i<l; i++) {
        *pb++ = 0;
    }
}

} //namespace mxnet
