#!/usr/bin/env python3

# Author: Federico Amedeo Izzo, federico.izzo42@gmail.com

import argparse
import queue
from graphviz import Graph
from collections import defaultdict

### GLOBAL SETTINGS
# enable or disable spatial redundancy
# if disabled only temporal redundancy is used whether required.
#multipath = True
multipath = True
# max hop difference between first and redundant solution
more_hops = 2
bfs_debug = False
sch_debug = True
sch_raw_print = False

### GREEDY SCHEDULING ALGORITHM

## INPUTS:
# - Topology map: graph as list of edges without repeated arcs
# This is because every established link is bidirectional and the Network Graph
# is an unoriented graph
# ex: (1,2) and (2,1) is saved as (1,2)
#    - edge: tuple of (nodeID, nodeID)
# Diamond topology example
topology_1 = [(0, 1), (0, 2), 
              (1, 2), (1, 3),
              (2, 3)]
  
# RTSS Paper topology example
topology_2 = [(0, 1), (0, 3), (0, 5), (0, 7),
              (1, 3), (1, 5), (1, 7),
              (2, 4), (2, 6), (2, 7), (2, 8),
              (3, 5),
              (4, 5), (4, 6), (4, 7), (4, 8),
              (5, 7), (5, 8),
              (6, 8),
              (7, 8)]

# - List of required streams
# 1-hop streams: tuple
# N-hop streams: list of tuples
# Stream tuple: (source, destination, redundancy, period, num_pkt)
# example 1, TX-RX conflict (3,1)
req_streams_1 = [(0, 1),
                 (3, 2)]         
# example 2, TX-RX conflict (2,1)         
req_streams_2 = [(0, 1), 
                 (2, 3)]
# example 3, RTSS Paper streams
req_streams_3 = [(3, 0), 
                 (6, 0),
                 (4, 0)]
                 
# - Number of data slots per slotframe
data_slots_1 = 10

## OUTPUTS
# - Schedule: list of ScheduleElement:
#   - ScheduleElement: tuple of (timeslot, node, activity, [destination node])
# schedule = []

## Data structures to use
topology = topology_2
req_streams = req_streams_3
data_slots = data_slots_1

def adjacence(topology, node):
    return [edge[1] for edge in topology if edge[0] == node] \
    + [edge[0] for edge in topology if edge[1] == node]

def is_onehop(topology,stream):
    # Connectivity check: edge between src and dst nodes
    # Check if nodes are 1-hop distance in graph
    src, dst = stream;
    if (src,dst) in topology or (dst,src) in topology:
        return True;
    else:
        return False;

def check_unicity_conflict(schedule, timeslot, stream):
    # Unicity check: no activity for src or dst node on a given timeslot
    src, dst = stream;
    link = {src,dst}
    conflict_set = [e for e in schedule if ((e[0] == timeslot) and (e[1] in link))]
    if conflict_set:
        if sch_debug:
            print('Conflict Detected! src or dst node are busy on timeslot ' + repr(timeslot))
        return True;
    else:
        return False

def check_interference_conflict(schedule, topology, timeslot, node, activity):
    # Interference check: no TX and RX for nodes at 1-hop distance in the same timeslot
    # Checks if nodes at 1-hop distance of 'node' are doing 'activity'
    conflict = False;
    for n in adjacence(topology, node):
        for elem in schedule:
            #print(repr(node)+repr(elem))
            if elem == (timeslot, n, activity):
                conflict = True;
                if sch_debug:
                    print('Conflict Detected! TX-RX conflict between node ' + repr(node) + ' and node ' + repr(n))
    return conflict;

def breadth_first_search(topology, stream, avoid):
    # Breadth First Search for topology graph
    if bfs_debug:
        print('Starting Breadth First search')
    # Data structures
    open_set = queue.Queue()
    visited = set()
    parent_of = dict()  # key:node value:parent node
    src, dst = stream;
    
    root = src
    parent_of[root] = None
    open_set.put(root)

    while not open_set.empty():
        if bfs_debug:
            print('bfs open set: ' + repr(list(open_set.queue)))
            print('parent_of: ' + repr(parent_of))
        subtree_root = open_set.get()
        if subtree_root == dst:
            return construct_path(subtree_root, parent_of)

        for child in adjacence(topology, subtree_root):
            if child in visited:
                continue;
            if child not in list(open_set.queue) and child not in avoid:
                parent_of[child] = subtree_root
                open_set.put(child)
        visited.add(subtree_root)

