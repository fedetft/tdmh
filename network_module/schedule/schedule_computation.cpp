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

#include <list>
#include <utility>
#include <stdio.h>
#include "../debug_settings.h"
#include "schedule_computation.h"
#include "../uplink/stream_management/stream_management_context.h"
#include "../uplink/stream_management/stream_management_element.h"
#include "../uplink/topology/topology_context.h"
#include "../mac_context.h"

/**
 * The methods of this class run on a separate thread, don't use print_dbg()
 * to avoid race conditions, use printf() or cout instead.
 */

namespace mxnet {

ScheduleComputation::ScheduleComputation(MACContext& mac_ctx, MasterMeshTopologyContext& topology_ctx) : 
    topology_ctx(topology_ctx), mac_ctx(mac_ctx), topology_map(mac_ctx.getNetworkConfig().getMaxNodes()) {}

void ScheduleComputation::startThread() {
    if (scthread == NULL)
#ifdef _MIOSIX
        // Thread launched using static function threadLauncher with the class instance as parameter.
        scthread = miosix::Thread::create(&ScheduleComputation::threadLauncher, 2048, miosix::PRIORITY_MAX-2, this, miosix::Thread::JOINABLE);
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
    for(;;) {   
        // Mutex lock to access stream list.
        {
#ifdef _MIOSIX
            miosix::Lock<miosix::Mutex> lck(sched_mutex);
#else
            std::unique_lock<std::mutex> lck(sched_mutex);
#endif
            // Wait for beginScheduling()
            sched_cv.wait(lck);
            while(stream_mgmt.getStreamNumber() == 0) {
                printf("No stream to schedule, waiting...\n");
                // Condition variable to wait for streams to schedule.
                sched_cv.wait(lck);
            }
            // IF stream list not changed AND topology not changed
            // Skip to next for loop cycle
            if(!(topology_ctx.getTopologyMap().wasModified() || stream_mgmt.wasModified())) {
                printf("-");
                continue;
            }
            // ELSE we can begin scheduling 
            // Take snapshot of stream requests and network topology
            stream_snapshot = stream_mgmt;
            topology_map = topology_ctx.getTopologyMap();
        }
        /* From now on use only the snapshot class `stream_snapshot` */
        // Retrieve information about the tile and superframe structure
        // Get current controlsuperframe configuration
        ControlSuperframeStructure superframe = mac_ctx.getNetworkConfig().getControlSuperframeStructure();
        // Get tile duration
        unsigned long long tile_duration = mac_ctx.getNetworkConfig().getTileDuration();

        printf("\n#### Starting schedule computation ####\n\n");
        printStreams();

        // NOTE: Debug topology print
        printf("Topology:\n");
        for(auto it : topology_map.getEdges())
            printf("[%d - %d]\n", it.first, it.second);

        /* Here we prioritize established streams over new ones,
           also we try to avoid unnecessary work like re-scheduling or
           re-routing when topology or stream list did not change. */

        /* If topology changed or a stream was removed:
           clear current schedule and reroute + reschedule established streams */
        if(topology_map.wasModified() || stream_mgmt.wasRemoved()) {
            printf("Topology changed or a stream was removed, Re-scheduling all streams\n");
            routed_streams.clear();
            routeAndScheduleStreams(stream_snapshot.getEstablishedStreams(), tile_duration, superframe);
        }
        else {
            printf("Topology did not change, Keep current schedule\n");
            /* No action is needed since the old schedule is kept unless
               it is explicitly removed with routed_streams.clear() */
        }
        /* If there are new streams:
           route + schedule them and add them to existing schedule */
        if(stream_mgmt.wasAdded()) {
            printf("New stream added, scheduling new stream\n");
            routeAndScheduleStreams(stream_snapshot.getNewStreams(), tile_duration, superframe);
        }

        printSchedule();
        // To avoid caching of stdout
        fflush(stdout);

        // Clear modified bit to detect changes to topology or streams
        topology_ctx.clearModifiedFlag();
        stream_mgmt.clearModifiedFlag();
    }
}

void ScheduleComputation::routeAndScheduleStreams(std::vector<StreamManagementElement> stream_list, unsigned long long tile_duration, ControlSuperframeStructure superframe) {
    Router router(*this, 1, 2);
    printf("## Routing ##\n");
    // Run router to route multi-hop streams and get multiple paths
    router.run();
    printf("Stream list after routing:\n");
    printStreamList(routed_streams);

    // Schedule expanded streams, avoiding conflicts
    printf("## Scheduling ##\n");
    scheduleStreams(tile_duration, superframe);
}

void ScheduleComputation::addNewStreams(std::vector<StreamManagementElement>& smes) {
#ifdef _MIOSIX
    miosix::Lock<miosix::Mutex> lck(sched_mutex);
#else
    std::unique_lock<std::mutex> lck(sched_mutex);
#endif
    stream_mgmt.receive(smes);
}

void ScheduleComputation::open(StreamManagementElement sme) {
#ifdef _MIOSIX
    miosix::Lock<miosix::Mutex> lck(sched_mutex);
#else
    std::unique_lock<std::mutex> lck(sched_mutex);
#endif
    stream_mgmt.open(sme);
}

void ScheduleComputation::scheduleStreams(unsigned long long tile_duration, ControlSuperframeStructure superframe) {
    // Start with an empty schedule
    // If scheduling is successful, this vector will be moved to replace the "schedule" field
    std::vector<ScheduleElement> scheduled_transmissions;
    // Schedulesize value is equal to lcm(p1,p2,...,pn) or p1 for a single stream
    int schedule_size = 0;

    for(auto stream : routed_streams) {
        int block_size = 0;
        bool stream_err = false;
        // Counter to last slot offset: ensures sequentiality
        int last_offset = 0;
        for(auto transmission : stream) {
            unsigned char src = transmission.getSrc();
            unsigned char dst = transmission.getDst();
            printf("Scheduling transmission %d,%d\n", src, dst);
            // Connectivity check
            if(!topology_map.hasEdge(src,dst)) {
                stream_err = true;
                printf("%d,%d are not connected in topology, cannot schedule stream\n", src, dst);
            }
            // If a transmission cannot be scheduled, undo the whole stream
            if(stream_err) {
                printf("Transmission scheduling failed, removing rest of the stream\n");
                for(int i=0; i<block_size; i++) {
                    scheduled_transmissions.pop_back();
                }
                //TODO recalculate schedule size without new stream
                // Skip to next block
                break;
                }
            // The offset must be smaller than (stream period * minimum period size)-1
            // with minimum period size being equal to the tile lenght (by design)
            // Otherwise the resulting stream won't be periodic
            unsigned int max_offset = (toInt(transmission.getPeriod()) * tile_duration) - 1;
            for(unsigned int offset = last_offset; offset < max_offset; offset++) {
                printf("Checking offset %d\n", offset);
                // Cycle over already scheduled transmissions to check for conflicts
                bool conflict = false;
                for(auto elem : scheduled_transmissions) {
                    // conflictPossible is a simple condition used to reduce number of conflict checks
                    if(slotConflictPossible(transmission, elem, offset, tile_duration)) {
                        if(checkSlotConflict(transmission, elem, offset, tile_duration, schedule_size)) {
                            printf("Other transmissions have timeslots in common\n");
                            /* Conflict checks */
                            // Unicity check: no activity for src or dst node in a given timeslot
                            if(checkUnicityConflict(transmission, elem)) {
                                conflict |= true;
                                printf("Unicity conflict!\n");
                            }
                            // Interference check: no TX and RX for nodes at 1-hop distance in the same timeslot
                            if(checkInterferenceConflict(transmission, elem)) {
                                conflict |= true;
                                printf("Interference conflict!\n");
                            }
                            if(conflict)
                                // Avoid checking other streams when a conflict is found
                                break;
                        }
                    }
                    // else conflict is not possible
                }
                if(!conflict) {
                    last_offset = offset;
                    block_size++;
                    // Calculate new schedule size
                    int period = toInt(transmission.getPeriod());
                    if(schedule_size == 0)
                        schedule_size = period;
                    else
                        schedule_size = lcm(schedule_size, period);
                    // Add transmission to schedule, and set schedule offset
                    scheduled_transmissions.push_back(transmission);
                    scheduled_transmissions.back().setOffset(offset);
                    printf("Scheduled transmission %d,%d with offset %d\n", src, dst, offset);
                    // Successfully scheduled transmission, break timeslot cycle
                    break;
                    }
                else {
                    printf("Cannot schedule stream %d,%d with offset %d\n", src, dst, offset);
                    //Try to schedule in next timeslot

                }
            }
            // Next transmission of stream should start from next timeslot
            // to guarantee sequentiality in transmissions of the same stream
            last_offset++;
            // If we are in the last timeslot and have a conflict,
            // cannot reschedule on another timeslot
            if(last_offset == max_offset)
                stream_err = true;
        }
    }
    printf("Final schedule size: %d\n", schedule_size);
    schedule = std::move(scheduled_transmissions);
}



// This easy check is a necessary condition for a slot conflict,
// if the result is false, then a conflict cannot happen
// It can be used to avoid nested loops
bool ScheduleComputation::slotConflictPossible(ScheduleElement newtransm, ScheduleElement oldtransm, int offset, int tile_duration) {
    // Compare offsets relative to current tile of two transmissions
    return ((offset % tile_duration) == (oldtransm.getOffset() % tile_duration));
}

// Extensive check to be used when slotConflictPossible returns true
bool ScheduleComputation::checkSlotConflict(ScheduleElement newtransm, ScheduleElement oldtransm, int offset_a, int tile_duration, int schedule_size) {
    // Calculate slots used by the two transmissions and see if there is any common value
    int periodslots_a = toInt(newtransm.getPeriod()) * tile_duration;
    int periodslots_b = toInt(oldtransm.getPeriod()) * tile_duration;

    for(int slot_a=offset_a; slot_a < schedule_size; slot_a += periodslots_a) {
        for(int slot_b=oldtransm.getOffset(); slot_b < schedule_size; slot_b += periodslots_b) {
            if(slot_a == slot_b)
                return true;
        }
    }
    return false;
}

bool ScheduleComputation::checkUnicityConflict(ScheduleElement new_transmission, ScheduleElement old_transmission) {
    // Unicity check: no activity for src or dst node on a given timeslot
    unsigned char src_a = new_transmission.getSrc();
    unsigned char dst_a = new_transmission.getDst();
    unsigned char src_b = old_transmission.getSrc();
    unsigned char dst_b = old_transmission.getDst();

    return (src_a == src_b) || (src_a == dst_b) || (dst_a == src_b) || (dst_a == dst_b);
}

bool ScheduleComputation::checkInterferenceConflict(ScheduleElement new_transmission, ScheduleElement old_transmission) {
    // Interference check: no TX and RX for nodes at 1-hop distance in the same timeslot
    // Check that neighbors of src (TX) aren't receiving (RX)
    // And neighbors of dst (RX) aren't transmitting (TX)
    unsigned char src_a = new_transmission.getSrc();
    unsigned char dst_a = new_transmission.getDst();
    unsigned char src_b = old_transmission.getSrc();
    unsigned char dst_b = old_transmission.getDst();

    bool conflict = false;
    conflict |= topology_map.hasEdge(src_a, dst_b);
    conflict |= topology_map.hasEdge(dst_a, src_b);
    return conflict;
}

void ScheduleComputation::printSchedule() {
    printf("ID  SRC DST PER OFF\n");
    for(auto elem : schedule) {
        printf("%d  %d-->%d   %d   %d\n", elem.getKey(), elem.getSrc(), elem.getDst(),
                                        toInt(elem.getPeriod()), elem.getOffset());
    }
}

void ScheduleComputation::printStreams() {
    printf("Stream requests:\n");
    printf("ID  SRC DST PER\n");
    int num_streams = stream_snapshot.getStreamNumber();
    // Cycle over stream_requests
    for(int i=0; i<num_streams; i++) {
        StreamManagementElement stream = stream_snapshot.getStream(i);
        printf("%d  %d-->%d   %d\n", stream.getKey(), stream.getSrc(), stream.getDst(), toInt(stream.getPeriod()));
    }
}

void ScheduleComputation::printStreamList(std::list<std::list<ScheduleElement>> stream_list) {
    printf("ID  SRC DST PER\n");
    for (auto block : stream_list)
        for (auto stream : block)
            printf("%d  %d-->%d   %d\n", stream.getKey(), stream.getSrc(), stream.getDst(), toInt(stream.getPeriod()));
}

void Router::run() {
    int num_streams = scheduler.stream_snapshot.getStreamNumber();
    printf("Routing %d stream requests\n", num_streams);
    // Cycle over stream_requests
    for(int i=0; i<num_streams; i++) {
        StreamManagementElement stream = scheduler.stream_snapshot.getStream(i);
        unsigned char src = stream.getSrc();
        unsigned char dst = stream.getDst();
        printf("Routing stream n.%d: %d,%d\n", i, src, dst);

        // Check if 1-hop
        if(scheduler.topology_map.hasEdge(src, dst)) {
            // Add stream as is to final List
            printf("Stream n.%d: %d,%d is single hop\n", i, src, dst);
            std::list<ScheduleElement> single_hop;
            single_hop.push_back(ScheduleElement(stream));
            scheduler.routed_streams.push_back(single_hop);
            continue;
        }
        // Otherwise run BFS
        std::list<ScheduleElement> path = breadthFirstSearch(stream);
        if(!path.empty()) {
            // Print routed path
            printf("Path found: \n");
            for(auto s : path) {
                printf("%d->%d ", s.getSrc(), s.getDst());
            }
            printf("\n");
        }
        // Insert routed path in place of multihop stream
        scheduler.routed_streams.push_back(path);
        // If redundancy, run DFS
        if(multipath) {
            //int sol_size = path.size();

        }
    }
}


std::list<ScheduleElement> Router::breadthFirstSearch(StreamManagementElement stream) {
    unsigned char root = stream.getSrc();
    unsigned char dest = stream.getDst();
    // Check that the source node exists in the graph
    if(!scheduler.topology_map.hasNode(root)) {
        printf("Error: source node is not present in TopologyMap\n");
        return std::list<ScheduleElement>();
    }
    // Check that the destination node exists in the graph
    if(!scheduler.topology_map.hasNode(dest)) {
        printf("Error: destination node is not present in TopologyMap\n");
        return std::list<ScheduleElement>();
    }
    // V = number of nodes in the network
    //TODO: implement topology_ctx.getnodecount()
    int V = 32;
    // Mark all the vertices as not visited
    //TODO: Can be turned to a bit-vector to save space
    bool visited[V];
    for(int i = 0; i < V; i++)
        visited[i] = false;

    // Create a queue for BFS
    std::list<unsigned char> open_set;
    // Dictionary to save parent-of relations between vertices
    std::map<const unsigned char, unsigned char> parent_of;
    // Mark the current node as visited and enqueue it
    visited[root] = true;
    open_set.push_back(root);
    /* The root node is the only to have itself as predecessor */
    parent_of[root] = root;

    while (!open_set.empty()) {
        //Get and remove first element of open set
        unsigned char subtree_root = open_set.front();
        open_set.pop_front();
        if (subtree_root == dest) return construct_path(stream, subtree_root, parent_of);
        // Get all adjacent vertices of the dequeued vertex
        std::vector<unsigned char> adjacence = scheduler.topology_map.getEdges(subtree_root);
        for (unsigned char child : adjacence) {
            // If child is already visited, skip.
            if (visited[child] == true) continue;
            // If child is not already in open_set
            if(!(std::find(open_set.begin(), open_set.end(), child) != open_set.end())) {
                // Add to parent_of structure
                parent_of[child] = subtree_root;
                // Add to open_set
                open_set.push_back(child);
            }
        }
        visited[subtree_root] = true;
    }
    // If the execution ends here, src and dst are not connected in the graph
    printf("Error: source and destination node are not connected in TopologyMap\n");
    return std::list<ScheduleElement>();
}

std::list<ScheduleElement> Router::construct_path(StreamManagementElement stream, unsigned char node, std::map<const unsigned char, unsigned char> parent_of) {
    /* Construct path by following the parent-of relation to the root node */
    std::list<ScheduleElement> path;
    unsigned char dst = node;
    unsigned char src = parent_of[node];
    // Copy over period, ports, ecc... from original multi-hop stream
    path.push_back(ScheduleElement(stream.getKey(), src, dst, stream.getSrcPort(),
                                   stream.getDstPort(), stream.getRedundancy(),
                                   stream.getPeriod(), stream.getPayloadSize()));
    /* The root node is the only to have itself as predecessor */
    while(parent_of[src] != src) {
        dst = src;
        src = parent_of[dst];
        path.push_back(ScheduleElement(stream.getKey(), src, dst, stream.getSrcPort(),
                                       stream.getDstPort(), stream.getRedundancy(),
                                       stream.getPeriod(), stream.getPayloadSize()));
    }
    path.reverse();
    return path;
}
}
