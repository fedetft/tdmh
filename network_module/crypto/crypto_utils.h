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

namespace mxnet {

inline void xorBytes(void *dst, const void *op1, const void *op2, unsigned int length) {
    auto d = reinterpret_cast<unsigned char*>(dst);
    auto o1 = reinterpret_cast<const unsigned char*>(op1);
    auto o2 = reinterpret_cast<const unsigned char*>(op2);

    for (unsigned i=0; i<length; i++) {
        d[i] = o1[i] ^ o2[i];
    }
}

inline void secureClearBytes(void *bytes, unsigned int length) {
    auto p = reinterpret_cast<volatile unsigned char*>(bytes);
    for (unsigned i=0; i<length; i++) {
        p[i] = 0x00;
    }
}

} //namespace mxnet
