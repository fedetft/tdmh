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

namespace mxnet {

class SerializableMessage {
public:
    virtual ~SerializableMessage() {};
    virtual std::vector<unsigned char> serialize() {
        std::vector<unsigned char> retval(getSize());
        serialize(retval);
        return retval;
    }
    virtual void serialize(std::vector<unsigned char>& pkt) {
        assert(pkt.size() >= getSize());
        serialize(pkt.begin());
    }
    virtual void serialize(std::vector<unsigned char>::iterator pkt);
    //static SerializableMessage deserialize(std::vector<unsigned char>& pkt);
    //static SerializableMessage deserialize(const unsigned char* pkt);
    virtual std::size_t getSize() = 0;
protected:
    SerializableMessage() {};
};

class BitSerializableMessage {
public:
    virtual ~BitSerializableMessage() {};
    virtual std::vector<unsigned char> serialize() {
        std::vector<unsigned char> retval(getSize());
        serialize(retval.begin(), 0);
        return retval;
    }
    virtual void serialize(std::vector<unsigned char>& pkt, unsigned char startBit) {
        serialize(pkt.begin(), startBit);
    }
    virtual void serialize(std::vector<unsigned char>::iterator pkt, const unsigned char startBit) = 0;
    //static SerializableMessage deserialize(std::vector<unsigned char>& pkt, const unsigned char startBit);
    //static SerializableMessage deserialize(const unsigned char* pkt, const unsigned char startBit);
    virtual std::size_t getSize() = 0;
    virtual std::size_t getBitSize() = 0;
protected:
    BitSerializableMessage() {};
};

} /* namespace mxnet */
