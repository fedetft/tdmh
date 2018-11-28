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
            if(topology_ctx.getTopologyMap().isEmpty() ||
               !(topology_ctx.getTopologyMap().wasModified() ||
                 stream_mgmt.wasModified())) {
                printf("-");
                continue;
            }
            // ELSE we can begin scheduling 
            // Take snapshot of stream requests and network topology
            stream_snapshot = stream_mgmt;
            topology_map = topology_ctx.getTopologyMap();
        }
        /* IMPORTANT!: From now on use only the snapshot class `stream_snapshot` */
        // Get network config to get info about the tile and superframe structure
        // used to get controlsuperframestructure
        NetworkConfiguration netconfig = mac_ctx.getNetworkConfig();

        printf("\n#### Starting schedule computation ####\n");

        // NOTE: Debug topology print
        printf("Topology:\n");
        for(auto it : topology_map.getEdges())
            printf("[%d - %d]\n", it.first, it.second);

        /* Here we prioritize established streams over new ones,
           also we try to avoid unnecessary work like re-scheduling or
           re-routing when topology or stream list did not change. */

        printf("Total streams: %d\n", stream_snapshot.getStreamNumber());
        auto established_streams = stream_snapshot.getEstablishedStreams();
        auto new_streams = stream_snapshot.getNewStreams();
        printf("Established streams: %d\n", established_streams.size());
        printf("New streams: %d\n", new_streams.size());
        printStreams(established_streams);
        printStreams(new_streams);

        /* If topology changed or a stream was removed:
           clear current schedule and reroute + reschedule established streams */
        if(topology_map.wasModified() || stream_mgmt.wasRemoved()) {
            printf("Topology changed or a stream was removed, Re-scheduling established streams\n");
            schedule.clear();
            auto new_schedule = routeAndScheduleStreams(established_streams, netconfig);
            schedule = std::move(new_schedule);
        }
        else {
            printf("Topology and current streams did not change, Keep established schedule\n");
            /* No action is needed since the old schedule is kept unless
               it is explicitly removed with routed_streams.clear() */
        }
        /* If there are new streams:
           route + schedule them and add them to existing schedule */
        if(stream_mgmt.hasNewStreams()) {
            printf("Scheduling new streams\n");
            //Sort new streams based on highest period first
            std::sort(new_streams.begin(), new_streams.end(),
                      [](StreamManagementElement a, StreamManagementElement b) {
                          return toInt(a.getPeriod()) > toInt(b.getPeriod());});
            //printf("New streams after sorting:\n");
            //printStreams(new_streams);
            auto new_schedule = routeAndScheduleStreams(new_streams, netconfig);
            schedule.insert(schedule.end(), new_schedule.begin(), new_schedule.end());
            //Mark successfully scheduled streams as established
            for(auto& stream: new_schedule) {
                printf("Setting stream %d to ESTABLISHED\n", stream.getKey());
                stream_mgmt.setStreamStatus(stream.getKey(), StreamStatus::ESTABLISHED);
            }
        }

        printf("Final schedule\n");
        printSchedule();

        printf("Stream list after scheduling\n");
        printStreams(stream_mgmt.getStreams());

        // To avoid caching of stdout
        fflush(stdout);

        // Clear modified bit to detect changes to topology or streams
        topology_ctx.clearModifiedFlag();
        stream_mgmt.clearFlags();
    }
}

