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
#include "packet.h"

namespace mxnet {

/**
 * This class is not meant to be used polymorphically. Do not upcast to this class.
 * 
 * Represents a serializable data structure, providing an interface with the basic methods for doing so.
 * The serialization output is a byte-aligned vector of bytes.
 * The complementary part for deserialization would need the addition of the following method:
 *
 * static MyMessage deserialize(Packet& pkt);
 */
class SerializableMessage {
public:
    /**
     * Serializes the structure into the provided Packet object.
     * @param pkt the object in which the data will be serialized.
     */
    virtual void serialize(Packet& pkt) const = 0;

    //static SerializableMessage deserialize(Packet& pkt);

    /**
     * Gets the size of the serializable data structure
     * @return the size of the `serialize()` return value.
     */
    virtual std::size_t size() const = 0;
    
    virtual ~SerializableMessage() {}
    
protected:
    SerializableMessage() {}
    //Explicit copy constructor in base class causes slicing attempts to produce compiler errors
    //https://stackoverflow.com/questions/36473354/prevent-derived-class-from-casting-to-base
    explicit SerializableMessage(const SerializableMessage&) {}
};

} /* namespace mxnet */