def construct_path(node, parent_of):
    path = list()
    # Continue until you reach root that has parent = None
    # Append destination node to avoid losing it
    path.append(node)
    while parent_of[node] is not None:
        # Skip the destination node saved above
        node = parent_of[node]
        path.append(node)
    path.reverse()    
    return path
    
def dfs_paths(graph, start, target, limit, path=None):
    if path is None:        #Necessary for python only
        path = [start]      #
    if start == target:
        yield path    
    for next in set(adjacence(graph,start)) - set(path):
        if limit == 0:
            continue
        limit -= 1
        yield from dfs_paths(graph, next, target, limit, path + [next]) #https://legacy.python.org/dev/peps/pep-0380/

def path_to_stream_block(path):
    if path is not None:
        st_block=[]
        for x in range(0,len(path)-1):
            st_block.append((path[x],path[x+1]))
        return st_block
    else:
        return None

def get_shortest_path(path_list):
    best_path = path_list[0]
    size = len(path_list[0])
    for path in path_list:
        if len(path) < size:
            best_path = path
            size = len(path)
    return best_path

# option A iteration
# TODO sort streams according to chosen metric
def scheduler(topology, req_streams, data_slots):
    schedule = []
    schedule_human = [] #Just for printing purposes
    for stream_block in req_streams:
        # last_ts guarantees sequentiality in blocks 
        # and avoid conflicts between two consecutive 
        # streams in a block.
        last_ts = 0
        # If a stream in a block cannot be scheduled, 
        #undo the whole block
        err_block = False;
        num_streams_in_block = 0;
        for stream in stream_block:
            for timeslot in range(last_ts, data_slots):
                if sch_debug:
                    print('Checking stream ' + repr(stream) + ' on timeslot ' + repr(timeslot))
                
                # Stream tuple unpacking
                src, dst = stream;
                
                conflict = False;
                err_unreachable = False;
                
                ## Connectivity check: edge between src and dst nodes
                if not is_onehop(topology,stream):
                    err_block = True;
                    err_unreachable = True;
                    if sch_debug:
                        print('Nodes are not reachable in topology, cannot schedule stream ' + repr(stream_block))
                    break;    #Cannot schedule transmission
                    
                ## Conflict checks
                # Unicity check: no activity for src or dst node on a given timeslot
                conflict |= check_unicity_conflict(schedule, timeslot, stream)        
                # Interference check: no TX and RX for nodes at 1-hop distance in the same timeslot
                # Check TX node for RX neighbors
                conflict |= check_interference_conflict(schedule, topology, timeslot, src, 'RX')            
                # Check RX node for TX neighbors
                conflict |= check_interference_conflict(schedule, topology, timeslot, dst, 'TX')
                
                # Checks evaluation
                if conflict:
                    if sch_debug:
                        print('Cannot schedule stream ' + repr(stream) + ' on timeslot ' + repr(timeslot))
                    continue;  #Try to schedule in next timeslot
                else:
                    last_ts = timeslot
                    num_streams_in_block += 1
                    # Adding stream to schedule
                    schedule.append((timeslot, src, 'TX'));
                    schedule.append((timeslot, dst, 'RX'));
                    schedule_human.append((timeslot, src, dst));
                    if sch_debug:
                        print('Scheduled stream ' + repr(stream) + ' on timeslot ' + repr(timeslot))
                    break;     #Successfully scheduled transmission, break timeslot cycle
            ## Next stream in block starts from next timeslot (otherwise conflict is assured)
            last_ts += 1
            # Last-timeslot + conflict = unschedulable stream
            if timeslot == data_slots - 1:
                err_block = True;
            # If a stream in a block cannot be scheduled, 
            # undo the whole block,
            # then break block cycle
            if err_block:
                if sch_debug:
                    print('Block scheduling failed, removing streams of block ' + repr(stream_block))
                for i in range(num_streams_in_block):
                    schedule.pop();
                    schedule.pop();
                break;

    if sch_raw_print:
        ### Print Raw Schedule
        print('\nResulting schedule')
        print('Time, Node, Activity')
        for x in schedule:
            print(' {}     {}      {}'.format(x[0], x[1], x[2]))
    
    else:
        ### Print Pretty Schedule
        print('\nResulting schedule')
        print('Time   Src, Dst')
        for x in schedule_human:
            print(' {}:     {}    {}'.format(x[0], x[1], x[2]))
            
    return schedule;

