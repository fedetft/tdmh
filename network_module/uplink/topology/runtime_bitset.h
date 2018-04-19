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
/**
 * This class implements a bitset whose size is definable at runtime.
 * Its implementation varies if on ARM MCU (>M0) or others.
 * In ARM the bit banding memory area is used for bitwise fast access.
 * In other architectures the memory is allocated word by word (4B on 32 bits, 8B on 64 bits arches).
 * To achieve cross-compatibility, The values are set referencing the order used on ARM MCUs when using bit banding.
 * So bits are ordered from 0 to n while the vector address increases.
 * In ARM the same memory, accesses by-word uses b0 as the MSB and b31 as the LSB.
 * Thus this order is respected, in order to grant cross-compatibility.
 */
/**
 * Class representing a bit array of an arbitrary dimension.
 * It is designed to achieve space efficiency.
 * IN ARM architectures, in which the bit banding area is available, it uses such area.
 */
class RuntimeBitset {
public:
    typedef unsigned long word_t;
    RuntimeBitset() = delete;

    /**
     * Creates an uninitialized array of size bits
     * @param size the array dimension
     */
    explicit RuntimeBitset(std::size_t size) :
        bitCount(size),
        wordCount(((size - 1) >> shiftDivisor) + 1),
        content(new word_t[wordCount])
#ifdef _ARCH_CORTEXM3_EFM32GG
    , bbData((content - sramBase) << 5 + bitBandBase)
#endif
    {}

    /**
     * Creates an array of size bits, initializing it with a given value.
     * @param size the array dimension
     * @param init the value with which it is initialized
     */
    RuntimeBitset(std::size_t size, bool init) : RuntimeBitset(size) {
        memset(content, init? ~0 : 0, wordCount);
    }

    virtual ~RuntimeBitset() {
        delete content;
    }

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

        Bit& operator=(const bool& rhs) {
            if (rhs)
#ifndef _ARCH_CORTEXM3_EFM32GG
                content[wordIdx] |= b0 >> bitPow;
            else
                content[wordIdx] &= ~(b0 >> bitPow);
#else
                content[wordIdx] = 1;
            else
                content[wordIdx] = 0;
#endif
            value = rhs;
            return *this;
        }
    private:
        word_t* const content;
        bool value;
        std::size_t wordIdx;
#ifndef _ARCH_CORTEXM3_EFM32GG
        unsigned char bitPow;
#endif
    };

    /**
     * Accesses the i-th bit of the array
     * @param i the index to access
     */
    const Bit operator[](unsigned i) const {
#ifdef _ARCH_CORTEXM3_EFM32GG
        if (i < size)
            return Bit(bbData, bbData[i], i);
        return Bit(bbData, false, 0);
#else
        if (i < bitCount) {
            unsigned char bitPow = i & indexSplitterMask;
            auto index = i >> shiftDivisor;
            return Bit(content, (content[index] & (b0 >> bitPow)) > 0, index, bitPow);
        }
        return Bit(content, false, 0, 0);
#endif
    }

    /**
     * Accesses the i-th bit of the array
     * @param i the index to access
     */
    Bit operator[](unsigned i) {
#ifdef _ARCH_CORTEXM3_EFM32GG
        if (i < size)
            return Bit(bbData, bbData[i], i);
        return Bit(bbData, false, 0);
#else
        if (i < bitCount) {
            unsigned char bitPow = i & indexSplitterMask;
            auto index = i >> shiftDivisor;
            return Bit(content, (content[index] & (b0 >> bitPow)) > 0, index, bitPow);
        }
        return Bit(content, false, 0, 0);
#endif
    }

    /**
     * Reinitializes the whole vector with zeros or ones.
     * @param value the value to set
     */
    void setAll(bool value) {
        memset(content, value? ~0 : 0, wordCount);
    }

    /**
     * Accesses the memory area behind the array directly
     * @return a pointer to the memory area
     */
    word_t* data() const { return content; }

    /**
     * The quantity of bits stored in the array
     * @return the number of bits in the array
     */
    std::size_t size() const { return bitCount; }

    /**
     * The number of words used to store the array
     * @return the size of the array, in words
     */
    std::size_t wordSize() const { return wordCount; }

    bool operator ==(const RuntimeBitset& other) const {
        return other.bitCount == bitCount && memcmp(content, other.content, wordCount);
    }
    bool operator !=(const RuntimeBitset& other) const {
        return !(*this == other);
    }
    RuntimeBitset(const RuntimeBitset& other) :
        bitCount(other.bitCount),
        wordCount(other.wordCount),
        content(new word_t[wordCount])
#ifdef _ARCH_CORTEXM3_EFM32GG
        , bbData((content - sramBase) << 5 + bitBandBase)
#endif
    {
        memcpy(content, other.content, wordCount);
    }
    RuntimeBitset(RuntimeBitset&& other) = delete;
    RuntimeBitset& operator=(const RuntimeBitset& other) = delete;
    RuntimeBitset& operator=(RuntimeBitset&& other) = delete;
private:
    std::size_t bitCount;
    std::size_t wordCount;
    word_t* const content;
#ifdef _ARCH_CORTEXM3_EFM32GG
    word_t* const bbData;
#endif
    static const unsigned char shiftDivisor = BitwiseOps::bitsForRepresentingCountConstexpr(std::numeric_limits<word_t>::digits);
#ifndef _ARCH_CORTEXM3_EFM32GG
    static const word_t b0 = ((word_t) 1) << (std::numeric_limits<word_t>::digits - 1);
    static const unsigned char indexSplitterMask = static_cast<unsigned char>(static_cast<unsigned char>(~0) << shiftDivisor);
#else
    static const unsigned long sramBase = 0x20000000;
    static const unsigned long bitBandBase = 0x22000000;
#endif
};
} /* namespace mxnet */
