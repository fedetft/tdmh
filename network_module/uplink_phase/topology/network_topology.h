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

#pragma once

#include "../../util/runtime_bitset.h"
#include "../../util/updatable_queue.h"
#include "../../network_configuration.h"
#include "topology_element.h"
#include "network_graph.h"
#ifdef _MIOSIX
#include <miosix.h>
#else
#include <mutex>
#endif

#include <set>
#include <vector>
#include <utility>


namespace mxnet {

inline std::pair<unsigned char,unsigned char> orderLink(unsigned char a, unsigned char b)
{
   if(a<b) return std::make_pair(a,b);
   else return std::make_pair(b,a);
}

/**
 * NetworkTopology contains all the information about the network graph
 * in the Master node.
 */
class NetworkTopology {
public:
    NetworkTopology(const NetworkConfiguration& config) :
        channelSpatialReuse(config.getChannelSpatialReuse()),
        useWeakTopologies(config.getUseWeakTopologies()),
        graph(config.getMaxNodes()),
        weakGraph(config.getMaxNodes()) {}

    void handleTopologies(UpdatableQueue<unsigned char, TopologyElement>& topologies);

    /**
     * Return true if the map was modified since last time the flag was cleared 
     */
    bool wasModified() {
        // Mutex lock to access schedule (shared with ScheduleDownlink).
#ifdef _MIOSIX
        miosix::Lock<miosix::Mutex> lck(graph_mutex);
#else
        std::unique_lock<std::mutex> lck(graph_mutex);
#endif
        return modified_flag;
    }

    /**
     * This function is called by ScheduleComputation at every scheduler round
     * If the graphs have changed from the last check, update the graph snapshots
     * of the scheduler, and set corresponding graph_changed flag
     */
    bool updateSchedulerNetworkGraph(GRAPH_TYPE& otherGraph, GRAPH_TYPE& otherWeakGraph) {
        // Mutex lock to access NetworkGraph (shared with ScheduleComputation).
#ifdef _MIOSIX
        miosix::Lock<miosix::Mutex> lck(graph_mutex);
#else
        std::unique_lock<std::mutex> lck(graph_mutex);
#endif
        scheduleInProgress = true;
        
        // Always copy, as some changes do not set modified_flag
        otherGraph = graph;
        if(useWeakTopologies)
            otherWeakGraph = weakGraph;
        
        bool result = modified_flag;
        modified_flag = false;
        return result;
    }

    /**
     * This function is called by ScheduleComputation after completing the algorithm
     * to eliminate nodes not reachable by the master from the graph snaphot.
     * The changes need to be written back on the main network graph, but this is
     * possible only if the graph hasn't changed in the meantime.
     * If the graph instead has changed, the possiblyNotConnected_flag
     * remains true since the removeUnreachableNodes() algorithm sets it only on the
     * graph snapshot and not on the main graph.
     * In this case we will run again the algorithm at the next scheduling,
     * and try again to write back the results.
     * This function returns true if the write back was successful
     */
    bool writeBackNetworkGraph(const GRAPH_TYPE& newGraph) {
        // Mutex lock to access NetworkGraph (shared with ScheduleComputation).
#ifdef _MIOSIX
        miosix::Lock<miosix::Mutex> lck(graph_mutex);
#else
        std::unique_lock<std::mutex> lck(graph_mutex);
#endif
        // If graph hasn't changed from the last copy,
        // copy back graph snapshot
        // NOTE: the snapshot contains possiblyNotConnected_flag set to false
        if(modified_flag == false) {
            graph = newGraph;
            return true;
        }
        return false;
    }
    
    void usedLinksNotChanged();
    
    void usedLinksChanged(std::set<std::pair<unsigned char,unsigned char>>&& usedLinks);

#ifdef UNITTEST
    /** Manually add an edge to the graph
     *  NOTE: to be used for debugging only */
    void addEdge(unsigned char a, unsigned char b) {
        graph.addEdge(a,b);
        // Set flag since we added an arc that was not present before
        modified_flag = true;
    }

    void weakAddEdge(unsigned char a, unsigned char b) {
        weakGraph.addEdge(a,b);
        // Set flag since we added an arc that was not present before
        modified_flag = true;
    }
#endif

private:
    
    void performDelayedRemovalChecks();

    /* Method used internally to add or remove arcs of the graph depending on
       the forwarded topology */
    void doReceivedTopology(const TopologyElement& topology);
    
    bool channelSpatialReuse;
    bool useWeakTopologies;

    /* NetworkGraph class containing the complete graph of the network */
    GRAPH_TYPE graph;
    /* NetworkGraph class containing the graph of weak links */
    GRAPH_TYPE weakGraph;

    /* Flag used by the scheduler to check if the topology has changed */
    bool modified_flag = false;
    
    bool scheduleInProgress = false;
    std::set<std::pair<unsigned char,unsigned char>> removedWhileScheduling;
    
    std::set<std::pair<unsigned char,unsigned char>> usedLinks;

    /* Mutex to synchronize the concurrent access to the network graph
       by the uplink and schedule_computation classes */
#ifdef _MIOSIX
    miosix::Mutex graph_mutex;
#else
    std::mutex graph_mutex;
#endif


};

} /* namespace mxnet */
