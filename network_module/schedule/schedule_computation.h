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


#include "../stream_manager.h"
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
class MACContext;

class ScheduleComputation {
    friend class Router;
    friend class MasterScheduleDownlinkPhase;
public:
    ScheduleComputation(MACContext& mac_ctx, MasterMeshTopologyContext& topology_ctx);

    void startThread();
    
    void beginScheduling();

    /**
     * Receives a vector of SME from the network and register them in the StreamManager
     */
    void receiveSMEs(const std::vector<StreamManagementElement>& smes);

    void open(const StreamInfo& sme);

private: 
    void run();

    std::vector<ScheduleElement> routeAndScheduleStreams(const std::vector<StreamInfo>& stream_list);
    
    std::vector<ScheduleElement> scheduleStreams(const std::list<std::list<ScheduleElement>>& routed_streams);

    bool checkAllConflicts(std::vector<ScheduleElement> other_streams, const ScheduleElement& transmission, unsigned offset, unsigned tile_size);

    bool checkDataSlot(unsigned offset, unsigned tile_size, unsigned downlink_size, unsigned uplink_size);

    bool slotConflictPossible(const ScheduleElement& newtransm, const ScheduleElement& oldtransm, unsigned offset, unsigned tile_size);

    bool checkSlotConflict(const ScheduleElement& newtransm, const ScheduleElement& oldtransm, unsigned offset_a, unsigned tile_size);

    bool checkUnicityConflict(const ScheduleElement& new_transmission, const ScheduleElement& old_transmission);

    bool checkInterferenceConflict(const ScheduleElement& new_transmission, const ScheduleElement& old_transmission);

    void printSchedule();

    std::vector<ScheduleElement> getSchedule() {
        return schedule;
    }

    unsigned long getScheduleID() {
        return scheduleID;
    }

    unsigned int getScheduleTiles() {
        return schedule_size;
    }

    void printStreams(const std::vector<StreamInfo>& stream_list);

    void printStreamList(const std::list<std::list<ScheduleElement>>& stream_list);        

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

    // Class containing information about Streams
    StreamManager stream_mgmt;
    StreamCollection stream_snapshot;
    // Final stream list after scheduling
    std::vector<ScheduleElement> schedule;
    // Used to check if a (new) schedule is available
    unsigned long scheduleID = 0;
    // Schedulesize value is equal to lcm(p1,p2,...,pn) or p1 for a single stream
    unsigned int schedule_size;
    // Schedule size of last stream, used if the scheduling of a stream has failed
    // and we need to rollback the size to the old value
    unsigned int last_schedule_size;

    // References to other classes
    MasterMeshTopologyContext& topology_ctx;
    // Needed to get number of dataslots
    MACContext& mac_ctx;
    // Used to get controlsuperframestructure
    const NetworkConfiguration& netconfig;
    // Get network tile/superframe information
    const ControlSuperframeStructure superframe;
    // Class containing a snapshot of the network topology
    TopologyMap topology_map;


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
    Router(ScheduleComputation& scheduler, int more_hops) : 
        scheduler(scheduler), more_hops(more_hops) {};
    virtual ~Router() {};

    std::list<std::list<ScheduleElement>> run(const std::vector<StreamInfo>& stream_list);

private:
    std::list<unsigned char> breadthFirstSearch(StreamInfo stream);
    std::list<unsigned char> construct_path(unsigned char node, std::map<const unsigned char, unsigned char>& parent_of);
    /* Transform path ( 0 1 2 3 ) to schedule (0->1 1->2 2->3) */
    std::list<ScheduleElement> pathToSchedule(const std::list<unsigned char>& path,
                                                      const StreamInfo& stream);
    void printPath(const std::list<unsigned char>& path);
    void printPathList(const std::list<std::list<unsigned char>>& path_list);
    std::list<std::list<unsigned char>> depthFirstSearch(StreamInfo stream, unsigned int limit);
    // Recursive function
    void dfsRun(unsigned char start, unsigned char target, unsigned int limit,
                std::vector<bool>& visited, std::list<unsigned char>& path,
                std::list<std::list<unsigned char>>& all_paths);
    std::list<unsigned char> findShortestPath(const std::list<std::list<unsigned char>>& path_list);
    std::list<std::list<unsigned char>> findIndependentPaths(const std::list<std::list<unsigned char>>& path_list,
                                                             const std::list<unsigned char> primary);
protected:
    // References to other classes
    ScheduleComputation& scheduler;

    // TODO: make more_hop configurable in network_configuration
    int more_hops;
};
}
