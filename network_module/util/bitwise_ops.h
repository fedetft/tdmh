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

#include <limits>
#include <type_traits>
#include <cassert>

namespace mxnet {

struct BitwiseOps {
    /**
     * Sets the least significant bits of a value, taking them from the least significant bits of another.
     * @tparam T the type of both the value to which the bits are modified and the source one.
     * @param origValue the value to which the bits will be set.
     * @param value the value from which the bits are taken
     * @param length the quantity of modified bits
     * @return the modified value
     */
    template<typename T>
    static typename std::enable_if<std::is_integral<T>::value>::type
    setRightmost(T origValue, T value, unsigned short length) {
        T mask = (~0) << length;
        return (origValue & mask) | (value & ~mask);
    }

    /**
     * Sets the most significant bits of a value, taking them from the most significant bits of another.
     * @tparam T the type of both the value to which the bits are modified and the source one.
     * @param origValue the value to which the bits will be set.
     * @param value the value from which the bits are taken
     * @param length the quantity of modified bits
     * @return the modified value
     */
    template<typename T>
    static typename std::enable_if<std::is_integral<T>::value>::type
    setLeftmost(T origValue, T value, unsigned short length) {
        T mask = (~0) >> length;
		return (origValue & mask) | (value & ~mask);
    }

    /**
     * Populates a part of an array starting from a certain bit with an array value.
     * Both must be aligned to the first array value and the last value to the most significant bit.
     * @tparam T the type of both the value to which the bits are modified and the source one.
     * @param arr the target array to which the bits will be set.
     * @param arrLen the number of elements in the target array.
     * @param value the source array from which the bits are taken.
     * @param valLen the number of elements in the source array.
     * @param startPos the start position from which the values are set, in bits.
     * @param length the quantity of modified bits.
     */
    template<typename T>
    static void bitwisePopulateBitArrTop(
            typename std::enable_if<std::is_integral<T>::value, T>::type* arr,
            unsigned short arrLen,
            typename std::enable_if<std::is_integral<T>::value, T>::type* value,
            unsigned short valLen,
            unsigned short startPos,
            unsigned short length) {
        assert(startPos + length <= std::numeric_limits<T>::digits * arrLen);
        unsigned short startBit = startPos % (std::numeric_limits<T>::digits);
        unsigned short startIndex = startPos / (std::numeric_limits<T>::digits);
        auto endBit = length  + startBit;
        unsigned short cellsLen = (endBit - 1) / (std::numeric_limits<T>::digits);
        unsigned short trailingBits = (std::numeric_limits<T>::digits + endBit - 1) %
			std::numeric_limits<T>::digits + 1;
        T ones = ~((T) 0);
        auto leftShiftCount = std::numeric_limits<T>::digits - startBit;
        if (cellsLen > 0) {
			auto lastCell = startIndex + cellsLen;
            unsigned short j = startIndex;
            unsigned short i = 0;
            arr[j] = (arr[j] & (ones << leftShiftCount)) | //masking the bits to keep
                    (value[i] >> startBit);
            //cells populated with vals
            for (j++; i < valLen - 1 && j < lastCell; j++) {
                arr[j] = value[i] << leftShiftCount; //preceding value
                arr[j] |= value[++i] >> startBit;
            }
            //check if we are at the last cell of arr
            T remMask = ones >> trailingBits;
            if (i < valLen - 1) {// <=> j == lastCell
                arr[j] = (arr[j] & remMask) | ((((value[i] << leftShiftCount) | (value[i + 1] >> startBit))) & ~remMask);
            } else { // <=> i == (valLen - 1): clear the last arr cell
                arr[lastCell] &= remMask;
                if (j < lastCell) { //last value cell in middle of arr
                    arr[j++] = value[i] << leftShiftCount;
                    for (; j < lastCell; j++) //clear the non-populated cells
                        arr[j] = 0;
                } else //last value cell, last arr cell
                    arr[lastCell] |= (value[i] << leftShiftCount) & ~remMask;
            }
        } else { //single cell of arr impacted
            T mask = static_cast<T>(ones << (std::numeric_limits<T>::digits - length)) >> startBit;
            arr[startIndex] = (arr[startIndex] & ~mask) |
                    ((value[0] >> startBit) & mask);
        }
    }

