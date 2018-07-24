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

#include "../debug_settings.h"
#include "schedule_computation.h"
#include "../uplink/stream_management/stream_management_context.h"
#include "../uplink/stream_management/stream_management_element.h"
#include "../uplink/topology/topology_context.h"
#include "../mac_context.h"
#include <miosix.h>

namespace mxnet {

ScheduleComputation::ScheduleComputation(MACContext& mac_ctx, MasterTopologyContext& topology_ctx,
                                         MasterStreamManagementContext& stream_ctx)
                            : topology_ctx(topology_ctx), stream_ctx(stream_ctx), mac_ctx(mac_ctx) {}

void ScheduleComputation::startThread() {
    if (scthread == NULL)
#ifdef _MIOSIX
        // Thread launched using static function threadLauncher with the class instance as parameter.
        scthread = miosix::Thread::create(&ScheduleComputation::threadLauncher, 2048, miosix::PRIORITY_MAX-2, this, miosix::Thread::JOINABLE);
#else
        scthread = new std::thread(&ScheduleComputation::run, this);
#endif
}

void ScheduleComputation::wakeupThread() {
    if (scthread == NULL)
#ifdef _MIOSIX
        // Thread launched using static function threadLauncher with the class instance as parameter.
        scthread->wakeup();
#else
        scthread = new std::thread(&ScheduleComputation::run, this);
#endif
}

void ScheduleComputation::run() {
    // Get number of dataslots in current controlsuperframe (to avoid re-computating it)
    int data_slots = mac_ctx.getDataSlotsInControlSuperframeCount();
    print_dbg("Begin scheduling for %d dataslots\n", data_slots);
    int num_streams = stream_ctx.getStreamNumber();
    if(num_streams == 0) {
        print_dbg("No stream to schedule, waiting...\n");
        scthread->wait();
    }
    Router router(topology_ctx, stream_ctx, 1, 2);
    // Run router to route multi-hop streams and get multiple paths
    router.run();
    
    //Add scheduler code
    
}

void Router::run() {
    int num_streams = stream_ctx.getStreamNumber();
    print_dbg("Routing %d stream requests\n", num_streams);
    //Cycle over stream_requests
    for(int i=0; i<num_streams; i++) {
        StreamManagementElement* stream = stream_ctx.getStream(i);
        unsigned char src = stream->getSrc();
        unsigned char dst = stream->getDst();
        print_dbg("Routing stream n.%d: %c to %c\n", i, src, dst);
        
        //Check if 1-hop
        if(topology_ctx.areSuccessors(src, dst)) {
            //Add stream as is to final List
            //routed_streams.push_back(stream);
            continue;
        }
        //Otherwise run BFS
        //TODO Uncomment after implementing nested list
        //std::list<StreamManagementElement*> path = breadthFirstSearch(stream);
        //routed_streams.push_back(path);
        //If redundancy, run DFS
        if(multipath) {
            //int sol_size = path.size();
            
        }    
    }
}

void Router::breadthFirstSearch(StreamManagementElement& stream) {
    
/*    // V = number of nodes in the network
    V = 
    // Mark all the vertices as not visited
    bool *visited = new bool[V];
    for(int i = 0; i < V; i++)
        visited[i] = false;
 
    // Create a queue for BFS
    list<int> queue;
 
    // Mark the current node as visited and enqueue it
    visited[s] = true;
    queue.push_back(s);
 
    // 'i' will be used to get all adjacent
    // vertices of a vertex
    list<int>::iterator i;
 
    while(!queue.empty())
    {
        // Dequeue a vertex from queue and print it
        s = queue.front();
        cout << s << " ";
        queue.pop_front();
 
        // Get all adjacent vertices of the dequeued
        // vertex s. If a adjacent has not been visited, 
        // then mark it visited and enqueue it
        for (i = adj[s].begin(); i != adj[s].end(); ++i)
        {
            if (!visited[*i])
            {
                visited[*i] = true;
                queue.push_back(*i);
            }
        }
    }*/
}
}
