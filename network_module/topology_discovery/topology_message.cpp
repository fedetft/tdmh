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
#include "topology_message.h"
#include <cstring>
#include <stdexcept>
#include <limits>

namespace miosix {

std::vector<unsigned char> NeighborMessage::getPkt()  {
    auto bitSize = 280;
    auto byteSize = (bitSize - 1) / (std::numeric_limits<unsigned char>::digits) + 1;
    std::vector<unsigned char> retval(byteSize);
    auto pkt = retval.data();
    pkt[0] = sender;
    pkt[1] = hop;
    pkt[2] = assignee;
    for (auto n : neighbors) {
        pkt[n/8 + 3] |= 1 << (7 - (n % 8));
    }
    return retval;
}

NeighborMessage* NeighborMessage::fromPkt(unsigned char* pkt, unsigned short bitLen, unsigned short startBit) {
    auto size = 280;
    if (bitLen < size)
        return nullptr; //throw std::runtime_error("Invalid data, unparsable packet");
    auto i = (startBit + 7) / 8;
    auto sender = pkt[i++];
    auto hop = pkt[i++];
    auto assignee = pkt[i++];
    std::vector<unsigned short> neighbors;
    for (unsigned short j = 0; j < 255 - 1; j++)
        if (pkt[i + j / 8] & (1 << (7 - (j % 8))))
            neighbors.push_back(j);
    return new NeighborMessage(256, 8, 8, sender, hop, assignee, std::move(neighbors));
}

bool NeighborMessage::operator ==(const NeighborMessage &b) const {
    return this->sender == b.sender && this->assignee == b.assignee && this->hop == b.hop && this->neighbors == b.neighbors;
}

} /* namespace miosix */
