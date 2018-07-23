/***************************************************************************
 *   Copyright (C)  2018 by Federico Amedeo Izzo                           *
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

#include "master_schedule_information.h"

namespace mxnet {

class MasterTopologyContext;
class MasterStreamManagementContext;
class StreamManagementElement;
class MACContext;


class ScheduleComputation{
public:
    ScheduleComputation(MACContext& mac_ctx, MasterTopologyContext& topology_ctx,
                        MasterStreamManagementContext& stream_ctx);
    virtual ~ScheduleComputation() {};
    
    void run();
    
protected:
    // References to other classes
    MasterTopologyContext& topology_ctx;
    MasterStreamManagementContext& stream_ctx;
    MACContext& mac_ctx; //TODO is really needed?
    std::vector<StreamManagementElement*> stream_list;
    
private:
    static void threadLauncher(void *arg)
    {
        reinterpret_cast<ScheduleComputation*>(arg)->run();
    }
};

class Router {
public:
    Router(MasterTopologyContext& topology_ctx, MasterStreamManagementContext& stream_ctx, bool multipath, int more_hops) : 
    topology_ctx(topology_ctx), stream_ctx(stream_ctx) {};
    virtual ~Router() {};
    
    std::vector<StreamManagementElement*> run();
    
    std::vector<StreamManagementElement*> breadthFirstSearch(StreamManagementElement& stream);
    
protected:
    int multipath;
    // References to other classes
    MasterTopologyContext& topology_ctx;
    MasterStreamManagementContext& stream_ctx;
};
}