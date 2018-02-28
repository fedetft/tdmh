//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU Lesser General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
// 
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU Lesser General Public License for more details.
// 
// You should have received a copy of the GNU Lesser General Public License
// along with this program.  If not, see http://www.gnu.org/licenses/.
// 

#include "Node.h"
#include "network_module/mediumaccesscontroller.h"
#include "network_module/macround/dynamicmacround.h"
#include "network_module/network_configuration.h"
#include <iostream>

Define_Module(Node);

void Node::activity()
{
    using namespace miosix;
    print_dbg("Dynamic node 1 hop 1\n");
    const NetworkConfiguration config(3, 256, address, 6, 1, 2450, 2, 3, 3);
    MediumAccessController controller(new DynamicMACRound::DynamicMACRoundFactory(), &config);
    controller.run();
}
