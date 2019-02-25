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
#include <unordered_set>
#include <utility>
#include <stdio.h>
#include "../debug_settings.h"
#include "schedule_computation.h"
#include "../uplink/stream_management/stream_management_element.h"
#include "../uplink/topology/topology_context.h"
#include "../mac_context.h"

/**
 * The methods of this class run on a separate thread, don't use print_dbg()
 * to avoid race conditions, use printf() or cout instead.
 */

namespace mxnet {

ScheduleComputation::ScheduleComputation(MACContext& mac_ctx, MasterMeshTopologyContext& topology_ctx) : 
    topology_ctx(topology_ctx), mac_ctx(mac_ctx), netconfig(mac_ctx.getNetworkConfig()),
    superframe(netconfig.getControlSuperframeStructure()),
    topology_map(mac_ctx.getNetworkConfig().getMaxNodes()),
    stream_mgmt(0) // Initialize StreamManager with ID=0 (Master node)
{
    // schedule_size must always be initialized to the number of tiles in superframe
    schedule_size = superframe.size();
}

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
                printf("-");
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
            if(ENABLE_STREAM_LIST_INFO_DBG)
                printf("\n[SC] Stream number in master: %d\n", stream_mgmt.getStreamNumber());
            stream_snapshot = stream_mgmt.getSnapshot();
            topology_map = topology_ctx.getTopologyMap();
        }
        /* IMPORTANT!: From now on use only the snapshot class `stream_snapshot` */


        if(ENABLE_SCHEDULE_COMP_INFO_DBG)
            printf("\n#### Starting schedule computation ####\n");

        // Used to check if the schedule has been modified or not
        bool changed = false;
        // NOTE: Debug topology print
        if(ENABLE_SCHEDULE_COMP_INFO_DBG) {
            printf("\nTopology:\n");
            for(auto it : topology_map.getEdges())
                printf("[%d - %d]\n", it.first, it.second);
        }

        if(ENABLE_STREAM_LIST_INFO_DBG){
            printf("[SC] Stream list before scheduling:\n");
            printStreams(stream_snapshot.getStreams());
        }

        /* Here we prioritize established streams over new ones,
           also we try to avoid unnecessary work like re-scheduling or
           re-routing when topology or stream list did not change. */
        if(ENABLE_SCHEDULE_COMP_INFO_DBG)
            printf("Total streams: %d\n", stream_snapshot.getStreamNumber());
        auto established_streams = stream_snapshot.getStreamsWithStatus(StreamStatus::ESTABLISHED);
        auto accepted_streams = stream_snapshot.getStreamsWithStatus(StreamStatus::ACCEPTED);
        if(ENABLE_SCHEDULE_COMP_INFO_DBG) {
            printf("Established streams: %d\n", established_streams.size());
            printf("Accepted streams: %d\n", accepted_streams.size());
        }

        /* If topology changed or a stream was removed:
           clear current schedule and reroute + reschedule established streams */
        if(topology_map.wasModified() || stream_mgmt.wasRemoved()) {
            if(ENABLE_SCHEDULE_COMP_INFO_DBG)
                printf("Topology changed or a stream was removed, Re-scheduling established streams\n");
            schedule.clear();
            // schedule_size must always be initialized to the number of tiles in superframe
            schedule_size = superframe.size();
            auto new_schedule = routeAndScheduleStreams(established_streams);
            schedule = std::move(new_schedule);
            changed = true;
        }
        else {
            if(ENABLE_SCHEDULE_COMP_INFO_DBG)
                printf("Topology and current streams did not change, Keep established schedule\n");
            /* No action is needed since the old schedule is kept unless
               it is explicitly removed with routed_streams.clear() */
        }
        /* If there are accepted streams:
           route + schedule them and add them to existing schedule */
        if(stream_snapshot.hasSchedulableStreams()) {
            if(ENABLE_SCHEDULE_COMP_INFO_DBG)
                printf("Scheduling accepted streams\n");
            //Sort accepted streams based on highest period first
            std::sort(accepted_streams.begin(), accepted_streams.end(),
                      [](StreamInfo a, StreamInfo b) {
                          return toInt(a.getPeriod()) > toInt(b.getPeriod());});
            //printf("Accepted streams after sorting:\n");
            //printStreams(accepted_streams);
            auto new_schedule = routeAndScheduleStreams(accepted_streams);
            schedule.insert(schedule.end(), new_schedule.begin(), new_schedule.end());
            changed = true;
            //Mark successfully scheduled streams as established
            for(auto& stream: new_schedule) {
                if(ENABLE_SCHEDULE_COMP_INFO_DBG)
                    printf("Setting stream %d to ESTABLISHED\n", stream.getKey());
                stream_snapshot.setStreamStatus(stream.getStreamId(), StreamStatus::ESTABLISHED);
                stream_mgmt.setStreamStatus(stream.getStreamId(), StreamStatus::ESTABLISHED);
            }
            // Mark streams in snapshot that are ACCEPTED but not scheduled as REJECTED
            for(auto& stream: stream_snapshot.getStreams()) {
                if(stream.getStatus() == StreamStatus::ACCEPTED) {
                    // setStreamStatus handles notifying the constructor
                    stream_snapshot.setStreamStatus(stream.getStreamId(), StreamStatus::REJECTED);
                    stream_mgmt.setStreamStatus(stream.getStreamId(), StreamStatus::REJECTED);
                    // Enqueue NACK_CONNECT to inform Stream in other nodes
                    std::vector<InfoElement> infos;
                    infos.push_back(InfoElement(stream.getStreamId(), InfoType::NACK_CONNECT));
                    stream_mgmt.enqueueInfo(infos);
                }
            }
        }
        if(changed == true) {
            // NOTE: schedule with ScheduleID=0 are not sent in MasterScheduleDistribution
            if(scheduleID == std::numeric_limits<unsigned long>::max())
                scheduleID = 1;
            else
                scheduleID++;
        }

        if(ENABLE_SCHEDULE_COMP_INFO_DBG) {
            printf("## Results ##\n");
            printf("Final schedule, ID:%lu\n", scheduleID);
            printSchedule();
        }
        if(ENABLE_STREAM_LIST_INFO_DBG){
            printf("[SC] Stream list after scheduling:\n");
            StreamCollection streams = stream_mgmt.getSnapshot();
            printStreams(streams.getStreams());
        }

        // To avoid caching of stdout
        fflush(stdout);

        // Clear modified bit to detect changes to topology or streams
        topology_ctx.clearModifiedFlag();
        stream_mgmt.clearFlags();
    }
}

