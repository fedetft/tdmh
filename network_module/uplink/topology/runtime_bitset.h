/***************************************************************************
 *   Copyright (C)  2018 by Polidori Paolo                                 *
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

#include "../../bitwise_ops.h"
#include <limits>
#include <cstring>

namespace mxnet {
class RuntimeBitset {
public:
    typedef unsigned long word_t;
    RuntimeBitset() = delete;
    explicit RuntimeBitset(std::size_t size) :
        bitCount(size),
        wordCount(((size - 1) >> shiftDivisor) + 1),
        content(new word_t[wordCount])
#ifdef _ARCH_CORTEXM3_EFM32GG
    , bbData((data - sramBase) << 5 + bitBandBase)
#endif
    {}

    RuntimeBitset(std::size_t size, bool init) : RuntimeBitset(size) {
        memset(content, init? ~0 : 0, wordCount);
    }

    virtual ~RuntimeBitset() {
        delete content;
    }
    RuntimeBitset(const RuntimeBitset& other) = delete;

    class Bit {
    public:

#ifndef _ARCH_CORTEXM3_EFM32GG
        Bit(word_t* content, bool el, std::size_t wordIdx, unsigned char bitPow) : content(content), value(el), wordIdx(wordIdx), bitPow(bitPow)
#else
        Bit(word_t* content, bool el, std::size_t wordIdx) : content(content), value(el), wordIdx(wordIdx)
#endif
    {}

        /*operator const bool& () const {
            return value;
        }*/

        operator bool() const { return value; } // For use on RHS of assignment

        bool& operator=(const bool& rhs) {
            if (rhs)
#ifndef _ARCH_CORTEXM3_EFM32GG
                content[wordIdx] |= 1 << bitPow;
            else
                content[wordIdx] &= ~(1 << bitPow);
#else
                content[wordIdx] = 1;
            else
                content[wordIdx] = 0;
#endif
        }
    private:
        word_t* const content;
        bool value;
        std::size_t wordIdx;
#ifndef _ARCH_CORTEXM3_EFM32GG
        unsigned char bitPow;
#endif
    };

    const Bit operator[](int i) const {
#ifdef _ARCH_CORTEXM3_EFM32GG
        if (i < size)
            return Bit(bbData, bbData[i], i);
        return Bit(bbData, false, 0);
#else
        if (i < bitCount) {
            unsigned char bitPow = i & mask;
            return Bit(content, content[i >> shiftDivisor] & (1 << bitPow) > 0, i >> shiftDivisor, bitPow);
        }
        return Bit(content, false, 0, 0);
#endif
    }

    Bit operator[](int i) {
#ifdef _ARCH_CORTEXM3_EFM32GG
        if (i < size)
            return Bit(bbData, bbData[i], i);
        return Bit(bbData, false, 0);
#else
        if (i < bitCount) {
            unsigned char bitPow = i & mask;
            return Bit(content, content[i >> shiftDivisor] & (1 << bitPow) > 0, i >> shiftDivisor, bitPow);
        }
        return Bit(content, false, 0, 0);
#endif
    }

    void setAll(bool value) {
        memset(content, value? ~0 : 0, wordCount);
    }

    word_t* data() { return content; }
    std::size_t size() const { return bitCount; }
    std::size_t wordSize() { return wordCount; }

    bool operator ==(const RuntimeBitset& b) const {
        return b.bitCount == bitCount && memcmp(content, b.content, wordCount);
    }
private:
    std::size_t bitCount;
    std::size_t wordCount;
    word_t* const content;
#ifdef _ARCH_CORTEXM3_EFM32GG
    word_t* const bbData;
#endif
    static const unsigned char shiftDivisor = BitwiseOps::bitsForRepresentingCountConstexpr(std::numeric_limits<word_t>::digits);
#ifndef _ARCH_CORTEXM3_EFM32GG
    static const unsigned char mask = static_cast<unsigned char>(static_cast<unsigned char>(~0) << shiftDivisor);
#else
    static const unsigned long sramBase = 0x20000000;
    static const unsigned long bitBandBase = 0x22000000;
#endif
};
} /* namespace mxnet */
