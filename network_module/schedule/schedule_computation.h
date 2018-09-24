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


#include "../uplink/stream_management/stream_management_context.h"
#include "../uplink/topology/mesh_topology_context.h"
#ifdef _MIOSIX
#include <miosix.h>
#else
#include <thread>
#include <mutex>
#include <condition_variable>
#endif
#include <list>
#include <vector>
#include <iterator>

namespace mxnet {

class MasterTopologyContext;
class StreamManagementElement;
class MACContext;

class ScheduleComputation {
    friend class Router;
public:
    ScheduleComputation(MACContext& mac_ctx, MasterMeshTopologyContext& topology_ctx);

    void startThread();
    
    void beginScheduling();
    
    void run();
    
    void addNewStreams(std::vector<StreamManagementElement>& smes);

    void open(StreamManagementElement sme);
    
protected:
    
    std::list<StreamManagementElement> scheduled_streams;
    // References to other classes
    MasterMeshTopologyContext& topology_ctx;
    MACContext& mac_ctx; //TODO is really needed?
#ifdef _MIOSIX
    miosix::Mutex sched_mutex;
    miosix::ConditionVariable sched_cv;
    miosix::Thread* scthread = NULL;
private:
    static void threadLauncher(void *arg) {
        reinterpret_cast<ScheduleComputation*>(arg)->run();
    }
#else
    std::mutex sched_mutex;
    std::condition_variable sched_cv;
    std::thread* scthread = NULL;
private:
#endif
    // Class containing the current Stream Requests (SME)
    MasterStreamManagementContext stream_mgmt;
    MasterStreamManagementContext stream_snapshot;
    // Class containing a snapshot of the network topology
    TopologyMap topology_map;
};

class Router {
public:
    Router(ScheduleComputation& scheduler, bool multipath, int more_hops) : 
        scheduler(scheduler) {};
    virtual ~Router() {};

    void run();

private:
    std::list<StreamManagementElement> breadthFirstSearch(StreamManagementElement stream);
    std::list<StreamManagementElement> construct_path(unsigned char node, std::map<const unsigned char, unsigned char> parent_of);
    
protected:
    int multipath;
    // Expanded stream request after routing
    std::list<StreamManagementElement> routed_streams;
    // References to other classes
    ScheduleComputation& scheduler;
};
}