std::vector<ScheduleElement> ScheduleComputation::routeAndScheduleStreams(
                                                  const std::vector<StreamInfo>& stream_list) {
    if(!stream_list.empty()) {
        Router router(*this, 1);
        if(ENABLE_SCHEDULE_COMP_INFO_DBG)
            printf("## Routing ##\n");
        // Run router to route multi-hop streams and get multiple paths
        auto routed_streams = router.run(stream_list);
        if(ENABLE_SCHEDULE_COMP_INFO_DBG) {
            printf("Stream list after routing:\n");
            printStreamList(routed_streams);
        }

        // Schedule expanded streams, avoiding conflicts
        if(ENABLE_SCHEDULE_COMP_INFO_DBG)
            printf("## Scheduling ##\n");
        auto new_schedule = scheduleStreams(routed_streams);
        return new_schedule;
    }
    else {
        if(ENABLE_SCHEDULE_COMP_INFO_DBG)
            printf("Stream list empty, scheduling done.\n");
        std::vector<ScheduleElement> empty;
        return empty;
    }

}

void ScheduleComputation::receiveSMEs(const std::vector<StreamManagementElement>& smes) {
#ifdef _MIOSIX
    miosix::Lock<miosix::Mutex> lck(sched_mutex);
#else
    std::unique_lock<std::mutex> lck(sched_mutex);
#endif
    std::vector<InfoElement> infos;
    // Iterate over received SMEs
    for(auto& sme: smes) {
        StreamStatus status = sme.getStatus();
        StreamId id = sme.getStreamId();
        // StreamId used to match LISTEN Streams StreamId(dst, dst, 0, dstPort)
        StreamId listenId(id.dst, id.dst, 0, id.dstPort);
        // Handle each SME
        switch(status){
            // If status == CONNECT, check for a corresponding LISTEN
        case StreamStatus::CONNECT:
            // Search for corresponding LISTEN StreamId
            printf("[SC] Received CONNECT, checking for LISTEN with (%d,%d,%d,%d)\n",
                   listenId.src, listenId.dst, listenId.srcPort, listenId.dstPort);
            if(stream_mgmt.getStreamStatus(listenId) == StreamStatus::LISTEN) {
                // Mark stream as accepted
                printf("[SC] LISTEN found, stream ACCEPTED\n");
                // Add stream because we just received the SME
                // TODO: add parameter negotiation between Client and Server
                stream_mgmt.addStream(StreamInfo(sme, StreamStatus::ACCEPTED));
            }
            else{
                printf("[SC] LISTEN not found, stream REJECTED\n");
                // Enqueue NACK_CONNECT
                infos.push_back(InfoElement(id, InfoType::NACK_CONNECT));
            }
            break;
            // If status == LISTEN, register it in StreamManager
        case StreamStatus::LISTEN:
            // Mark stream as LISTEN
            stream_mgmt.setStreamStatus(id, StreamStatus::LISTEN);
            // Enqueue ACK_LISTEN
            infos.push_back(InfoElement(id, InfoType::ACK_LISTEN));
            break;
            // If status == CLOSE, close the corresponding connection
        case StreamStatus::CLOSED:
            // Mark stream as CLOSED
            stream_mgmt.setStreamStatus(id, StreamStatus::CLOSED);
            break;
        }
    }
    // To avoid caching of stdout
    fflush(stdout);
    // Enqueue vector of InfoElement
    stream_mgmt.enqueueInfo(infos);
}

