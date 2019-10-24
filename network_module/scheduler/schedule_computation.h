/***************************************************************************
 *   Copyright (C) 2018-2019 by Federico Amedeo Izzo                       *
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

#include "../stream/stream_collection.h"
#include "../uplink_phase/topology/network_topology.h"
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
public:
    ScheduleComputation(const NetworkConfiguration& cfg, unsigned slotsPerTile,
                        unsigned dataslotsPerDownlinkTile, unsigned dataslotsPerUplinkTile);

    void startThread();
    
    void beginScheduling();
    
    /**
     * Used by the ScheduleDownlink class to get the latest schedule
     * @return a copy of the Schedule class containing schedule, size, id
     */
    void getSchedule(std::vector<ScheduleElement>& sched, unsigned long& id, unsigned int& tiles);
    
    /**
     * Used by the ScheduleDownlink class to know if a schedule needs to be sent
     * and applied.
     */
    bool needToSendSchedule() {
        // Mutex lock to access schedule (shared with ScheduleDownlink).
#ifdef _MIOSIX
        miosix::Lock<miosix::Mutex> lck(sched_mutex);
#else
        std::unique_lock<std::mutex> lck(sched_mutex);
#endif
        return scheduleNotApplied;
    }
    /**
     * Used by the ScheduleDownlink class to notify the scheduler that the previous
     * schedule has been applied and a new one can be computed.
     */
    void scheduleSentAndApplied(){      
        // Mutex lock to access schedule (shared with ScheduleDownlink).
#ifdef _MIOSIX
        miosix::Lock<miosix::Mutex> lck(sched_mutex);
#else
        std::unique_lock<std::mutex> lck(sched_mutex);
#endif
        scheduleNotApplied = false;
    }

    StreamCollection* getStreamCollection() {
        return &stream_collection;
    }

    void setTopology(NetworkTopology* topology) {
        this->topology = topology;
    }
    
#ifdef UNITTEST
    /**
     * Wait until the scheduler is ready to accept a beginScheduling()
     */
    void sync()
    {
        std::unique_lock<std::mutex> lck(sched_mutex);
        while(ready==false) sched_cv.wait(lck);
    }
#endif

private: 
    void run();
    
    bool reschedule();
    
    std::set<std::pair<unsigned char,unsigned char>> computeUsedLinks() const;

    void initialPrint(bool removed, bool wrote_back, bool graph_changed);

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
    const bool channelSpatialReuse;

    /* Class containing a map of all the Streams and Servers in the network
     * and a queue of InfoElements to send on the network */
    StreamCollection stream_collection;
    /* Snapshot of the StreamCollection used for schedule computation */
    StreamSnapshot stream_snapshot;
    // Class containing latest schedule, size in tiles and schedule ID
    Schedule schedule;
    // Used to disable scheduling until the previous schedule
    // has been distributed and applied
    bool scheduleNotApplied = false;

    /* References to other classes */
    const unsigned slotsPerTile;
    const unsigned dataslots_downlinktile;
    const unsigned dataslots_uplinktile;
    // Needed to get topology information
    NetworkTopology* topology = nullptr;
    // Used to get controlsuperframestructure
    const NetworkConfiguration& netconfig;
    // Get network tile/superframe information
    const ControlSuperframeStructure superframe;
    // Class containing a snapshot of the network topology
    GRAPH_TYPE network_graph;
    
#ifdef UNITTEST
    bool ready=false;
#endif

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

    unsigned char maxHops = 0;
    // TODO: make more_hop configurable in network_configuration
    unsigned char more_hops = 0;
};
}