# Algorithm that breaks down N-hop streams (N>1) in sequences of 1-hop streams,
# by finding shortest paths over the network graph. 
def router(topology, req_streams, multipath, more_hops):
    # Results should be put in separate data structure to not cycle over them
    final_streams = req_streams.copy()
    #Get the set of nodes from topology (list of tuples) by flattening the tuples 
    #and finding unique elements by turning the list into a set.
    nodes = set(sum(topology, ()))
    #print(repr(nodes))
    for stream in req_streams:
        # Stream tuple unpacking
        #print(repr(stream))
        src, dst = stream;

        ## If src and dst are 1-hop, no routing is needed
        ## single streams are packed into one-stream list for uniformity
        if is_onehop(topology,stream):
            pos = final_streams.index(stream)
            final_streams.remove(stream)
            final_streams.insert(pos, [stream])
            continue;

        #TODO remove unused avoid set in bfs 
        avoid = []
        ## Breadth First Search for topology graph
        first_path = breadth_first_search(topology, stream, avoid)
        print('Primary Path (BFS): ' + repr(first_path))
 
        ## Inserting first_path in final_streams in place of multihop stream
        stream_block = path_to_stream_block(first_path)
        print('Routing '+repr(stream)+' as '+repr(stream_block))
        pos = final_streams.index(stream)
        final_streams.remove(stream)
        final_streams.insert(pos, stream_block)

        # Multipath search enabled
        if multipath:
 
            ## DFS limited in depth
            sol_size = len(first_path)
            print('Primary path length: ' + repr(sol_size))
            print('Searching secondary path of max length: ' + repr(sol_size) + '+' + repr(more_hops) + '= ' + repr(sol_size + more_hops))
            path_list = dfs_paths(topology, src, dst, sol_size + more_hops)
            path_list = list(path_list) # Materializes results
            print('DFS solutions: ' + repr(path_list))
            
            ## Choosing best secondary path from DFS solutions
            # Check if another path exists
            path_list.remove(first_path)
            if not path_list: #Check if list is empty
                print('No extra path found: falling back to temporal redundancy')
            else:
                # Exists path without middle nodes in common with first_path
                middle_nodes = first_path[1:-1]
                indip_path = path_list.copy()
                print('Middle nodes ' + repr(middle_nodes))
                for path in path_list:
                    for node in middle_nodes:
                        if node in path and path in indip_path:
                            indip_path.remove(path)
                            continue
                if indip_path:
                    print('Found indipendent path')
                    second_path = get_shortest_path(indip_path)
                else:
                # Path with some nodes in common
                    print('Cannot find indipendent path')
                    second_path = get_shortest_path(path_list)
                
                ## SPATIAL REDUNDANCY IS ADDED HERE
                # Inserting secondary path
                print('Secondary Path (limited-DFS): ' + repr(second_path))
                stream_block_2 = path_to_stream_block(second_path)
                print('Routing ' + repr(stream)+' as '+repr(stream_block_2))
                if (pos + 1) > len(final_streams):
                    final_streams.append(stream_block_2)
                else:
                    final_streams.insert((pos + 1), stream_block_2)

    return final_streams;

def draw_graph(topology):
    # Create directed graph (Digraph)
    dot = Graph(format='pdf', comment='Network Topology', strict = True)

    # Flatten topology (list of tuples)
    nodes = list(sum(topology, ()))
    # Get unique nodes
    nodes = set(nodes)
    for x in nodes:
        dot.node(repr(x))
    
    ed = [''.join((repr(e[0]), repr(e[1]))) for e in topology]
    dot.edges(ed)
    dot.render('topology.gv', view=True)  # doctest: +SKIP

if __name__ == '__main__':
    parser = argparse.ArgumentParser()
    parser.add_argument("command", help="examples: plot, run")
    args = parser.parse_args()
    if (args.command == "plot"):
        draw_graph(topology_2)
    if (args.command == "run"):
        final_streams = router(topology, req_streams, multipath, more_hops)
        print('Final stream list: '+repr(final_streams))
        scheduler(topology, final_streams, data_slots)
    
