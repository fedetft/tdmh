/***************************************************************************
 *   Copyright (C)  2019 by Federico Amedeo Izzo, Federico Terraneo        *
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

#include "../../util/runtime_bitset.h"
#include "../uplink_message.h"
#include <vector>
#include <utility>

namespace mxnet {

/**
 * NetworkTopology contains all the information about the network graph
 * in the Master node.
 */
class NetworkTopology {
public:
    NetworkTopology();

    void receivedMessage(unsigned char currentNode, unsigned char currentHop,
                         int rssi, RuntimeBitset senderTopology) {};

    void missedMessage(unsigned char sender) {};

    void handleForwardedTopologies(const ReceiveUplinkMessage& message) {};

    std::vector<std::pair<unsigned char, unsigned char>> getEdges() {
        std::vector<std::pair<unsigned char, unsigned char>> result;
        return result;
    };

private:

};

} /* namespace mxnet */
