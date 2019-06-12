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


#include "../stream/stream_manager.h"
#include "../stream/stream_collection.h"
#include "../uplink_phase/topology/network_graph.h"
#include "../uplink_phase/uplink_message.h"
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

namespace mxnet {

class MACContext;
class MasterUplinkPhase;

// Used by the ScheduleDownlink to get all data related to schedule at once
class Schedule {
public:
    Schedule() {}
    Schedule(unsigned long id, unsigned int tiles) : id(id), tiles(tiles) {}
    Schedule(std::list<ScheduleElement> schedule, unsigned long id,
              unsigned int tiles) : schedule(schedule), id(id), tiles(tiles) {}
    void swap(Schedule& rhs) {
        schedule.swap(rhs.schedule);
        std::swap(id, rhs.id);
        std::swap(tiles, rhs.tiles);
    }

    std::list<ScheduleElement> schedule;
    // NOTE: schedule with id=0 are not sent in MasterScheduleDistribution
    unsigned long id;
    // schedule_size must always be initialized to the number of tiles in superframe
    // it can be obtained with superframe.size()
    unsigned int tiles;
};

class ScheduleComputation {
    friend class Router;
    friend class MasterScheduleDownlinkPhase;
public:
    ScheduleComputation(MACContext& mac_ctx);

    void startThread();
    
    void beginScheduling();

    /**
     * Receives a vector of SME from the network and register them in the StreamCollection
     */
    void receiveSMEs(UpdatableQueue<StreamId, StreamManagementElement>& smes);

    StreamManager* getStreamManager() {
        return &stream_mgr;
    }

    StreamCollection* getStreamCollection() {
        return &stream_collection;
    }

    void setUplinkPhase(MasterUplinkPhase* uplink) {
        uplink_phase = uplink;
    }

    void setNetworkGraph(const GRAPH_TYPE& graph) {
        network_graph = graph;
        graph_changed = true;
    }

private: 
    void run();

    void initialPrint(bool removed, bool wrote_back);

    /**
     * @return a schedule class containing schedule, tile number and ID
     * Reschedule and route already ESTABLISHED streams
     */
    Schedule scheduleEstablishedStreams(unsigned long id);
    /**
     * Updates a Schedule class
     * Schedule and route ACCEPTED streams
     */
    void scheduleAcceptedStreams(Schedule& currSchedule);
    /**
     * Updates stream_snapshot with results of scheduling,
     * notifies REJECTED streams
     */
    void updateStreams(const std::list<ScheduleElement>& final_schedule);

    void finalPrint();
    /**
     * @return a pair of schedule and schedule_size
     * Runs the Router and the Scheduler to produce a new schedule
     */
    std::pair<std::list<ScheduleElement>,
              unsigned int> routeAndScheduleStreams(std::vector<MasterStreamInfo>& stream_list,
                                                    const std::list<ScheduleElement>& current_schedule,
                                                    const unsigned int sched_size);
    /**
     * @return a pair of schedule and schedule_size.
     * Runs the Scheduler to schedule routed streams
     */
    std::pair<std::list<ScheduleElement>,
              unsigned int> scheduleStreams(const std::list<std::list<ScheduleElement>>& routed_streams,
                                            const std::list<ScheduleElement>& current_schedule,
                                            const unsigned int sched_size);

    bool checkAllConflicts(std::list<ScheduleElement> other_streams, const ScheduleElement& transmission, unsigned offset, unsigned tile_size);

    bool checkDataSlot(unsigned offset, unsigned tile_size, unsigned downlink_size, unsigned uplink_size);

    bool slotConflictPossible(const ScheduleElement& newtransm, const ScheduleElement& oldtransm, unsigned offset, unsigned tile_size);

    bool checkSlotConflict(const ScheduleElement& newtransm, const ScheduleElement& oldtransm, unsigned offset_a, unsigned tile_size);

    bool checkUnicityConflict(const ScheduleElement& new_transmission, const ScheduleElement& old_transmission);

    bool checkInterferenceConflict(const ScheduleElement& new_transmission, const ScheduleElement& old_transmission);

    void printSchedule(const Schedule& sched);
    /**
     * Used by the ScheduleDownlink class to get the latest schedule
     * @return a Schedule class containing schedule, size, id
     */
    Schedule getSchedule() {
        // Mutex lock to access schedule (shared with ScheduleDownlink).
#ifdef _MIOSIX
        miosix::Lock<miosix::Mutex> lck(sched_mutex);
#else
        std::unique_lock<std::mutex> lck(sched_mutex);
#endif
        return schedule;
    }
    /**
     * Used by the ScheduleDownlink class to know if a newer schedule is available
     * @return the latest scheduleID
     */
    unsigned long getScheduleID() {
        // Mutex lock to access schedule (shared with ScheduleDownlink).
#ifdef _MIOSIX
        miosix::Lock<miosix::Mutex> lck(sched_mutex);
#else
        std::unique_lock<std::mutex> lck(sched_mutex);
#endif
        return schedule.id;
    }

    void printStreams(const std::vector<MasterStreamInfo>& stream_list);

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

    /* Cached configuration parameters from NetworkConfiguration */
    bool channelSpatialReuse = false;

    /* Class containing Streams and Servers related to this node */
    StreamManager stream_mgr;
    /* Class containing a map of all the Streams and Servers in the network
     * and a queue of InfoElements to send on the network */
    StreamCollection stream_collection;
    /* Snapshot of the StreamCollection used for schedule computation */
    StreamCollection stream_snapshot;
    // Class containing latest schedule, size in tiles and schedule ID
    Schedule schedule;

    /* References to other classes */
    // Needed to get number of dataslots
    MACContext& mac_ctx;
    // Needed to get topology information
    MasterUplinkPhase* uplink_phase = nullptr;
    // Used to get controlsuperframestructure
    const NetworkConfiguration& netconfig;
    // Get network tile/superframe information
    const ControlSuperframeStructure superframe;
    // Class containing a snapshot of the network topology
    GRAPH_TYPE network_graph;
    // Flag set to true if the network graph changed from the last schedule
    bool graph_changed = false;

#ifdef _MIOSIX
    miosix::Mutex sched_mutex;
    miosix::ConditionVariable sched_cv;
    miosix::Thread* scthread = nullptr;
    static void threadLauncher(void *arg) {
        reinterpret_cast<ScheduleComputation*>(arg)->run();
    }
#else
    std::mutex sched_mutex;
    std::condition_variable sched_cv;
    std::thread* scthread = nullptr;
#endif
};

class Router {
public:
    Router(ScheduleComputation& scheduler, int maxHops, int more_hops) : 
        scheduler(scheduler), maxHops(maxHops), more_hops(more_hops) {};
    virtual ~Router() {};

    std::list<std::list<ScheduleElement>> run(std::vector<MasterStreamInfo>& stream_list);

private:
    std::list<unsigned char> breadthFirstSearch(MasterStreamInfo stream);
    std::list<unsigned char> construct_path(unsigned char node, std::map<const unsigned char, unsigned char>& parent_of);
    /* Transform path ( 0 1 2 3 ) to schedule (0->1 1->2 2->3) */
    std::list<ScheduleElement> pathToSchedule(const std::list<unsigned char>& path,
                                                      const MasterStreamInfo& stream);
    void printPath(const std::list<unsigned char>& path);
    void printPathList(const std::list<std::list<unsigned char>>& path_list);
    std::list<std::list<unsigned char>> depthFirstSearch(MasterStreamInfo stream, unsigned int limit);
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

    int maxHops = 0;
    // TODO: make more_hop configurable in network_configuration
    int more_hops;
};
}
