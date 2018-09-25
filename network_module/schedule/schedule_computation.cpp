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
#include "../debug_settings.h"
#include "schedule_computation.h"
#include "../uplink/stream_management/stream_management_context.h"
#include "../uplink/stream_management/stream_management_element.h"
#include "../uplink/topology/topology_context.h"
#include "../mac_context.h"

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
                print_dbg("No stream to schedule, waiting...\n");
                // Condition variable to wait for streams to schedule.
                sched_cv.wait(lck);
            }
            // IF stream list not changed AND topology not changed
            // Skip to next for loop cycle
            //NOTE: debug
            topology_ctx.print();
            if(!(topology_ctx.getTopologyMap().wasModified() || stream_mgmt.wasModified())) {
                print_dbg("Stream list or topology did not change, sleeping");
                continue;
            }
            // ELSE we can begin scheduling 
            // Take snapshot of stream requests and network topology
            stream_snapshot = stream_mgmt;
            topology_map = topology_ctx.getTopologyMap();
            topology_ctx.print();
        }
        // From now on use only the snapshot class `stream_snapshot`
        // Get number of dataslots in current controlsuperframe (to avoid re-computating it)
        int data_slots = mac_ctx.getDataSlotsInControlSuperframeCount();
        print_dbg("Begin scheduling for %d dataslots\n", data_slots);
        int num_streams = stream_snapshot.getStreamNumber();

        // Run router to route multi-hop streams and get multiple paths
        Router router(*this, 1, 2);
        router.run();

        // Add scheduler code

        // Clear modified bit to detect changes to topology or streams
        topology_ctx.clearModifiedFlag();
        stream_mgmt.clearModifiedFlag();
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


void Router::run() {
    int num_streams = scheduler.stream_mgmt.getStreamNumber();
    print_dbg("Routing %d stream requests\n", num_streams);
    // Cycle over stream_requests
    for(int i=0; i<num_streams; i++) {
        StreamManagementElement stream = scheduler.stream_snapshot.getStream(i);
        unsigned char src = stream.getSrc();
        unsigned char dst = stream.getDst();
        print_dbg("Routing stream n.%d: %d to %d\n", i, src, dst);

        // Check if 1-hop
        if(scheduler.topology_map.hasEdge(src, dst)) {
            // Add stream as is to final List
            print_dbg("Stream n.%d: %d to %d is single hop\n", i, src, dst);
            routed_streams.push_back(stream);
            continue;
        }
        // Otherwise run BFS
        // TODO Uncomment after implementing nested list
        std::list<StreamManagementElement> path;
        path = breadthFirstSearch(stream);
        // Print routed path
        for(auto s : path) {
            print_dbg("%d -> %d\n", s.getSrc(), s.getDst());
        }
        // Insert routed path in place of multihop stream
        // routed_streams.push_back(path);
        // If redundancy, run DFS
        if(multipath) {
            //int sol_size = path.size();

        }
    }
}


std::list<StreamManagementElement> Router::breadthFirstSearch(StreamManagementElement stream) {
    unsigned char root = stream.getSrc();
    unsigned char dest = stream.getDst();
    // Check that the source node exists in the graph
    if(!scheduler.topology_map.hasNode(root)) {
        print_dbg("Error: source node is not present in TopologyMap\n");
        return std::list<StreamManagementElement>();
    }
    // Check that the destination node exists in the graph
    if(!scheduler.topology_map.hasNode(dest)) {
        print_dbg("Error: destination node is not present in TopologyMap\n");
        return std::list<StreamManagementElement>();
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
        if (subtree_root == dest) return construct_path(subtree_root, parent_of);
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
    print_dbg("Error: source and destination node are not connected in TopologyMap\n");
    return std::list<StreamManagementElement>();
}

std::list<StreamManagementElement> Router::construct_path(unsigned char node, std::map<const unsigned char, unsigned char> parent_of) {
    /* Construct path by following the parent-of relation to the root node */
    std::list<StreamManagementElement> path;
    unsigned char dst = node;
    unsigned char src = parent_of[node];
    // TODO: figure out how to pass other parameters (period...)
    path.push_back(StreamManagementElement(src, dst, 0, 0, Period::P1, 0));
    /* The root node is the only to have itself as predecessor */
    while(parent_of[dst] != dst) {
        dst = src;
        src = parent_of[dst];
        path.push_back(StreamManagementElement(src, dst, 0, 0, Period::P1, 0));
    }
    path.reverse();
    return path;
}
}