    /**
     * Populates a part of an array starting from a certain bit with an array value.
     * Both must be aligned to the first array value and the last value to the most significant bit.
     * @tparam T the type of both the value to which the bits are modified and the source one.
     * @param arr the target array to which the bits will be set.
     * @param arrLen the number of elements in the target array.
     * @param value the source array from which the bits are taken.
     * @param valLen the number of elements in the source array.
     * @param startPos the start position from which the values are set, in bits.
     * @param length the quantity of modified bits.
     * @param valStartPos the number of bits of the source array that need to be skipped.
     */
    template<typename T>
    static void bitwisePopulateBitArrTop(
            typename std::enable_if<std::is_integral<T>::value, T>::type* arr,
            unsigned short arrLen,
            typename std::enable_if<std::is_integral<T>::value, T>::type* value,
            unsigned short valLen,
            unsigned short startPos,
            unsigned short length,
            unsigned short valStartPos) {
        assert(startPos + length <= std::numeric_limits<T>::digits * arrLen);
        unsigned short startBit = startPos % (std::numeric_limits<T>::digits);
        unsigned short startIndex = startPos / (std::numeric_limits<T>::digits);
        auto endBit = length  + startBit;
        unsigned short cellsLen = (endBit - 1) / (std::numeric_limits<T>::digits);
        unsigned short trailingBits = (std::numeric_limits<T>::digits + endBit - 1) %
			std::numeric_limits<T>::digits + 1;
        T ones = ~((T) 0);
        auto leftShiftCount = std::numeric_limits<T>::digits - startBit;
        unsigned short valStartBit = valStartPos % (std::numeric_limits<T>::digits);
        unsigned short valEndBit = std::numeric_limits<T>::digits - valStartBit;
		auto getVal = [valStartBit, valEndBit, valLen, &value](unsigned short i_idx){
			return static_cast<T>(value[i_idx] << valStartBit | ((i_idx < valLen - 1)? value[i_idx + 1] : 0) >> valEndBit);
		};
        if (cellsLen > 0) {
			auto lastCell = startIndex + cellsLen;
            unsigned short j = startIndex;
            unsigned short i = valStartPos / (std::numeric_limits<T>::digits);
            arr[j] = (arr[j] & (ones << leftShiftCount)) | //masking the bits to keep
                    (getVal(i) >> startBit);
            //cells populated with vals
            for (j++; i < valLen - 1 && j < lastCell; j++) {
                arr[j] = getVal(i) << leftShiftCount;
                arr[j] |= getVal(++i) >> startBit;
            }
            //check if we are at the last cell of arr
			T remMask = ones >> trailingBits;
            if (i < valLen - 1) { //didn't finish consuming value, need to populate properly last cell
                arr[j] = (arr[j] & remMask) | ((((getVal(i) << leftShiftCount ) | (getVal(i + 1) >> startBit))) & ~remMask);
            } else { // <=> i == (valLen - 1): clear the last arr cell
                arr[lastCell] &= remMask;
                if (j < lastCell) { //last value cell in middle of arr
                    arr[j++] = getVal(i) << leftShiftCount;
                    for (; j < lastCell; j++) //clear the non-populated cells
                        arr[j] = 0;
                } else //last value cell, last arr cell
                    arr[lastCell] |= (getVal(i) << leftShiftCount) & ~remMask;
            }
        } else { //single cell of arr impacted
            T mask = static_cast<T>(ones << (std::numeric_limits<T>::digits - length)) >> startBit;
            arr[startIndex] = (arr[startIndex] & ~mask) |
                    ((getVal(valStartPos / std::numeric_limits<T>::digits) >> startBit) & mask);
        }
    }

    /**
     * Populates a part of an array starting from a certain bit with an array value.
     * Both must be aligned to the first array value and the last value to the most significant bit.
     * This function is safe for upcasting and downcasting.
     * @tparam T the type of the value to which the bits are modified.
     * @tparam T the type of the value to which the bits are taken from.
     * @param arr the target array to which the bits will be set.
     * @param arrLen the number of elements in the target array.
     * @param value the source array from which the bits are taken.
     * @param valLen the number of elements in the source array.
     * @param startPos the start position from which the values are set, in bits.
     * @param length the quantity of modified bits.
     */
    template<typename T, typename V>
    static void bitwisePopulateBitArr(
            typename std::enable_if<std::is_integral<T>::value>::type* arr,
            unsigned short arrLen,
            typename std::enable_if<std::is_integral<V>::value>::type* value,
            unsigned short valLen,
            unsigned short startPos,
            unsigned short length) {
        if (sizeof(T) < sizeof(V)) //downscaling to array of smaller data
            bitwisePopulateBitArr(arr, arrLen, ((T*) value), valLen * sizeof(V) / sizeof(T), length);
        else
            bitwisePopulateBitArr(((V*) arr), arrLen * sizeof(T) / sizeof(V), value, valLen, length);
    }

    /**
     * Returns the number of bits necessary for representing an amount of elements (logarithm base 2)
     * using a fast algorithm (shift and bitwise ops).
     * @tparam T the type of the elements count.
     * @param count the number of elements to be represented.
     */
    template<typename T, typename std::enable_if<std::is_integral<T>::value, T>::type = 0>
    static unsigned char bitsForRepresentingCount(T count) {
        T temp = count;
        unsigned char retval = 0;
        while (temp >>= 1) ++retval;
        return count ^ (1 << retval)? retval + 1: retval;
    }

    /**
     * Returns the number of bits necessary for representing an amount of elements (logarithm base 2)
     * and can be run at compile time.
     * @tparam T the type of the elements count.
     * @param count the number of elements to be represented.
     */
    template<typename T, typename std::enable_if<std::is_integral<T>::value, T>::type = 0>
    constexpr static unsigned bitsForRepresentingCountConstexpr(T count) {
        return count == 1 ? 0 : floorlog2(count - 1) + 1;
    }

private:
    template<typename T, typename std::enable_if<std::is_integral<T>::value, T>::type = 0>
    constexpr static unsigned floorlog2(T x)
    {
        return x == 1 ? 0 : 1 + floorlog2(x >> 1);
    }
};
}
