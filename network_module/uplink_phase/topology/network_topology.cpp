/***************************************************************************
 *   Copyright (C) 2019 by Federico Amedeo Izzo, Federico Terraneo         *
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

#include "network_topology.h"
#include "../../util/debug_settings.h"

namespace mxnet {

//
// class NetworkTopology
//

void NetworkTopology::handleTopologies(UpdatableQueue<unsigned char,
                                       TopologyElement>& topologies) {
    // Lock mutex to access NetworkGraph (shared with ScheduleComputation).
    graph_mutex.lock();

    int numTopologies = topologies.size();
    for(int i=0; i<numTopologies; i++) {
        doReceivedTopology(topologies.dequeue());
    }

    // Unlock mutex, we finished modifying the graph
    graph_mutex.unlock();
}

void NetworkTopology::usedLinksNotChanged()
{
#ifdef _MIOSIX
    miosix::Lock<miosix::Mutex> lck(graph_mutex);
#else
    std::unique_lock<std::mutex> lck(graph_mutex);
#endif
    performDelayedRemovalChecks();
}

void NetworkTopology::usedLinksChanged(std::set<std::pair<unsigned char,unsigned char>>&& usedLinks)
{
#ifdef _MIOSIX
    miosix::Lock<miosix::Mutex> lck(graph_mutex);
#else
    std::unique_lock<std::mutex> lck(graph_mutex);
#endif
    //First update usedLinks, then perform checks
    this->usedLinks=std::move(usedLinks);
    performDelayedRemovalChecks();
}

void NetworkTopology::performDelayedRemovalChecks()
{
    scheduleInProgress = false;
    for(auto removedLink : removedWhileScheduling)
    {
        if(usedLinks.find(removedLink)!=usedLinks.end())
        {
            modified_flag = true;
            break;
        }
    }
    removedWhileScheduling.clear();
}

void NetworkTopology::doReceivedTopology(const TopologyElement& topology) {
    // Mutex already locked by caller
    unsigned char src = topology.getId();
    RuntimeBitset bitset = topology.getNeighbors();
    
    if(ENABLE_TOPOLOGY_BITMASK_DBG)
    {
        std::string s;
        s.reserve(bitset.bitSize()+2);
        s+='[';
        for(unsigned int i=0;i<bitset.bitSize();i++) s+=bitset[i] ? '1' : '0';
        s+=']';
        print_dbg("[U] Topo %03d: %s\n",src,s.c_str());
    }

    /* Update graph according to received topology */
    for (unsigned i = 0; i < bitset.bitSize(); i++) {
        // Avoid auto-arcs (useless in NetworkGraph)
        if(i == src)
            continue;
        
        if(bitset[i]) {
            graph.addEdge(src, i);
        } else {
            bool removed = graph.removeEdge(src, i);
            
            if(removed) {
                if(scheduleInProgress == false)
                {
                    // We have an up-to-date usedLinks
                    if(usedLinks.find(orderLink(src, i))!=usedLinks.end())
                    {
                        modified_flag = true;
                    }
                } else {
                    // We don't have usedLinks, so we need to defer this check
                    removedWhileScheduling.insert(orderLink(src, i));
                }
            }
        }
    }
}

} /* namespace mxnet */
