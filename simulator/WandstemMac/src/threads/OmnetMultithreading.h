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

#include <omnetpp.h>
#include <functional>
#include "miosix_utils_sim.h"
#include <miosix.h>
#include "OmnetThread.h"

using namespace omnetpp;

class OmnetMultithreading : public cSimpleModule
{
public:
    OmnetMultithreading() : cSimpleModule(coroutineStack) {}

    /**
     * Create a new thread (actually run in a new OMNET++ coroutine).
     * It must also be explicitly started.
     * 
     * @param f      function to be executed by the new thread
     * @param nodeId network id of node starting the thread
     */
    OmnetThread* create(std::function<void()> f);

    /**
     * Create and start a new thread (actually run in a new
     * OMNET++ coroutine).
     * 
     * @param f      function to be executed by the new thread
     * @param nodeId network id of node starting the thread
     */
    OmnetThread* createStart(std::function<void()> f);

private:
    /**
     * Build the OMNET++ module that will be executed in a new coroutine.
     * @param f      function to be executed by the new thread
     */
    cModule* buildModule(std::function<void()> f);

    static const int coroutineStack = 32 * 1024;
    static unsigned int threadsCount;
};

