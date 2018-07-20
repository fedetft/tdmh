/***************************************************************************
 *   Copyright (C)  2018 by Polidori Paolo                                 *
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
#include "master_schedule_context.h"
#include "../uplink/stream_management/stream_management_context.h"
#include "../uplink/stream_management/stream_management_element.h"
#include "../uplink/topology/topology_context.h"
#include "../mac_context.h"
#include <algorithm>

using namespace std;
using namespace miosix;

namespace mxnet {

void Scheduler::run(long long slotStart) {
    // Get number of dataslots in current controlsuperframe (to avoid re-computating it)
    int data_slots = mac_ctx.getDataSlotsInControlSuperframeCount();
    
    Router router(topology_ctx, stream_ctx, 1, 2);
    // Run router to route multi-hop streams and get multiple paths
    stream_list = router.run();
    
    //Add scheduler code
    
}

std::vector<StreamManagementElement*> Router::run() {
    // Expanded stream request after routing
    std::list<StreamManagementElement*> routed_streams;
    
    //Cycle over stream_requests
    for(int i=0; i<stream_ctx.getStreamNumber(); i++) {
        StreamManagementElement* stream = stream_ctx.getStream(i);
        unsigned char src = stream->getSrc();
        unsigned char dst = stream->getDst();
        
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

std::vector<StreamManagementElement*> Router::breadthFirstSearch(StreamManagementElement& stream) {
    
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

void MasterScheduleDownlinkPhase::execute(long long slotStart) {
    auto data = getScheduleToDistribute(MediumAccessController::maxPktSize - 1);
    auto bitSize = numeric_limits<unsigned char>::digits;
    packet[0] = data.first.size();
    for (auto* d : data.first) {
        std::vector<unsigned char> v(packet.begin(), packet.end());
        d->serialize(v, bitSize);
        bitSize += d->bitSize();
    }
    auto lsbShift = bitSize % numeric_limits<unsigned char>::digits;
    auto msbShift = numeric_limits<unsigned char>::digits - lsbShift;
    auto idx = bitSize / numeric_limits<unsigned char>::digits;
    if (lsbShift == 0)
        for(auto d : data.second) {
            packet[idx++] = d;
        }
    else {
        packet[idx] &= ~0 << msbShift;
        for(auto d : data.second) {
            packet[idx] |= d >> lsbShift;
            packet[++idx] = d << msbShift;
        }
    }
    ctx.configureTransceiver(ctx.getTransceiverConfig());
    //Thread::nanoSleepUntil(startTime);
    auto wakeUp = slotStart - MediumAccessController::sendingNodeWakeupAdvance;
    if(getTime() < wakeUp)
        ctx.sleepUntil(wakeUp);
    ctx.sendAt(packet.data(), (bitSize - 1) / numeric_limits<unsigned char>::digits + 1, slotStart);
    if (ENABLE_SCHEDULE_DL_INFO_DBG)
        print_dbg("[SC] ST=%lld\n", slotStart);
    ctx.transceiverIdle();
    for (auto el : data.first)
        delete el;
    //TODO add management of nodeSchedule, containing the current schedule to execute in the data phase
}

std::pair<std::vector<ScheduleAddition*>, std::vector<unsigned char>> MasterScheduleDownlinkPhase::getScheduleToDistribute(unsigned short bytes) {
    static int i = 0;
    if(i++ == 15) {
        //vector<std::tuple<unsigned short, unsigned short, unsigned short, unsigned short>>* vec;

        //pass from master
        //                                    TS                              SCHEDNUM                SRC                 DST
        /*schedule.insert(std::make_pair((unsigned short)0, std::make_tuple((unsigned short)0, (unsigned char)2, (unsigned char)0)));
        schedule.insert(std::make_pair((unsigned short)1, std::make_tuple((unsigned short)0, (unsigned char)0, (unsigned char)1)));
        schedule.insert(std::make_pair((unsigned short)2, std::make_tuple((unsigned short)0, (unsigned char)1, (unsigned char)3)));
        //                                          DELNUM           SCHEDNUM                TS                SRC                 DST
        vec = new vector<std::tuple<unsigned short, unsigned short, unsigned short, unsigned short>> {
                                              std::make_tuple((unsigned short)0, (unsigned short)0, (unsigned char)2, (unsigned char)0),
                                              std::make_tuple((unsigned short)0, (unsigned short)1, (unsigned char)0, (unsigned char)1),
                                              std::make_tuple((unsigned short)0, (unsigned short)2, (unsigned char)1, (unsigned char)3)
        };
        deltaToDistribute.push_back(std::make_pair((unsigned short) 0, vec));
        //forwarding queues
        queuesBySched[0] = new std::queue<std::vector<unsigned char>>();
        toForward[0] = queuesBySched[0];
        forwarded[1] = queuesBySched[0];*/

        //ping pong
        //                                    TS                              SCHEDNUM                SRC                 DST
        /*schedule.insert(std::make_pair((unsigned short)0, std::make_tuple((unsigned short)0, (unsigned char)0, (unsigned char)1)));
        schedule.insert(std::make_pair((unsigned short)1, std::make_tuple((unsigned short)0, (unsigned char)1, (unsigned char)3)));
        schedule.insert(std::make_pair((unsigned short)2, std::make_tuple((unsigned short)1, (unsigned char)3, (unsigned char)1)));
        schedule.insert(std::make_pair((unsigned short)3, std::make_tuple((unsigned short)1, (unsigned char)1, (unsigned char)0)));
        //                                          DELNUM           SCHEDNUM                TS                SRC                 DST
        vec = new vector<std::tuple<unsigned short, unsigned short, unsigned short, unsigned short>> {
                                              std::make_tuple((unsigned short)0, (unsigned short)0, (unsigned char)0, (unsigned char)1),
                                              std::make_tuple((unsigned short)0, (unsigned short)1, (unsigned char)1, (unsigned char)3)
        };
        deltaToDistribute.push_back(std::make_pair((unsigned short) 0, vec));
        vec = new vector<std::tuple<unsigned short, unsigned short, unsigned short, unsigned short>> {
                                              std::make_tuple((unsigned short)1, (unsigned short)2, (unsigned char)3, (unsigned char)1),
                                              std::make_tuple((unsigned short)1, (unsigned short)3, (unsigned char)1, (unsigned char)0)
        };
        deltaToDistribute.push_back(std::make_pair((unsigned short) 0, vec));*/

        //ping mirrored dt kite
        //                                    TS                              SCHEDNUM                SRC                 DST
        /*schedule.insert(std::make_pair((unsigned short)0, std::make_tuple((unsigned short)0, (unsigned char)0, (unsigned char)1)));
        schedule.insert(std::make_pair((unsigned short)1, std::make_tuple((unsigned short)0, (unsigned char)1, (unsigned char)2)));
        schedule.insert(std::make_pair((unsigned short)2, std::make_tuple((unsigned short)0, (unsigned char)2, (unsigned char)3)));
        schedule.insert(std::make_pair((unsigned short)4, std::make_tuple((unsigned short)0, (unsigned char)3, (unsigned char)5)));
        schedule.insert(std::make_pair((unsigned short)5, std::make_tuple((unsigned short)0, (unsigned char)5, (unsigned char)6)));
        schedule.insert(std::make_pair((unsigned short)6, std::make_tuple((unsigned short)0, (unsigned char)6, (unsigned char)7)));
        schedule.insert(std::make_pair((unsigned short)0, std::make_tuple((unsigned short)1, (unsigned char)7, (unsigned char)6)));
        schedule.insert(std::make_pair((unsigned short)1, std::make_tuple((unsigned short)1, (unsigned char)6, (unsigned char)5)));
        schedule.insert(std::make_pair((unsigned short)3, std::make_tuple((unsigned short)1, (unsigned char)5, (unsigned char)4)));
        schedule.insert(std::make_pair((unsigned short)4, std::make_tuple((unsigned short)1, (unsigned char)4, (unsigned char)2)));
        schedule.insert(std::make_pair((unsigned short)5, std::make_tuple((unsigned short)1, (unsigned char)2, (unsigned char)1)));
        schedule.insert(std::make_pair((unsigned short)6, std::make_tuple((unsigned short)1, (unsigned char)1, (unsigned char)0)));
        //                                          DELNUM           SCHEDNUM                TS                SRC                 DST
        vec = new vector<std::tuple<unsigned short, unsigned short, unsigned short, unsigned short>> {
                                              std::make_tuple((unsigned short)0, (unsigned short)0, (unsigned char)0, (unsigned char)1),
                                              std::make_tuple((unsigned short)0, (unsigned short)1, (unsigned char)1, (unsigned char)2),
                                              std::make_tuple((unsigned short)0, (unsigned short)2, (unsigned char)2, (unsigned char)3),
                                              std::make_tuple((unsigned short)0, (unsigned short)4, (unsigned char)3, (unsigned char)5),
                                              std::make_tuple((unsigned short)0, (unsigned short)5, (unsigned char)5, (unsigned char)6),
                                              std::make_tuple((unsigned short)0, (unsigned short)6, (unsigned char)6, (unsigned char)7)
        };
        deltaToDistribute.push_back(std::make_pair((unsigned short) 0, vec));
        vec = new vector<std::tuple<unsigned short, unsigned short, unsigned short, unsigned short>> {
                                              std::make_tuple((unsigned short)1, (unsigned short)0, (unsigned char)7, (unsigned char)6),
                                              std::make_tuple((unsigned short)1, (unsigned short)1, (unsigned char)6, (unsigned char)5),
                                              std::make_tuple((unsigned short)1, (unsigned short)3, (unsigned char)5, (unsigned char)4),
                                              std::make_tuple((unsigned short)1, (unsigned short)4, (unsigned char)4, (unsigned char)2),
                                              std::make_tuple((unsigned short)1, (unsigned short)5, (unsigned char)2, (unsigned char)1),
                                              std::make_tuple((unsigned short)1, (unsigned short)6, (unsigned char)1, (unsigned char)0)
        };
        deltaToDistribute.push_back(std::make_pair((unsigned short) 0, vec));*/
    }
    std::vector<ScheduleAddition*> retval1;
    std::vector<unsigned char> retval2;
    unsigned bitsLimit = bytes * std::numeric_limits<unsigned char>::digits;
    //insert until i encounter the first one exceeding
    for (bool exceeds = false; bitsLimit > 0 && !exceeds && !deltaToDistribute.empty();) {
        auto* val = deltaToDistribute.front();
        auto nextSize = val->bitSize();
        if (nextSize > bitsLimit)
            exceeds = true;
        else {
            deltaToDistribute.pop_front();
            if (val->getDeltaType() == ScheduleDeltaElement::ADDITION)
                retval1.push_back(static_cast<ScheduleAddition*>(val));
            else {
                retval2.push_back(val->getScheduleId());
                delete val;
            }
            bitsLimit -= nextSize;
        }
    }
    //then start browsing the queue for trying to fill all the available space greedily
    for (auto it = deltaToDistribute.begin(); it != deltaToDistribute.end() && bitsLimit >= std::numeric_limits<unsigned char>::digits;) {
        if ((*it)->bitSize() > bitsLimit) it++;
        else {
            if ((*it)->getDeltaType() == ScheduleDeltaElement::ADDITION)
                retval1.push_back(static_cast<ScheduleAddition*>(*it));
            else {
                retval2.push_back((*it)->getScheduleId());
                delete *it;
            }
            it = deltaToDistribute.erase(it);
        }
    }
    return std::make_pair(retval1, retval2);
}

}