void ScheduleComputation::open(const StreamInfo& stream) {
#ifdef _MIOSIX
    miosix::Lock<miosix::Mutex> lck(sched_mutex);
#else
    std::unique_lock<std::mutex> lck(sched_mutex);
#endif
    stream_mgmt.addStream(stream);
}

std::vector<ScheduleElement> ScheduleComputation::scheduleStreams(
                                                  const std::list<std::list<ScheduleElement>>& routed_streams) {

    unsigned tile_size = mac_ctx.getSlotsInTileCount();
    unsigned dataslots_downlinktile = mac_ctx.getDataSlotsInDownlinkTileCount();
    unsigned dataslots_uplinktile = mac_ctx.getDataSlotsInUplinkTileCount();
    unsigned downlink_size = tile_size - dataslots_downlinktile;
    unsigned uplink_size = tile_size - dataslots_uplinktile;

    if(ENABLE_SCHEDULE_COMP_INFO_DBG)
        printf("Network configuration:\n- tile_size: %d\n- downlink_size: %d\n- uplink_size: %d\n",
           tile_size, downlink_size, uplink_size);

    // Start with an empty schedule
    // If scheduling is successful, this vector will be moved to replace the "schedule" field
    std::vector<ScheduleElement> scheduled_transmissions;
    for(auto& stream : routed_streams) {
        int block_size = 0;
        bool stream_err = false;
        // Counter to last slot offset: ensures sequentiality
        unsigned last_offset = 0;
        // Save schedule size to rollback in case the stream scheduling fails
        last_schedule_size = schedule_size;
        for(auto& transmission : stream) {
            unsigned char tx = transmission.getTx();
            unsigned char rx = transmission.getRx();
            if(ENABLE_SCHEDULE_COMP_INFO_DBG)
                printf("Scheduling transmission %d,%d\n", tx, rx);
            // Connectivity check
            if(!topology_map.hasEdge(tx, rx)) {
                stream_err = true;
                if(ENABLE_SCHEDULE_COMP_INFO_DBG)
                    printf("%d,%d are not connected in topology, cannot schedule stream\n", tx, rx);
            }
            // If a transmission cannot be scheduled, undo the whole stream
            if(stream_err) {
                if(ENABLE_SCHEDULE_COMP_INFO_DBG)
                    printf("Transmission scheduling failed, removing rest of the stream\n");
                for(int i=0; i<block_size; i++) {
                    scheduled_transmissions.pop_back();
                }
                // Rollback schedule size to before the failed stream
                schedule_size = last_schedule_size;
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
                if(ENABLE_SCHEDULE_COMP_INFO_DBG)
                    printf("Checking offset %d\n", offset);
                // Cycle over already scheduled elements to find conflicts
                if(ENABLE_SCHEDULE_COMP_INFO_DBG)
                    printf("Checking against old streams\n");
                bool conflict = checkAllConflicts(schedule, transmission, offset, tile_size);
                // Cycle over elements to be scheduled to find conflicts
                if(ENABLE_SCHEDULE_COMP_INFO_DBG)
                    printf("Checking against new streams\n");
                conflict |= checkAllConflicts(scheduled_transmissions, transmission, offset, tile_size);

                if(!conflict) {
                    last_offset = offset;
                    block_size++;
                    // Calculate new schedule size
                    unsigned period = toInt(transmission.getPeriod());
                    if(ENABLE_SCHEDULE_COMP_INFO_DBG)
                        printf("Schedule size, before:%d ", schedule_size);
                    schedule_size = lcm(schedule_size, period);
                    if(ENABLE_SCHEDULE_COMP_INFO_DBG)
                        printf("after:%d \n", schedule_size);
                    // Add transmission to schedule, and set schedule offset
                    scheduled_transmissions.push_back(transmission);
                    scheduled_transmissions.back().setOffset(offset);
                    if(ENABLE_SCHEDULE_COMP_INFO_DBG)
                        printf("Scheduled transmission %d,%d with offset %d\n", tx, rx, offset);
                    // Successfully scheduled transmission, break timeslot cycle
                    break;
                    }
                else {
                    if(ENABLE_SCHEDULE_COMP_INFO_DBG)
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
    if(ENABLE_SCHEDULE_COMP_INFO_DBG)
        printf("Final schedule length: %d\n", schedule_size);
    return scheduled_transmissions;
}

bool ScheduleComputation::checkAllConflicts(std::vector<ScheduleElement> other_streams, const ScheduleElement& transmission, unsigned offset, unsigned tile_size) {
    bool conflict = false;
    for(auto& elem : other_streams) {
        // conflictPossible is a simple condition used to reduce number of conflict checks
        if(slotConflictPossible(transmission, elem, offset, tile_size)) {
            if(ENABLE_SCHEDULE_COMP_INFO_DBG)
                printf("Conflict possible with %d->%d\n", elem.getTx(), elem.getRx());
            if(checkSlotConflict(transmission, elem, offset, tile_size)) {
                if(ENABLE_SCHEDULE_COMP_INFO_DBG)
                    printf("%d->%d and %d-%d have timeslots in common\n", transmission.getTx(),
                           transmission.getRx(), elem.getTx(), elem.getRx());
                /* Conflict checks */
                // Unicity check: no activity for src or dst node in a given timeslot
                if(checkUnicityConflict(transmission, elem)) {
                    conflict |= true;
                    if(ENABLE_SCHEDULE_COMP_INFO_DBG)
                        printf("Unicity conflict!\n");
                }
                // Interference check: no TX and RX for nodes at 1-hop distance in the same timeslot
                if(checkInterferenceConflict(transmission, elem)) {
                    conflict |= true;
                    if(ENABLE_SCHEDULE_COMP_INFO_DBG)
                        printf("Interference conflict!\n");
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
bool ScheduleComputation::checkDataSlot(unsigned offset, unsigned tile_size,
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
    conflict |= topology_map.hasEdge(tx_a, rx_b);
    conflict |= topology_map.hasEdge(rx_a, tx_b);
    return conflict;
}

void ScheduleComputation::printSchedule() {
    printf("ID  TX  RX  PER OFF\n");
    for(auto& elem : schedule) {
        printf("%d   %d-->%d   %d   %d\n", elem.getKey(), elem.getTx(), elem.getRx(),
                                        toInt(elem.getPeriod()), elem.getOffset());
    }
}

void ScheduleComputation::printStreams(const std::vector<StreamInfo>& stream_list) {
    printf("ID SRC DST  PER STS\n");
    for(auto& stream : stream_list) {
        printf("%d   %d-->%d   %2d  ", stream.getKey(), stream.getSrc(),
               stream.getDst(), toInt(stream.getPeriod()));
        switch(stream.getStatus()){
        case StreamStatus::CLOSED:
            printf("CLD");
            break;
        case StreamStatus::LISTEN_REQ:
            printf("LIR");
            break;
        case StreamStatus::LISTEN:
            printf("LIS");
            break;
        case StreamStatus::CONNECT_REQ:
            printf("COR");
            break;
        case StreamStatus::CONNECT:
            printf("CON");
            break;
       case StreamStatus::ACCEPTED:
            printf("ACC");
            break;
        case StreamStatus::ESTABLISHED:
            printf("EST");
            break;
        case StreamStatus::REJECTED:
            printf("REJ");
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

std::list<std::list<ScheduleElement>> Router::run(const std::vector<StreamInfo>& stream_list) {
    std::list<std::list<ScheduleElement>> routed_streams;
    if(ENABLE_SCHEDULE_COMP_INFO_DBG)
        printf("Routing %d stream requests\n", stream_list.size());
    // Cycle over stream_requests
    for(auto& stream: stream_list) {
        unsigned char src = stream.getSrc();
        unsigned char dst = stream.getDst();
        if(ENABLE_SCHEDULE_COMP_INFO_DBG)
            printf("Routing stream %d->%d\n", src, dst);

        // Check if 1-hop
        if(scheduler.topology_map.hasEdge(src, dst)) {
            // Add stream as is to final List
            if(ENABLE_SCHEDULE_COMP_INFO_DBG)
                printf("Stream %d->%d is single hop\n", src, dst);
            std::list<ScheduleElement> single_hop;
            single_hop.push_back(ScheduleElement(stream));
            routed_streams.push_back(single_hop);
            continue;
        }
        // Otherwise run BFS
        std::list<unsigned char> path = breadthFirstSearch(stream);
        // Calculate path lenght (for limiting DFS)
        unsigned int sol_size = path.size();
        if(path.empty()) {
            if(ENABLE_SCHEDULE_COMP_INFO_DBG)
                printf("No path found, stream not scheduled\n");
            continue;
        }
        // Print routed path
        if(ENABLE_SCHEDULE_COMP_INFO_DBG) {
            printf("Found path of length %d:\n", sol_size);
            printPath(path);
        }
        std::list<ScheduleElement> schedule = pathToSchedule(path, stream);
        // Insert routed path in place of multihop stream
        routed_streams.push_back(schedule);
        Redundancy redundancy = stream.getRedundancy();
        // Temporal redundancy
        if(redundancy == Redundancy::DOUBLE)
            routed_streams.push_back(schedule);
        if(redundancy == Redundancy::TRIPLE) {
            routed_streams.push_back(schedule);
            routed_streams.push_back(schedule);
        }
        // Spatial redundancy
        if(redundancy == Redundancy::DOUBLE_SPATIAL || redundancy == Redundancy::TRIPLE_SPATIAL) {
            // Runs depth first search to get a list of possible paths,
            // with maximum hops = first_solution + more_hops, among
            // which we choose the redundant path.
            if(ENABLE_SCHEDULE_COMP_INFO_DBG)
                printf("Searching alternative paths of length %d + %d\n", sol_size, more_hops);
            std::list<std::list<unsigned char>> extra_paths = depthFirstSearch(stream, sol_size + more_hops);
            // Choose best secondary path for redundancy
            if(!extra_paths.empty()) {
                // Print secondary paths found
                if(ENABLE_SCHEDULE_COMP_INFO_DBG) {
                    printf("Secondary Paths found: \n");
                    printPathList(extra_paths);
                }
                // Remove primary path from solutions
                extra_paths.remove(path);
                if(ENABLE_SCHEDULE_COMP_INFO_DBG) {
                    printf("Secondary Paths after removing primary path: \n");
                    printPathList(extra_paths);
                }
                // FInd paths without intermediate nodes in common with primary path
                std::list<std::list<unsigned char>> indip_paths = findIndependentPaths(extra_paths, path);
                // If the independent set is not empty choose shortest solution
                std::list<unsigned char> solution;
                if(indip_paths.size()) {
                    if(ENABLE_SCHEDULE_COMP_INFO_DBG) {
                        printf("Indipendent Paths found: \n");
                        printPathList(indip_paths);
                    }
                    solution = findShortestPath(indip_paths);
                }
                else { // Else pick the shortest non independent solution
                    if(ENABLE_SCHEDULE_COMP_INFO_DBG)
                        printf("Indipendent Paths not found: \n");
                    solution = findShortestPath(extra_paths);
                }
                if(ENABLE_SCHEDULE_COMP_INFO_DBG) {
                    printf("The best redundand path is:\n");
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

std::list<unsigned char> Router::breadthFirstSearch(StreamInfo stream) {
    unsigned char root = stream.getSrc();
    unsigned char dest = stream.getDst();
    // Check that the source node exists in the graph
    if(!scheduler.topology_map.hasNode(root)) {
        if(ENABLE_SCHEDULE_COMP_INFO_DBG)
            printf("Error: source node is not present in TopologyMap\n");
        return std::list<unsigned char>();
    }
    // Check that the destination node exists in the graph
    if(!scheduler.topology_map.hasNode(dest)) {
        if(ENABLE_SCHEDULE_COMP_INFO_DBG)
            printf("Error: destination node is not present in TopologyMap\n");
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
        std::vector<unsigned char> adjacence = scheduler.topology_map.getEdges(subtree_root);
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
    if(ENABLE_SCHEDULE_COMP_INFO_DBG)
        printf("Error: source and destination node are not connected in TopologyMap\n");
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
                                                  const StreamInfo& stream) {
    /* Es: path: 0 1 2 3 schedule: 0->1 1->2 2->3 */
    std::list<ScheduleElement> result;
    if(!path.size())
        return result;
    // Cache data from original multi-hop stream
    unsigned char src = stream.getSrc();
    unsigned char dst = stream.getDst();
    unsigned char srcPort = stream.getSrcPort();
    unsigned char dstPort = stream.getDstPort();
    Period period = stream.getPeriod();
    unsigned char tx = path.front();
    for(auto& rx : path) {
        // Skip first round of for because we are ciclyng with rx node
        if(rx == tx) continue;
        result.push_back(ScheduleElement(src, dst, srcPort, dstPort, tx, rx, period));
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
    printf("\n");
}

std::list<std::list<unsigned char>> Router::depthFirstSearch(StreamInfo stream,
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
        if(ENABLE_SCHEDULE_COMP_INFO_DBG)
            printf("Found redundand path of length %d, ", path.size());
        all_paths.push_back(path);
        if(ENABLE_SCHEDULE_COMP_INFO_DBG)
            printf("total paths found: %d\n", all_paths.size());
    }
    else{ // If current node != target
        // Recur for all the vertices adjacent to current vertex
        std::vector<unsigned char> adjacence = scheduler.topology_map.getEdges(start);
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
    std::list<unsigned char> result;
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
    if(ENABLE_SCHEDULE_COMP_INFO_DBG) {
        printf("Nodes to avoid:\n");
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
