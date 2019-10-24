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

#include "bitwise_ops.h"
#include <limits>
#include <cstring>
#include <cstdint>
#include <string>

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
    /**
     * Default constructor for RuntimeBitset used for putting a RuntimeBitset
     * into an stl container
     */
    RuntimeBitset() :
        bitCount(0),
        byteSize(0),
        content(nullptr)
#ifdef _ARCH_CORTEXM3_EFM32GG
        , bbData(nullptr)
#endif
    {}

    /**
     * Creates an uninitialized array of size bits
     * @param size the array dimension
     */
    explicit RuntimeBitset(std::size_t size) :
        bitCount(size),
        byteSize((size + 7) / 8),
        content(new uint8_t[byteSize])
#ifdef _ARCH_CORTEXM3_EFM32GG
    , bbData(reinterpret_cast<unsigned*>(((reinterpret_cast<unsigned long>(content) - sramBase) << 5) + bitBandBase))
#endif
    {
        // Make sure that size is a multiple of 8, otherwise the RuntimeBitset won't work
        assert((size & 0b111) == 0);
    }

    /**
     * Creates an array of size bits, initializing it with a given value.
     * @param size the array dimension
     * @param init the value with which it is initialized
     */
    RuntimeBitset(std::size_t size, bool init) : RuntimeBitset(size) {
        // Make sure that size is a multiple of 8, otherwise the RuntimeBitset won't work
        assert((size & 0b111) == 0);
        memset(content, init? ~0 : 0, this->size());
    }

    RuntimeBitset(const RuntimeBitset& other) :
        bitCount(other.bitCount),
        byteSize(other.byteSize),
        content(new uint8_t[byteSize])
#ifdef _ARCH_CORTEXM3_EFM32GG
        , bbData(reinterpret_cast<unsigned*>(((reinterpret_cast<unsigned long>(content) - sramBase) << 5) + bitBandBase))
#endif
    {
        memcpy(content, other.content, byteSize);
    }

    RuntimeBitset(RuntimeBitset&& other) :
        bitCount(other.bitCount),
        byteSize(other.byteSize),
        content(other.content)
#ifdef _ARCH_CORTEXM3_EFM32GG
        , bbData(other.bbData)
#endif
    {
        other.content = nullptr;
#ifdef _ARCH_CORTEXM3_EFM32GG
        other.bbData = nullptr;
#endif
    }

    RuntimeBitset& operator=(const RuntimeBitset& other) {
        bitCount = other.bitCount;
        byteSize = other.byteSize;
        delete[] content;
        content = new uint8_t[byteSize];
        memcpy(content, other.content, byteSize);
#ifdef _ARCH_CORTEXM3_EFM32GG
        bbData = reinterpret_cast<unsigned*>(((reinterpret_cast<unsigned long>(content) - sramBase) << 5) + bitBandBase);
#endif
        return *this;
    }

    RuntimeBitset& operator=(RuntimeBitset&& other) {
        bitCount = other.bitCount;
        byteSize = other.byteSize;
        delete[] content;
        content = other.content;
        other.content = nullptr;
#ifdef _ARCH_CORTEXM3_EFM32GG
        bbData = other.bbData;
        other.bbData = nullptr;
#endif
        return *this;
    }

    virtual ~RuntimeBitset() {
        delete[] content;
    }

    class Bit {
    public:

#ifndef _ARCH_CORTEXM3_EFM32GG
        Bit(uint8_t* content, bool el, std::size_t idx, unsigned char bitPow) : content(content), value(el), idx(idx), bitPow(bitPow)
#else
        Bit(unsigned* content, bool el, std::size_t idx) : content(content), value(el), idx(idx)
#endif
    {}

        /*operator const bool& () const {
            return value;
        }*/

        operator bool() const { return value; } // For use on RHS of assignment

        Bit& operator=(const bool& rhs) {
            if (rhs)
#ifndef _ARCH_CORTEXM3_EFM32GG
                content[idx] |= b0 >> bitPow;
            else
                content[idx] &= ~(b0 >> bitPow);
#else
                content[idx] = 1;
            else
                content[idx] = 0;
#endif
            value = rhs;
            return *this;
        }

        bool get() const { return value; }
        bool getRaw() const { return content[idx]; }
    private:
#ifdef _ARCH_CORTEXM3_EFM32GG
        unsigned* const content;
#else
        uint8_t* const content;
#endif
        bool value;
        std::size_t idx;
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
        if (i < bitCount)
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
        if (i < bitCount)
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
        memset(content, value? ~0 : 0, size());
    }

    /**
     * @return true if the BitVector is empty
     */
    bool empty();

    /**
     * Accesses the memory area behind the array directly
     * @return a pointer to the memory area
     */
    uint8_t* data() const { return content; }

    /**
     * The size of the array in bytes
     * @return the dimension of the bitset in bytes
     */
    std::size_t size() const {
        return byteSize;
    }

    /**
     * The quantity of bits stored in the array
     * @return the number of bits in the array
     */
    std::size_t bitSize() const { return bitCount; }

    bool operator ==(const RuntimeBitset& other) const {
        return other.bitCount == bitCount && memcmp(content, other.content, size());
    }
    bool operator !=(const RuntimeBitset& other) const {
        return !(*this == other);
    }

    /**
     * Debug method used to print the bitmap content as a string of 0s and 1s
     */
    operator std::string() const {
        std::string result;
        result.reserve(bitCount);
        for (unsigned i = 0; i < bitCount; i++) result+=this->operator[](i) ? '1' : '0';
        return result;
    }
private:
    std::size_t bitCount;
    std::size_t byteSize;
    uint8_t* content;
#ifdef _ARCH_CORTEXM3_EFM32GG
    unsigned* bbData;
#endif
    /**
     * Number of LSBs used to address the bit within the array element
     */
    static const uint8_t shiftDivisor = BitwiseOps::bitsForRepresentingCountConstexpr(std::numeric_limits<uint8_t>::digits);
#ifndef _ARCH_CORTEXM3_EFM32GG
    /**
     * Most significant bit in an array element, used as index 0
     */
    static const uint8_t b0 = ((uint8_t) 1) << (std::numeric_limits<uint8_t>::digits - 1);
    /**
     * Bitwise mask for spllitting an index between array index (MSBs)
     * and index within the array (shiftDivisor MSBs).
     */
    static const uint8_t indexSplitterMask = static_cast<uint8_t>((1 << shiftDivisor) - 1);
#else
    static const unsigned long sramBase = 0x20000000;
    static const unsigned long bitBandBase = 0x22000000;
#endif
};
} /* namespace mxnet */
