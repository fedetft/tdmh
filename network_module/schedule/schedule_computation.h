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
#include "../network_configuration.h"
#include "schedule_element.h"
#ifdef _MIOSIX
#include <miosix.h>
#else
#include <thread>
#include <mutex>
#include <condition_variable>
#endif
#include <list>
#include <vector>
#include <tuple>
#include <iterator>

namespace mxnet {

class MasterTopologyContext;
class StreamManagementElement;
class MACContext;

class ScheduleComputation {
    friend class Router;
    friend class MasterScheduleDownlinkPhase;
public:
    ScheduleComputation(MACContext& mac_ctx, MasterMeshTopologyContext& topology_ctx);

    void startThread();
    
    void beginScheduling();
    
    void addNewStreams(std::vector<StreamManagementElement>& smes);

    void open(StreamManagementElement sme);

private: 
    void run();

    std::vector<ScheduleElement> routeAndScheduleStreams(std::vector<StreamManagementElement> stream_list, NetworkConfiguration netconfig);
    
    std::vector<ScheduleElement> scheduleStreams(std::list<std::list<ScheduleElement>> routed_streams, NetworkConfiguration netconfig);

    bool checkDataSlot(unsigned offset, unsigned tile_size, ControlSuperframeStructure superframe, unsigned downlink_size, unsigned uplink_size);

    bool slotConflictPossible(ScheduleElement newtransm, ScheduleElement oldtransm, unsigned offset, unsigned tile_size);

    bool checkSlotConflict(ScheduleElement newtransm, ScheduleElement oldtransm, unsigned offset_a, unsigned tile_size, unsigned schedule_size);

    bool checkUnicityConflict(ScheduleElement new_transmission, ScheduleElement old_transmission);

    bool checkInterferenceConflict(ScheduleElement new_transmission, ScheduleElement old_transmission);

    void printSchedule();

    std::vector<ScheduleElement> getSchedule() {
        return schedule;
    }

    void printStreams(std::vector<StreamManagementElement> stream_list);

    void printStreamList(std::list<std::list<ScheduleElement>> stream_list);        

    int gcd(int a, int b) {
        for (;;) {
            if (a == 0) return b;
            b %= a;
            if (b == 0) return a;
            a %= b;
        }
    };

    int lcm(int a, int b) {
        int temp = gcd(a, b);
        return temp ? (a / temp * b) : 0;
    };

    // Class containing the current Stream Requests (SME)
    MasterStreamManagementContext stream_mgmt;
    MasterStreamManagementContext stream_snapshot;
    // Class containing a snapshot of the network topology
    TopologyMap topology_map;
    // Final stream list after scheduling
    std::vector<ScheduleElement> schedule;

    // References to other classes
    MasterMeshTopologyContext& topology_ctx;
    // Needed to get number of dataslots
    MACContext& mac_ctx;

#ifdef _MIOSIX
    miosix::Mutex sched_mutex;
    miosix::ConditionVariable sched_cv;
    miosix::Thread* scthread = NULL;
    static void threadLauncher(void *arg) {
        reinterpret_cast<ScheduleComputation*>(arg)->run();
    }
#else
    std::mutex sched_mutex;
    std::condition_variable sched_cv;
    std::thread* scthread = NULL;
#endif
};

class Router {
public:
    Router(ScheduleComputation& scheduler, bool multipath, int more_hops) : 
        scheduler(scheduler), multipath(multipath), more_hops(more_hops) {};
    virtual ~Router() {};

    std::list<std::list<ScheduleElement>> run(std::vector<StreamManagementElement> stream_list);

private:
    std::list<ScheduleElement> breadthFirstSearch(StreamManagementElement stream);
    std::list<ScheduleElement> construct_path(StreamManagementElement stream, unsigned char node, std::map<const unsigned char, unsigned char> parent_of);
    
protected:
    bool multipath;
    int more_hops;
    // References to other classes
    ScheduleComputation& scheduler;
};
}
