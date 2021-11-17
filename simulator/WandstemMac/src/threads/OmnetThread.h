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
#include "NodeBase.h"

using namespace omnetpp;

/**
 * A generic node to simulate threads during simulations.
 * The activity() method of this node is run in a new OMNET++ coroutine.
 * 
 * In order to create a new thread use the OmnetMultithreading factory.
 */
class OmnetThread : public NodeBase
{
public:
    OmnetThread() : NodeBase(), id(0), started(false) {}
    
    /**
     * Used by the factory to set the function to be called 
     * by the activity() method.
     */
    void setFunction(std::function<void()> f);

    /**
     * Set identifier of the new thread.
     */
    void setId(int identifier);

    /**
     * Get identifier of the new thread.
     */
    int getId();

    /**
     * Start the thread (coroutine).
     * OMNET++ will execute the initialize() and activity() methods.
     */
    void start();

protected:
    virtual void initialize() override;
    virtual void activity() override;

private:    
    std::function<void()> application; // function executed by the thread
    int id;                            // thread's identifier
    bool started;                      // true if start() has already been called
};

