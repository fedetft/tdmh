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

#include "schedule_computation.h"
#include "../util/debug_settings.h"
#include "../uplink_phase/master_uplink_phase.h"
#include "../util/stackrange.h"
#include <unordered_set>
#include <utility>
#include <stdio.h>

/**
 * The methods of this class run on a separate thread, don't use print_dbg()
 * to avoid race conditions, use printf() or cout instead.
 */

namespace mxnet {

ScheduleComputation::ScheduleComputation(const NetworkConfiguration& cfg,
    unsigned slotsPerTile, unsigned dataslotsPerDownlinkTile, unsigned dataslotsPerUplinkTile) :
    channelSpatialReuse(cfg.getChannelSpatialReuse()),
    schedule(0, cfg.getControlSuperframeStructure().size()), // Initialize Schedule with ID=0 and tile_size = superframe size
    slotsPerTile(slotsPerTile),
    dataslots_downlinktile(dataslotsPerDownlinkTile),
    dataslots_uplinktile(dataslotsPerUplinkTile),
    netconfig(cfg),
    superframe(netconfig.getControlSuperframeStructure()),
    network_graph(netconfig.getNeighborBitmaskSize())
{

}

void ScheduleComputation::startThread() {
    if (scthread == nullptr)
#ifdef _MIOSIX
        // Thread launched using static function threadLauncher with the class instance as parameter.
        scthread = miosix::Thread::create(&ScheduleComputation::threadLauncher, 3*1024, miosix::PRIORITY_MAX-2, this, miosix::Thread::JOINABLE);
#else
        scthread = new std::thread(&ScheduleComputation::run, this);
#endif
}

void ScheduleComputation::beginScheduling() {
#ifdef _MIOSIX
    sched_cv.signal();
#else
    sched_cv.notify_one();
#endif
}

void ScheduleComputation::run() {
    printStackRange("scheduler");
    for(;;) {
        bool forceResend=false;
        for(;;)
        {
#ifdef _MIOSIX
            miosix::Lock<miosix::Mutex> lck(sched_mutex);
#else
            std::unique_lock<std::mutex> lck(sched_mutex);
#endif
            /* NOTE: A new schedule has to be computed if the topology or
                streams have changed, but once a new schedule has been computed,
                the scheduling has to wait until the previous schedule has been
                applied. Otherwise, the getCurrentSchedule fails to compute the
                next activation time and the new schedule may not be aligned to
                the previous one */
            if(scheduleNotApplied==false) {
                // Can compute a new schedule if required
                if(uplink_phase->wasModified()) break;
                auto op = stream_collection.getOperation();
                if(op.resend && !op.reschedule)
                {
                    //If we get here we are asked to ONLY resend and not
                    //to also reschedule
                    // Mark the presence of a new schedule, not still applied
                    scheduleNotApplied = true;
                } else {
                    if(op.resend) forceResend = true;
                    if(op.reschedule) break;
                }
            }
            sched_cv.wait(lck); // Wait for beginScheduling() to start scheduling
        }
#ifdef _MIOSIX
        unsigned int stackSize = miosix::MemoryProfiling::getStackSize();
        unsigned int absFreeStack = miosix::MemoryProfiling::getAbsoluteFreeStack();
        printf("[H] Scheduler stack %d/%d\n",stackSize-absFreeStack,stackSize);
#endif
        // Take snapshot of stream requests
        stream_snapshot = stream_collection.getSnapshot();
        
        // Get new graph snapshot if graph changed
        bool graph_changed = uplink_phase->updateSchedulerNetworkGraph(network_graph);
        
        /* IMPORTANT!: From now on use only the snapshot classes
           `stream_snapshot` and `network_graph` */
        /* NOTE: check local graph copy for nodes unreachable from the master
           and remove them */
        bool removed = false;
        bool wrote_back = false;
        if(network_graph.hasUnreachableNodes()) {
            removed = network_graph.removeUnreachableNodes();
            if(removed)
                wrote_back = uplink_phase->writeBackNetworkGraph(network_graph);
        }
        initialPrint(removed, wrote_back, graph_changed);
        // Used to check if the schedule has been changed in this iteration
        bool scheduleChanged = false;

        /* NOTE: Here we prioritize established streams over new ones */
        /* If topology changed or a stream was removed:
           clear current schedule and reschedule established streams */
        Schedule newSchedule;
        if(graph_changed || stream_snapshot.wasRemoved()) {
            newSchedule = scheduleEstablishedStreams(schedule.id + 1);
            scheduleChanged = true;
        }
        // Otherwise continue scheduling from the last schedule
        // NOTE: the schedule is read without mutex because the schedule class
        // is written in a mutex protected block and read by other threads in a
        // mutex protected block.
        else{
            newSchedule = Schedule(schedule.schedule, schedule.id + 1, schedule.tiles);
        }
        /* If there are new accepted streams:
           route + schedule them and add them to existing schedule */
        if(stream_snapshot.wasAdded()) {
            scheduleAcceptedStreams(newSchedule);
            scheduleChanged = true;
        }
        if(scheduleChanged) {
            // Mutex lock to access schedule (shared with ScheduleDownlink).
#ifdef _MIOSIX
            miosix::Lock<miosix::Mutex> lck(sched_mutex);
#else
            std::unique_lock<std::mutex> lck(sched_mutex);
#endif
            // Update stream_collection for printing results and notify REJECTED streams
            /* NOTE: Here we need to change the stream status in stream_collection.
            (note: we compute the changes using the snapshot and then update the StreamCollection)
            Do NOT ever change here the status of the streams in StreamManager,
            because doing so would mean applying the schedule before its activation time.
            The status in StreamManager must be changed ONLY in the ScheduleDistribution */
            auto changes = stream_snapshot.getStreamChanges(newSchedule.schedule);
            stream_collection.applyChanges(changes);
            // Overwrite current schedule with new one
            schedule.swap(newSchedule);
            // Mark the presence of a new schedule, not still applied
            scheduleNotApplied = true;
        } else if(forceResend) {
#ifdef _MIOSIX
            miosix::Lock<miosix::Mutex> lck(sched_mutex);
#else
            std::unique_lock<std::mutex> lck(sched_mutex);
#endif
            // Mark the presence of a new schedule, not still applied
            scheduleNotApplied = true;
        }
        finalPrint();
    }
}

void ScheduleComputation::initialPrint(bool removed, bool wrote_back, bool graph_changed) {
    if(SCHEDULER_DETAILED_DBG || SCHEDULER_SUMMARY_DBG) {
        printf("\n[SC] #### Starting schedule computation ####\n");
        printf("topology changed:%s, stream changed:%s\n",
               graph_changed?"True":"False",
               stream_snapshot.wasModified()?"True":"False");
        // NOTE: Debug topology print
        printf("[SC] Begin Topology\n");
        for(auto it : network_graph.getEdges())
            printf("[%d - %d]\n", it.first, it.second);
        printf("[SC] End Topology\n");
        if(removed) {
            printf("[SC] Removed nodes not reachable by master from the network graph\n");
            if(wrote_back)
                printf("[SC] Network graph without non-reachable nodes wrote-back to Uplink Phase\n");
            else
                printf("[SC] Cannot write back network graph with non-reachable nodes\n");
        }
        printf("[SC] Stream list before scheduling:\n");
        printStreams(stream_snapshot.getStreams());
    }
    if(SCHEDULER_DETAILED_DBG)
        printf("[SC] Total streams: %d\n", stream_snapshot.getStreamNumber());
}

Schedule ScheduleComputation::scheduleEstablishedStreams(unsigned long id) {
    if(SCHEDULER_DETAILED_DBG)
        printf("[SC] Topology changed or a stream was removed, re-scheduling established streams\n");
    // Get already ESTABLISHED streams, to reschedule
    auto established_streams = stream_snapshot.getStreamsWithStatus(MasterStreamStatus::ESTABLISHED);
    if(SCHEDULER_DETAILED_DBG)
        printf("[SC] Established streams: %u\n", established_streams.size());
    // We are starting from an empty schedule, no need to check for conflicts
    std::list<ScheduleElement> empty;
    // Schedule size must always be initialized to the number of tiles in superframe
    auto newSize = superframe.size();
    // Reschedule ESTABLISHED streams and return pair of schedule and schedule size
    auto schedulePair = routeAndScheduleStreams(established_streams, empty, newSize);
    return Schedule(schedulePair.first, id, schedulePair.second);
}

void ScheduleComputation::scheduleAcceptedStreams(Schedule& currSchedule) {
    if(SCHEDULER_DETAILED_DBG)
        printf("[SC] Scheduling accepted streams\n");
    // Get ACCEPTED streams, to schedule
    auto accepted_streams = stream_snapshot.getStreamsWithStatus(MasterStreamStatus::ACCEPTED);
    //Sort accepted streams based on highest period first
    std::sort(accepted_streams.begin(), accepted_streams.end(),
              [](MasterStreamInfo a, MasterStreamInfo b) {
                  return toInt(a.getPeriod()) > toInt(b.getPeriod());});
    if(SCHEDULER_DETAILED_DBG)
        printf("[SC] Accepted streams: %u\n", accepted_streams.size());
    auto extraSchedulePair = routeAndScheduleStreams(accepted_streams,
                                                     currSchedule.schedule,
                                                     currSchedule.tiles);
    // Insert computed schedule elements
    currSchedule.schedule.insert(currSchedule.schedule.end(), extraSchedulePair.first.begin(),
                              extraSchedulePair.first.end());
    // Update schedule size
    currSchedule.tiles = extraSchedulePair.second;
}

void ScheduleComputation::finalPrint() {
    /* Get updated stream status from StreamCollection */
    std::vector<MasterStreamInfo> streams = stream_collection.getStreams();
    /* Print the schedule and updated stream status */
    if(SCHEDULER_SUMMARY_DBG) {
        printf("[SC] ## Results ##\n");
        printf("[SC] Final schedule, ID:%lu\n", schedule.id);
        printSchedule(schedule);
        printf("[SC] Stream list after scheduling:\n");
        printStreams(streams);
    }
    // To avoid caching of stdout
    fflush(stdout);
}

std::pair<std::list<ScheduleElement>,
          unsigned int> ScheduleComputation::routeAndScheduleStreams(std::vector<MasterStreamInfo>& stream_list,
                                                                     const std::list<ScheduleElement>& current_schedule,
                                                                     const unsigned int schedSize) {
    if(stream_list.empty()) {
        if(SCHEDULER_DETAILED_DBG)
            printf("[SC] Stream list empty, scheduling done.\n");
        std::list<ScheduleElement> empty;
        return make_pair(empty, schedSize);
    }
    Router router(*this, netconfig.getMaxHops(), 1);
    if(SCHEDULER_DETAILED_DBG)
        printf("[SC] ## Routing ##\n");
    // Run router to route multi-hop streams and get multiple paths
    auto routed_streams = router.run(stream_list);
    if(SCHEDULER_DETAILED_DBG) {
        printf("[SC] Stream list after routing:\n");
        printStreamList(routed_streams);
    }
    // Schedule expanded streams, avoiding conflicts
    if(SCHEDULER_DETAILED_DBG)
        printf("[SC] ## Scheduling ##\n");
    return scheduleStreams(routed_streams, current_schedule, schedSize);
}

void ScheduleComputation::getSchedule(std::vector<ScheduleElement>& sched,
                                      unsigned long& id, unsigned int& tiles) {
    // Mutex lock to access schedule (shared with ScheduleDownlink).
#ifdef _MIOSIX
    miosix::Lock<miosix::Mutex> lck(sched_mutex);
#else
    std::unique_lock<std::mutex> lck(sched_mutex);
#endif
    sched.clear();
    sched.reserve(schedule.schedule.size());
    std::copy(schedule.schedule.begin(),schedule.schedule.end(),std::back_inserter(sched));
    id = schedule.id;
    tiles = schedule.tiles;
}

std::pair<std::list<ScheduleElement>,
          unsigned int> ScheduleComputation::scheduleStreams(const std::list<std::list<ScheduleElement>>& routed_streams,
                                                             const std::list<ScheduleElement>& current_schedule,
                                                             const unsigned int schedSize) {
    unsigned tile_size = slotsPerTile;
    unsigned downlink_size = tile_size - dataslots_downlinktile;
    unsigned uplink_size = tile_size - dataslots_uplinktile;
    if(SCHEDULER_DETAILED_DBG)
        printf("[SC] Network configuration:\n- tile_size: %d\n- downlink_size: %d\n- uplink_size: %d\n",
           tile_size, downlink_size, uplink_size);
    // Start with an empty schedule, this schedule will be returned
    std::list<ScheduleElement> scheduled_transmissions;
    // Schedule size of last stream, used if the scheduling of a stream has failed
    // and we need to rollback the size to the old value
    auto lastSize = schedSize;
    auto newSize = schedSize;
    for(auto& stream : routed_streams) {
        int block_size = 0;
        bool stream_err = false;
        // Counter to last slot offset: ensures sequentiality
        unsigned last_offset = 0;
        // Save schedule size to rollback in case the stream scheduling fails
        lastSize = newSize;
        for(auto& transmission : stream) {
            unsigned char tx = transmission.getTx();
            unsigned char rx = transmission.getRx();
            if(SCHEDULER_DETAILED_DBG)
                printf("[SC] Scheduling transmission %d,%d\n", tx, rx);
            // Connectivity check
            if(!network_graph.hasEdge(tx, rx)) {
                stream_err = true;
                if(SCHEDULER_DETAILED_DBG)
                    printf("[SC] %d,%d are not connected in topology, cannot schedule stream\n", tx, rx);
            }
            // If a transmission cannot be scheduled, undo the whole stream
            if(stream_err) {
                if(SCHEDULER_DETAILED_DBG)
                    printf("[SC] Transmission scheduling failed, removing rest of the stream\n");
                for(int i=0; i<block_size; i++) {
                    scheduled_transmissions.pop_back();
                }
                // Rollback schedule size to before the failed stream
                newSize = lastSize;
                // Skip to next block
                break;
                }
            // The offset must be smaller than (stream period * minimum period size)-1
            // with minimum period size being equal to the tile lenght (by design)
            // Otherwise the resulting stream won't be periodic
            unsigned max_offset = (toInt(transmission.getPeriod()) * tile_size) - 1;
            for(unsigned offset = last_offset; offset < max_offset; offset++) {
                if(!checkDataSlot(offset, tile_size, downlink_size, uplink_size))
                    continue;
                if(SCHEDULER_DETAILED_DBG)
                    printf("[SC] Checking offset %d\n", offset);
                // Cycle over already scheduled elements to find conflicts
                if(SCHEDULER_DETAILED_DBG)
                    printf("[SC] Checking against old streams\n");
                bool conflict = checkAllConflicts(current_schedule, transmission, offset, tile_size);
                // Cycle over elements to be scheduled to find conflicts
                if(SCHEDULER_DETAILED_DBG)
                    printf("[SC] Checking against new streams\n");
                conflict |= checkAllConflicts(scheduled_transmissions, transmission, offset, tile_size);

                if(!conflict) {
                    last_offset = offset;
                    block_size++;
                    // Calculate new schedule size
                    unsigned period = toInt(transmission.getPeriod());
                    if(SCHEDULER_DETAILED_DBG)
                        printf("[SC] Schedule size, before:%d ", newSize);
                    newSize = lcm(newSize, period);
                    if(SCHEDULER_DETAILED_DBG)
                        printf("after:%d \n", newSize);
                    // Add transmission to schedule, and set schedule offset
                    scheduled_transmissions.push_back(transmission);
                    scheduled_transmissions.back().setOffset(offset);
                    if(SCHEDULER_DETAILED_DBG)
                        printf("[SC] Scheduled transmission %d,%d with offset %d\n", tx, rx, offset);
                    // Successfully scheduled transmission, break timeslot cycle
                    break;
                    }
                else {
                    if(SCHEDULER_DETAILED_DBG)
                        printf("[SC] Cannot schedule transmission %d,%d with offset %d\n", tx, rx, offset);
                    //Try to schedule in next timeslot
                }
            }
            // Next transmission of stream should start from next timeslot
            // to guarantee sequentiality in transmissions of the same stream
            last_offset++;
            // If we are in the last timeslot and have a conflict,
            // cannot reschedule on another timeslot
            if(last_offset == max_offset) {
                stream_err = true;
                // Print error if max_offset is used on a transmission != last transmission
                if(transmission.getDst() != transmission.getRx())
                if(SCHEDULER_SUMMARY_DBG || SCHEDULER_DETAILED_DBG)
                    printf("[SC] ERROR: Cannot schedule stream %d,%d: no more free data slots\n", transmission.getSrc(), transmission.getDst());
            }
        }
    }
    if(SCHEDULER_DETAILED_DBG)
        printf("[SC] Final schedule length: %d\n", newSize);
    return make_pair(scheduled_transmissions, newSize);
}

bool ScheduleComputation::checkAllConflicts(std::list<ScheduleElement> other_streams, const ScheduleElement& transmission, unsigned offset, unsigned tile_size) {
    bool conflict = false;
    for(auto& elem : other_streams) {
        // conflictPossible is a simple condition used to reduce number of conflict checks
        if(slotConflictPossible(transmission, elem, offset, tile_size)) {
            if(SCHEDULER_DETAILED_DBG)
                printf("[SC] Conflict possible with %d->%d\n", elem.getTx(), elem.getRx());
            if(checkSlotConflict(transmission, elem, offset, tile_size)) {
                if(SCHEDULER_DETAILED_DBG)
                    printf("[SC] %d->%d and %d-%d have timeslots in common\n", transmission.getTx(),
                           transmission.getRx(), elem.getTx(), elem.getRx());
                // NOTE: if spatial reuse of channels is disabled, we consider
                // two distinct transmissions on the same dataslot as a conflict
                if(channelSpatialReuse == false) {
                    conflict |= true;
                }
                // otherwise we try to infer if the two transmissions will conflict or not
                else {
                    /* Conflict checks */
                    // Unicity check: no activity for src or dst node in a given timeslot
                    if(checkUnicityConflict(transmission, elem)) {
                        conflict |= true;
                        if(SCHEDULER_DETAILED_DBG)
                            printf("[SC] Unicity conflict!\n");
                    }
                    // Interference check: no TX and RX for nodes at 1-hop distance in the same timeslot
                    if(checkInterferenceConflict(transmission, elem)) {
                        conflict |= true;
                        if(SCHEDULER_DETAILED_DBG)
                            printf("[SC] Interference conflict!\n");
                    }
                }
                if(conflict)
                    // Avoid checking other streams when a conflict is found
                    break;
            }
        }
        // else conflict is not possible
    }
    return conflict;            
}

// This check makes sure that data is not scheduled in control slots (Downlink, Uplink)
// Return true if the slot is a data slot, false otherwise
bool ScheduleComputation::checkDataSlot(unsigned offset, unsigned tile_size,
                                        unsigned downlink_size,
                                        unsigned uplink_size) {
    /* NOTE: superframe.isControlDownlink/Uplink() accepts a number from 0 to (tileSize-1)
       so we need to convert the tile number to the position in the superframe
       we can do so by using the remainder operation */

    // Calculate current tile number
    unsigned tile = offset / tile_size;
    unsigned tilePos = tile % superframe.size();
    // Calculate position in current tile
    unsigned slot = offset % tile_size;
    if((superframe.isControlDownlink(tilePos) && (slot < downlink_size)) ||
       (superframe.isControlUplink(tilePos) && (slot < uplink_size)))
        return false;
    return true;
}

// This easy check is a necessary condition for a slot conflict,
// if the result is false, then a conflict cannot happen
// It can be used to avoid nested loops
bool ScheduleComputation::slotConflictPossible(const ScheduleElement& newtransm,
                                               const ScheduleElement& oldtransm,
                                               unsigned offset, unsigned tile_size) {
    // Compare offsets relative to current tile of two transmissions
    return ((offset % tile_size) == (oldtransm.getOffset() % tile_size));
}

// Extensive check to be used when slotConflictPossible returns true
bool ScheduleComputation::checkSlotConflict(const ScheduleElement& newtransm,
                                            const ScheduleElement& oldtransm,
                                            unsigned offset_a, unsigned tile_size) {
    // Calculate slots used by the two transmissions and see if there is any common value
    unsigned period_a = toInt(newtransm.getPeriod());
    unsigned period_b = toInt(oldtransm.getPeriod());
    unsigned periodslots_a = period_a * tile_size;
    unsigned periodslots_b = period_b * tile_size;
    unsigned schedule_slots = lcm(period_a, period_b) * tile_size;

    for(unsigned slot_a=offset_a; slot_a < schedule_slots; slot_a += periodslots_a) {
        for(unsigned slot_b=oldtransm.getOffset(); slot_b < schedule_slots; slot_b += periodslots_b) {
            if(slot_a == slot_b)
                return true;
        }
    }
    return false;
}

bool ScheduleComputation::checkUnicityConflict(const ScheduleElement& new_transmission,
                                               const ScheduleElement& old_transmission) {
    // Unicity check: no activity for TX or RX node on a given timeslot
    unsigned char tx_a = new_transmission.getTx();
    unsigned char rx_a = new_transmission.getRx();
    unsigned char tx_b = old_transmission.getTx();
    unsigned char rx_b = old_transmission.getRx();

    return (tx_a == tx_b) || (tx_a == rx_b) || (rx_a == tx_b) || (rx_a == rx_b);
}

bool ScheduleComputation::checkInterferenceConflict(const ScheduleElement& new_transmission,
                                                    const ScheduleElement& old_transmission) {
    // Interference check: no TX and RX for nodes at 1-hop distance in the same timeslot
    // Check that neighbors of transmitting node (TX) aren't receiving (RX)
    // And neighbors of receiving node (RX) aren't transmitting (TX)
    unsigned char tx_a = new_transmission.getTx();
    unsigned char rx_a = new_transmission.getRx();
    unsigned char tx_b = old_transmission.getTx();
    unsigned char rx_b = old_transmission.getRx();

    bool conflict = false;
    conflict |= network_graph.hasEdge(tx_a, rx_b);
    conflict |= network_graph.hasEdge(rx_a, tx_b);
    return conflict;
}

void ScheduleComputation::printSchedule(const Schedule& sched) {
    printf("ID  TX  RX  PER OFF\n");
    for(auto& elem : sched.schedule) {
        printf("%d   %d-->%d   %d   %d\n", elem.getKey(), elem.getTx(), elem.getRx(),
                                        toInt(elem.getPeriod()), elem.getOffset());
    }
}

void ScheduleComputation::printStreams(const std::vector<MasterStreamInfo>& stream_list) {
    printf("ID SRC DST  PER STS\n");
    for(auto& stream : stream_list) {
        printf("%d   %d-->%d   %2d  ", stream.getKey(), stream.getSrc(),
               stream.getDst(), toInt(stream.getPeriod()));
        switch(stream.getStatus()){
        case MasterStreamStatus::ACCEPTED:
            printf("ACC");
            break;
        case MasterStreamStatus::ESTABLISHED:
            printf("EST");
            break;
        case MasterStreamStatus::LISTEN:
            printf("LIS");
            break;
        }
        printf("\n");
    }
}

void ScheduleComputation::printStreamList(const std::list<std::list<ScheduleElement>>& stream_list) {
    printf("ID  TX  RX  PER\n");
    for (auto block : stream_list)
        for (auto stream : block)
            printf("%d  %d-->%d   %d\n", stream.getKey(), stream.getTx(),
                                       stream.getRx(), toInt(stream.getPeriod()));
}

std::list<std::list<ScheduleElement>> Router::run(std::vector<MasterStreamInfo>& stream_list) {
    std::list<std::list<ScheduleElement>> routed_streams;
    if(SCHEDULER_DETAILED_DBG)
        printf("[SC] Routing %d stream requests\n", stream_list.size());
    // Cycle over stream_requests
    for(auto& stream: stream_list) {
        unsigned char src = stream.getSrc();
        unsigned char dst = stream.getDst();
        if(SCHEDULER_DETAILED_DBG)
            printf("[SC] Routing stream %d->%d\n", src, dst);

        // Check if 1-hop
        if(scheduler.network_graph.hasEdge(src, dst)) {
            // Add stream as is to final List
            if(SCHEDULER_DETAILED_DBG)
                printf("[SC] Stream %d->%d is single hop\n", src, dst);
            Redundancy redundancy = stream.getRedundancy();
            // Single-hop stream, downgrade spatial redundancies to non spatial ones
            if (redundancy == Redundancy::DOUBLE_SPATIAL) {
                redundancy = Redundancy::DOUBLE;
                stream.setRedundancy(redundancy);
            }
            if (redundancy == Redundancy::TRIPLE_SPATIAL) {
                redundancy = Redundancy::TRIPLE;
                stream.setRedundancy(redundancy);
            }
            std::list<ScheduleElement> single_hop;
            single_hop.push_back(ScheduleElement(stream));
            routed_streams.push_back(single_hop);
            // Temporal redundancy
            if(redundancy == Redundancy::DOUBLE)
                routed_streams.push_back(single_hop);
            if(redundancy == Redundancy::TRIPLE) {
                routed_streams.push_back(single_hop);
                routed_streams.push_back(single_hop);
            }
            continue;
        }
        // Otherwise run BFS
        std::list<unsigned char> path = breadthFirstSearch(stream);
        // Calculate path lenght (for limiting DFS)
        unsigned int sol_size = path.size();
        if(path.empty()) {
            if(SCHEDULER_DETAILED_DBG)
                printf("[SC] No path found, stream not scheduled\n");
            continue;
        }else if((sol_size-1) > maxHops) {
            if(SCHEDULER_DETAILED_DBG)
                printf("[SC] Found path of hops=%d > maxHops=%d, stream not scheduled\n", sol_size - 1, maxHops);
            continue;
        }
        // Print routed path
        if(SCHEDULER_DETAILED_DBG) {
            printf("[SC] Found path of length %d:\n", sol_size);
            printPath(path);
        }
        std::list<ScheduleElement> schedule = pathToSchedule(path, stream);
        // Insert routed path in place of multihop stream
        routed_streams.push_back(schedule);
        Redundancy redundancy = stream.getRedundancy();
        // Temporal redundancy
        if(redundancy == Redundancy::DOUBLE ||
           redundancy == Redundancy::TRIPLE_SPATIAL)
            routed_streams.push_back(schedule);
        if(redundancy == Redundancy::TRIPLE) {
            routed_streams.push_back(schedule);
            routed_streams.push_back(schedule);
        }
        // Spatial redundancy
        if(redundancy == Redundancy::DOUBLE_SPATIAL ||
           redundancy == Redundancy::TRIPLE_SPATIAL) {
            // Runs depth first search to get a list of possible paths,
            // with maximum hops = first_solution + more_hops, among
            // which we choose the redundant path.
            if(SCHEDULER_DETAILED_DBG)
                printf("[SC] Searching alternative paths of length %d + %d\n", sol_size, more_hops);
            std::list<std::list<unsigned char>> extra_paths = depthFirstSearch(stream, sol_size + more_hops);
            // Choose best secondary path for redundancy
            if(!extra_paths.empty()) {
                // Print secondary paths found
                if(SCHEDULER_DETAILED_DBG) {
                    printf("[SC] Secondary Paths found: \n");
                    printPathList(extra_paths);
                }
                // Remove primary path from solutions
                extra_paths.remove(path);
                if(SCHEDULER_DETAILED_DBG) {
                    printf("[SC] Secondary Paths after removing primary path: \n");
                    printPathList(extra_paths);
                }
                // If the only path is the primary path (extra_paths empty)
                if(!extra_paths.size()) {
                    printf("[SC] The only path is the primary path.\nDowngrading from spatial to temporal redundancy\n");
                    // Downgrade spatial redundancies to non spatial ones
                    if (redundancy == Redundancy::DOUBLE_SPATIAL) {
                        redundancy = Redundancy::DOUBLE;
                        stream.setRedundancy(redundancy);
                        routed_streams.push_back(schedule);
                    }
                    if (redundancy == Redundancy::TRIPLE_SPATIAL) {
                        redundancy = Redundancy::TRIPLE;
                        stream.setRedundancy(redundancy);
                        routed_streams.push_back(schedule);
                    }
                    continue;
                }
                // Find paths without intermediate nodes in common with primary path
                std::list<std::list<unsigned char>> indip_paths = findIndependentPaths(extra_paths, path);
                // If the independent set is not empty choose shortest solution
                std::list<unsigned char> solution;
                if(indip_paths.size()) {
                    if(SCHEDULER_DETAILED_DBG) {
                        printf("[SC] Indipendent Paths found: \n");
                        printPathList(indip_paths);
                    }
                    solution = findShortestPath(indip_paths);
                }
                else { // Else pick the shortest non independent solution
                    if(SCHEDULER_DETAILED_DBG)
                        printf("[SC] Indipendent Paths not found: \n");
                    solution = findShortestPath(extra_paths);
                }
                if(SCHEDULER_DETAILED_DBG) {
                    printf("[SC] The best redundand path is:\n");
                    printPath(solution);
                }
                std::list<ScheduleElement> schedule = pathToSchedule(solution, stream);
                // Add secondary path to list of routed streams
                routed_streams.push_back(schedule);
                //TODO: possible improvement: choose the path with least common nodes
            }
        }
    }
    return routed_streams;
}

std::list<unsigned char> Router::breadthFirstSearch(MasterStreamInfo stream) {
    unsigned char root = stream.getSrc();
    unsigned char dest = stream.getDst();
    // Check that the source node exists in the graph
    if(!scheduler.network_graph.hasNode(root)) {
        if(SCHEDULER_DETAILED_DBG)
            printf("[SC] Error: source node is not present in TopologyMap\n");
        return std::list<unsigned char>();
    }
    // Check that the destination node exists in the graph
    if(!scheduler.network_graph.hasNode(dest)) {
        if(SCHEDULER_DETAILED_DBG)
            printf("[SC] Error: destination node is not present in TopologyMap\n");
        return std::list<unsigned char>();
    }
    // V = maximum number of nodes in the network
    unsigned int V = scheduler.netconfig.getMaxNodes();
    // Mark all the vertices as not visited
    std::vector<bool> visited(V, false);

    // Create a queue for BFS
    std::list<unsigned char> open_set;
    // Dictionary to save parent-of relations between vertices
    std::map<const unsigned char, unsigned char> parent_of;
    // Mark the current node as visited and enqueue it
    visited.at(root) = true;
    open_set.push_back(root);
    /* The root node is the only to have itself as predecessor */
    parent_of[root] = root;

    while (!open_set.empty()) {
        //Get and remove first element of open set
        unsigned char subtree_root = open_set.front();
        open_set.pop_front();
        if (subtree_root == dest) return construct_path(subtree_root, parent_of);
        // Get all adjacent vertices of the dequeued vertex
        std::vector<unsigned char> adjacence = scheduler.network_graph.getEdges(subtree_root);
        for (unsigned char child : adjacence) {
            // If child is already visited, skip.
            if (visited.at(child) == true) continue;
            // If child is not already in open_set
            if(!(std::find(open_set.begin(), open_set.end(), child) != open_set.end())) {
                // Add to parent_of structure
                parent_of[child] = subtree_root;
                // Add to open_set
                open_set.push_back(child);
            }
        }
        visited.at(subtree_root) = true;
    }
    // If the execution ends here, src and dst are not connected in the graph
    if(SCHEDULER_DETAILED_DBG)
        printf("[SC] Error: source and destination node are not connected in TopologyMap\n");
    return std::list<unsigned char>();
}

std::list<unsigned char> Router::construct_path(unsigned char node,
                                                std::map<const unsigned char, unsigned char>& parent_of) {
    /* Construct path by following the parent-of relation to the root node */
    std::list<unsigned char> path;
    path.push_back(node);
    /* The root node is the only to have itself as predecessor */
    while(parent_of[node] != node) {
        node = parent_of[node];
        path.push_back(node);
    }
    path.reverse();
    return path;
}

std::list<ScheduleElement> Router::pathToSchedule(const std::list<unsigned char>& path,
                                                  const MasterStreamInfo& stream) {
    /* Es: path: 0 1 2 3 schedule: 0->1 1->2 2->3 */
    std::list<ScheduleElement> result;
    if(!path.size())
        return result;
    /* Convert path (list of nodes) to schedule (list of StreamElement),
       copying stream parameters from the multi-hop stream we are scheduling */
    unsigned char tx = path.front();
    for(auto& rx : path) {
        // Skip first round of for because we are ciclyng with rx node
        if(rx == tx) continue;
        result.push_back(ScheduleElement(stream, tx, rx));
        tx = rx;
    }
    return result;
}

void Router::printPath(const std::list<unsigned char>& path) {
    for(auto& node : path) {
        printf(" %d ", node);                      
    }
    printf("\n");
}

void Router::printPathList(const std::list<std::list<unsigned char>>& path_list) {
    for(auto& p : path_list) {
        printPath(p);
    }
}

std::list<std::list<unsigned char>> Router::depthFirstSearch(MasterStreamInfo stream,
                                                               unsigned int limit) {
    unsigned char src = stream.getSrc();
    unsigned char dst = stream.getDst();

    // V = maximum number of nodes in the network
    unsigned int V = scheduler.netconfig.getMaxNodes();
    // Mark all the vertices as not visited
    std::vector<bool> visited(V, false);
    // Create a list to store a path
    std::list<unsigned char> path;
    // Create a list to store all paths found
    std::list<std::list<unsigned char>> all_paths;

    // Run the recursive function
    // NOTE: we do limit+1 otherwise only solution of size limit-1 are found
    dfsRun(src, dst, limit + 1, visited, path, all_paths);
    return all_paths;
}

void Router::dfsRun(unsigned char start, unsigned char target, unsigned int limit,
                    std::vector<bool>& visited, std::list<unsigned char>& path,
                    std::list<std::list<unsigned char>>& all_paths) {
    // Mark current node in visited, and store it in path
    visited.at(start) = true;
    path.push_back(start);

    // If current node == target -> return path
    if(start == target) {
        if(SCHEDULER_DETAILED_DBG)
            printf("[SC] Found redundand path of length %d, ", path.size());
        all_paths.push_back(path);
        if(SCHEDULER_DETAILED_DBG)
            printf("[SC] total paths found: %d\n", all_paths.size());
    }
    else{ // If current node != target
        // Recur for all the vertices adjacent to current vertex
        std::vector<unsigned char> adjacence = scheduler.network_graph.getEdges(start);
        for (unsigned char child : adjacence) {
            // Maximum depth reached
            if(limit == 0)
                continue;
            limit--;
            if(!visited.at(child))
                dfsRun(child, target, limit, visited, path, all_paths);
        }
    }
    // Remove current vertex from path[] and mark it as unvisited
    path.pop_back();
    visited.at(start) = false;
}

std::list<unsigned char> Router::findShortestPath(const std::list<std::list<unsigned char>>& path_list) {
    unsigned int min = path_list.front().size();
    std::list<unsigned char> result = path_list.front();
    for(auto& path : path_list) {
        if(path.size() < min) {
            min = path.size();
            result = path;
        }
    }
    return result;
}

std::list<std::list<unsigned char>> Router::findIndependentPaths(const std::list<std::list<unsigned char>>& path_list,
                                                                 const std::list<unsigned char> primary) {
    std::list<std::list<unsigned char>> result;
    std::list<unsigned char> nodes = primary;
    nodes.pop_front();
    nodes.pop_back();
    std::unordered_set<unsigned char> common_nodes(std::begin(nodes), std::end(nodes));
    if(SCHEDULER_DETAILED_DBG) {
        printf("[SC] Nodes to avoid:\n");
        for(auto& n : common_nodes) {
            printf(" %d ", n);
        }
        printf("\n");
    }
    for(auto& p : path_list) {
        bool ind = true;
        for(auto& e : p) {
            std::unordered_set<unsigned char>::const_iterator got = common_nodes.find(e);
            if (got != common_nodes.end()) {
                // If we found one element in common, there is no need to look further
                ind = false;
                break;
            }
        }
        if(ind)
            result.push_back(p);
    }
    return result;
}

}
