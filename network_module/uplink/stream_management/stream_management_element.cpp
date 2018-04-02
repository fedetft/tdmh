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

#include "stream_management_element.h"

namespace mxnet {

std::vector<unsigned char> StreamManagementElement::getPkt() {
    std::vector<unsigned char> retval(getSize());
    getPkt(retval);
    return retval;
}

void StreamManagementElement::getPkt(std::vector<unsigned char>& dest) {
    assert(dest.size() >= getSize());
    memcpy(dest.data(), content, getSize());
}

std::vector<StreamManagementElement*> StreamManagementElement::fromPkt(std::vector<unsigned char> pkt) {
    auto count = pkt.size() / getSize();
    std::vector<StreamManagementElement*> retval(count);
    for (int i = 0, bytes = 0; i < count; i++, bytes += getSize()) {
        auto* val = new StreamManagementElement();
        memcpy(val->content, pkt[bytes], getSize());
        retval[i] = val;
    }
    return retval;
}

} /* namespace mxnet */
