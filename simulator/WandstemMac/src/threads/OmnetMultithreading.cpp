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

#include "OmnetMultithreading.h"

Define_Module(OmnetMultithreading);

using namespace miosix;
using namespace mxnet;

OmnetThread* OmnetMultithreading::create(std::function<void()> f) {
    cModule* simModule = buildModule(f);
    OmnetThread* simThread = dynamic_cast<OmnetThread*>(simModule);
    simThread->setFunction(f);
    simThread->setId(threadsCount);
    threadsCount++;
    return simThread;
}

OmnetThread* OmnetMultithreading::createStart(std::function<void()> f) {
    OmnetThread* simThread = create(f);
    simThread->start();
    return simThread;
}

cModule* OmnetMultithreading::buildModule(std::function<void()> f) {
    cModule* parent = getParentModule();
    if (!parent)
        print_dbg("[OmnetThread] Error : parent module not found \n");
    // else 
    //     print_dbg("[OmnetThread] Parent module %s \n", parent->getFullName());
    
    cModuleType* moduleType = cModuleType::get("wandstemmac.threads.OmnetThread");
    cModule* simModule = moduleType->create("sim_thread", parent);
    simModule->finalizeParameters();
    simModule->buildInside();
    
    return simModule;
}

unsigned int OmnetMultithreading::threadsCount = 0;