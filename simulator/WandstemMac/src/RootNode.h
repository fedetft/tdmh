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

#pragma once

#include "NodeBase.h"
#include "network_module/tdmh.h"
#include <thread>
#include <atomic>

/**
 * The Root Node of the Wandstem Mac.
 * Is the time synchronization reference and handles TDMA schedules
 */
class RootNode : public NodeBase
{
public:
    RootNode() : quit(false) {};

protected:
    virtual void activity();
    void application();

    /* Pointer to tdmh class for opening streams */
    mxnet::MediumAccessController* tdmh = nullptr;
    std::atomic<bool> quit;
};
