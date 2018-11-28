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

#include <vector>
#include <cassert>
#include <array>

namespace mxnet {

/**
 * This class is not meant to be used polymorphically. Do not upcast to this class.
 * 
 * Represents a serializable data structure, providing an interface with the basic methods for doing so.
 * The serialization output is a byte-aligned vector of bytes.
 * The complementary part for deserialization would need the addition of the following methods:
 *
 * static MyMessage deserialize(std::vector<unsigned char>& pkt);
 * static MyMessage deserialize(unsigned char* pkt);
 */
class SerializableMessage {
public:
    /**
     * Serializes the structure into a vector of `size()` bytes created on purpose.
     * @return the serialized structure.
     */
    virtual std::vector<unsigned char> serialize() const {
        std::vector<unsigned char> retval(size());
        serializeImpl(retval.data());
        return retval;
    }

    /**
     * Serializes the structure into the provided vector of bytes.
     * @param pkt the buffer in which the data will be serialized.
     */
    virtual void serialize(std::vector<unsigned char>& pkt) const {
        assert(pkt.size() >= size());
        serializeImpl(pkt.data());
    }

    /**
     * Serializes the structure into the provided array of bytes.
     * @param pkt the buffer in which the data will be serialized.
     */
    template<unsigned int N>
    void serialize(std::array<unsigned char, N>& pkt) const {
        assert(pkt.size() >= size());
        serializeImpl(pkt.data());
    }

    /**
     * Serializes the structure into the provided vector of bytes.
     * The pointer must reference a memory area of at least `size()` bytes
     * @param pkt the buffer in which the data will be serialized.
     */
    virtual void serializeImpl(unsigned char* pkt) const = 0;

    //static SerializableMessage deserialize(std::vector<unsigned char>& pkt);
    //static SerializableMessage deserialize(unsigned char* pkt);

    /**
     * Gets the size of the serializable data structure
     * @return the size of the `serialize()` return value.
     */
    virtual std::size_t size() const = 0;
protected:
    SerializableMessage() {}
    //Explicit copy constructor in base class causes slicing attempts to produce compiler errors
    //https://stackoverflow.com/questions/36473354/prevent-derived-class-from-casting-to-base
    explicit SerializableMessage(const SerializableMessage&) {}
};

/**
 * Represents a bit-serializable data structure interface.
 * It has methods for producing or filling a bit-aligned vector of bytes.
 * The complementary part for deserialization would need the addition of the following methods:
 *
 * static MyMessage deserialize(std::vector<unsigned char>& pkt, const unsigned char startBit);
 * static MyMessage deserialize(unsigned char* pkt, const unsigned char startBit);
 */
class BitSerializableMessage {
public:
    virtual ~BitSerializableMessage() {};
    /**
     * Serializes the structure into a vector of `size()` bytes created on purpose.
     * @return the serialized structure.
     */
    virtual std::vector<unsigned char> serialize() const {
        std::vector<unsigned char> retval(size());
        serializeImpl(retval.data(), 0);
        return retval;
    }

    /**
     * Serializes the structure into the provided vector of bytes.
     * @param pkt the buffer in which the data will be serialized.
     */
    virtual void serialize(std::vector<unsigned char>& pkt, unsigned short startBit) const {
        serializeImpl(pkt.data(), startBit);
    }

    /**
     * Serializes the structure into the provided array of bytes.
     * @param pkt the buffer in which the data will be serialized.
     */
    template<unsigned int N>
    void serialize(std::array<unsigned char, N>& pkt, unsigned short startBit) const {
        assert(pkt.size() >= size());
        serializeImpl(pkt.data(), startBit);
    }

    /**
     * Serializes the structure into the provided memory area.
     * The pointer must reference a memory area of at least `(bitSize() + startBit) / 8` bytes
     * @param pkt the buffer in which the data will be serialized.
     * @param startBit the offset in bits, starting from 0, at which the data starts to be positioned.
     */
    virtual void serializeImpl(unsigned char* pkt, unsigned short startBit) const = 0;
    //static SerializableMessage deserialize(std::vector<unsigned char>& pkt, const unsigned char startBit);
    //static SerializableMessage deserialize(unsigned char* pkt, const unsigned char startBit);

    /**
     * Gets the size in bytes of the serializable data structure.
     * It is equal to 1 + ((`bitSize()` - 1) / 8), so:
     *  - from 1 to 8 bits, a byte
     *  - from 9 to 16 bits, 2 bytes
     *  - ...
     * @return the size, in bytes, of the `serialize()` return value.
     */
    virtual std::size_t size() const = 0;

    /**
     * Gets the size in bits of the serializable data structure.
     * @return the size, in bits, of the `serialize()` return value.
     */
    virtual std::size_t bitSize() const = 0;
protected:
    BitSerializableMessage() {};
};

} /* namespace mxnet */