std::vector<ScheduleElement> ScheduleComputation::routeAndScheduleStreams(
                                                  std::vector<StreamManagementElement> stream_list,
                                                  NetworkConfiguration netconfig) {
    if(!stream_list.empty()) {
        Router router(*this, 1, 2);
        printf("## Routing ##\n");
        // Run router to route multi-hop streams and get multiple paths
        auto routed_streams = router.run(stream_list);
        printf("Stream list after routing:\n");
        printStreamList(routed_streams);

        // Schedule expanded streams, avoiding conflicts
        printf("## Scheduling ##\n");
        auto new_schedule = scheduleStreams(routed_streams, netconfig);
        return new_schedule;
    }
    else {
        printf("Stream list empty, scheduling done.\n");
        std::vector<ScheduleElement> empty;
        return empty;
    }

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

std::vector<ScheduleElement> ScheduleComputation::scheduleStreams(std::list<std::list<ScheduleElement>> routed_streams,
                                          NetworkConfiguration netconfig) {

    // Get network tile/superframe information
    ControlSuperframeStructure superframe = netconfig.getControlSuperframeStructure();
    unsigned tile_size = mac_ctx.getSlotsInTileCount();
    unsigned dataslots_downlinktile = mac_ctx.getDataSlotsInDownlinkTileCount();
    unsigned dataslots_uplinktile = mac_ctx.getDataSlotsInUplinkTileCount();
    unsigned downlink_size = tile_size - dataslots_downlinktile;
    unsigned uplink_size = tile_size - dataslots_uplinktile;

    printf("Network configuration:\n- tile_size: %d\n- downlink_size: %d\n- uplink_size: %d\n",
           tile_size, downlink_size, uplink_size);

    // Start with an empty schedule
    // If scheduling is successful, this vector will be moved to replace the "schedule" field
    std::vector<ScheduleElement> scheduled_transmissions;
    // Schedulesize value is equal to lcm(p1,p2,...,pn) or p1 for a single stream
    int schedule_size = 0;

    for(auto& stream : routed_streams) {
        int block_size = 0;
        bool stream_err = false;
        // Counter to last slot offset: ensures sequentiality
        unsigned last_offset = 0;
        for(auto& transmission : stream) {
            unsigned char tx = transmission.getTx();
            unsigned char rx = transmission.getRx();
            printf("Scheduling transmission %d,%d\n", tx, rx);
            // Connectivity check
            if(!topology_map.hasEdge(tx, rx)) {
                stream_err = true;
                printf("%d,%d are not connected in topology, cannot schedule stream\n", tx, rx);
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
            unsigned max_offset = (toInt(transmission.getPeriod()) * tile_size) - 1;
            for(unsigned offset = last_offset; offset < max_offset; offset++) {
                if(!checkDataSlot(offset, tile_size, superframe, downlink_size, uplink_size))
                    continue;
                printf("Checking offset %d\n", offset);
                // Cycle over already scheduled transmissions to check for conflicts
                bool conflict = false;
                for(auto& elem : scheduled_transmissions) {
                    // conflictPossible is a simple condition used to reduce number of conflict checks
                    if(slotConflictPossible(transmission, elem, offset, tile_size)) {
                        if(checkSlotConflict(transmission, elem, offset, tile_size, schedule_size)) {
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
                    unsigned period = toInt(transmission.getPeriod());
                    if(schedule_size == 0)
                        schedule_size = period;
                    else
                        schedule_size = lcm(schedule_size, period);
                    // Add transmission to schedule, and set schedule offset
                    scheduled_transmissions.push_back(transmission);
                    scheduled_transmissions.back().setOffset(offset);
                    printf("Scheduled transmission %d,%d with offset %d\n", tx, rx, offset);
                    // Successfully scheduled transmission, break timeslot cycle
                    break;
                    }
                else {
                    printf("Cannot schedule transmission %d,%d with offset %d\n", tx, rx, offset);
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
    return scheduled_transmissions;
}

// This check makes sure that data is not scheduled in control slots (Downlink, Uplink)
bool ScheduleComputation::checkDataSlot(unsigned offset, unsigned tile_size,
                                        ControlSuperframeStructure superframe,
                                        unsigned downlink_size,
                                        unsigned uplink_size) {
    // Calculate current tile number
    unsigned tile = offset / tile_size; 
    // Calculate position in current tile
    unsigned slot = offset % tile_size;
    if((superframe.isControlDownlink(tile) && (slot <= downlink_size)) ||
       (superframe.isControlUplink(tile) && (slot <= uplink_size)))
        return false;
    else return true;
}

// This easy check is a necessary condition for a slot conflict,
// if the result is false, then a conflict cannot happen
// It can be used to avoid nested loops
bool ScheduleComputation::slotConflictPossible(ScheduleElement newtransm,
                                               ScheduleElement oldtransm,
                                               unsigned offset, unsigned tile_size) {
    // Compare offsets relative to current tile of two transmissions
    return ((offset % tile_size) == (oldtransm.getOffset() % tile_size));
}

// Extensive check to be used when slotConflictPossible returns true
bool ScheduleComputation::checkSlotConflict(ScheduleElement newtransm,
                                            ScheduleElement oldtransm,
                                            unsigned offset_a, unsigned tile_size,
                                            unsigned schedule_size) {
    // Calculate slots used by the two transmissions and see if there is any common value
    unsigned periodslots_a = toInt(newtransm.getPeriod()) * tile_size;
    unsigned periodslots_b = toInt(oldtransm.getPeriod()) * tile_size;

    for(unsigned slot_a=offset_a; slot_a < schedule_size; slot_a += periodslots_a) {
        for(unsigned slot_b=oldtransm.getOffset(); slot_b < schedule_size; slot_b += periodslots_b) {
            if(slot_a == slot_b)
                return true;
        }
    }
    return false;
}

bool ScheduleComputation::checkUnicityConflict(ScheduleElement new_transmission,
                                               ScheduleElement old_transmission) {
    // Unicity check: no activity for TX or RX node on a given timeslot
    unsigned char tx_a = new_transmission.getTx();
    unsigned char rx_a = new_transmission.getRx();
    unsigned char tx_b = old_transmission.getTx();
    unsigned char rx_b = old_transmission.getRx();

    return (tx_a == tx_b) || (tx_a == rx_b) || (rx_a == tx_b) || (rx_a == rx_b);
}

bool ScheduleComputation::checkInterferenceConflict(ScheduleElement new_transmission,
                                                    ScheduleElement old_transmission) {
    // Interference check: no TX and RX for nodes at 1-hop distance in the same timeslot
    // Check that neighbors of transmitting node (TX) aren't receiving (RX)
    // And neighbors of receiving node (RX) aren't transmitting (TX)
    unsigned char tx_a = new_transmission.getTx();
    unsigned char rx_a = new_transmission.getRx();
    unsigned char tx_b = old_transmission.getTx();
    unsigned char rx_b = old_transmission.getRx();

    bool conflict = false;
    conflict |= topology_map.hasEdge(tx_a, rx_b);
    conflict |= topology_map.hasEdge(rx_a, tx_b);
    return conflict;
}

void ScheduleComputation::printSchedule() {
    printf("ID   TX  RX  PER OFF\n");
    for(auto& elem : schedule) {
        printf("%d  %d-->%d   %d   %d\n", elem.getKey(), elem.getTx(), elem.getRx(),
                                        toInt(elem.getPeriod()), elem.getOffset());
    }
}

void ScheduleComputation::printStreams(std::vector<StreamManagementElement> stream_list) {
    printf("ID  SRC DST PER STS\n");
    for(auto& stream : stream_list) {
        printf("%d  %d-->%d   %d  ", stream.getKey(), stream.getSrc(),
               stream.getDst(), toInt(stream.getPeriod()));
        switch(stream.getStatus()){
        case StreamStatus::NEW:
            printf("NEW");
            break;
        case StreamStatus::ESTABLISHED:
            printf("EST");
            break;
        case StreamStatus::REJECTED:
            printf("REJ");
            break;
        case StreamStatus::CLOSED:
            printf("CLO");
            break;
        }
        printf("\n");
    }
}

void ScheduleComputation::printStreamList(std::list<std::list<ScheduleElement>> stream_list) {
    printf("ID  TX  RX  PER\n");
    for (auto block : stream_list)
        for (auto stream : block)
            printf("%d  %d-->%d   %d\n", stream.getKey(), stream.getTx(),
                                       stream.getRx(), toInt(stream.getPeriod()));
}

std::list<std::list<ScheduleElement>> Router::run(std::vector<StreamManagementElement> stream_list) {
    std::list<std::list<ScheduleElement>> routed_streams;
    printf("Routing %d stream requests\n", stream_list.size());
    // Cycle over stream_requests
    for(auto& stream: stream_list) {
        unsigned char src = stream.getSrc();
        unsigned char dst = stream.getDst();
        printf("Routing stream %d->%d\n", src, dst);

        // Check if 1-hop
        if(scheduler.topology_map.hasEdge(src, dst)) {
            // Add stream as is to final List
            printf("Stream %d->%d is single hop\n", src, dst);
            std::list<ScheduleElement> single_hop;
            single_hop.push_back(ScheduleElement(stream));
            routed_streams.push_back(single_hop);
            continue;
        }
        // Otherwise run BFS
        std::list<ScheduleElement> path = breadthFirstSearch(stream);
        if(!path.empty()) {
            // Print routed path
            printf("Path found: \n");
            for(auto& s : path) {
                printf("%d->%d ", s.getTx(), s.getRx());
            }
            printf("\n");
        }
        // Insert routed path in place of multihop stream
        routed_streams.push_back(path);
        // TODO: If redundancy, run DFS
        if(multipath) {
            //int sol_size = path.size();

        }
    }
    return routed_streams;
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
    path.push_back(ScheduleElement(stream.getSrc(), stream.getDst(), stream.getSrcPort(),
                                   stream.getDstPort(), src, dst, stream.getPeriod()));
    /* The root node is the only to have itself as predecessor */
    while(parent_of[src] != src) {
        dst = src;
        src = parent_of[dst];
        path.push_back(ScheduleElement(stream.getSrc(), stream.getDst(), stream.getSrcPort(),
                                       stream.getDstPort(), src, dst, stream.getPeriod()));
    }
    path.reverse();
    return path;
}
}
