#!/usr/bin/env python3

# Author: Federico Amedeo Izzo, federico.izzo42@gmail.com

### GREEDY SCHEDULING ALGORITHM

## INPUTS:
# - Topology map: graph as list of edges
#    - edge: tuple of (nodeID, nodeID)
# Diamond topology example
# TODO: should we state links bidirectionally? yes for searching
topology_1 = [(0, 1), (0, 2), 
              (1, 0), (1, 2), (1, 3),
              (2, 0), (2, 1), (2, 3), 
              (3, 1), (3, 2)]

# - List of required streams (broken down in 1-hop transmissions)
#    - transmission: (source, destination) hop not listed (=1)
# example 1, concurrent
stream_list_1 = [(0, 1), 
                 (3, 2)]         
# example 2, TX-RX conflict          
stream_list_2 = [(0, 1), 
                 (2, 3)]
             
# - Number of data slots per slotframe
data_slots_1 = 10

## OUTPUTS
# - Schedule: list of ScheduleElement:
#   - ScheduleElement: tuple of (timeslot, node, activity, [destination node])
# schedule = []

def check_unicity_conflict(schedule, timeslot, stream):
    # Unicity check: no TX and RX for same node,timeslot
    src, dst = stream;
    conflict_set = [e for e in schedule if ((e[0] == timeslot) and (e[1] in {src, dst}))]
    if conflict_set:
        print('Conflict Detected! src or dst node are busy on timeslot ' + repr(timeslot))
        return True;
    else:
        return False

def check_interference_conflict(schedule, topology, timeslot, node, activity):
    # Interference check: no TX and RX for nodes at 1-hop distance in the same timeslot
    conflict = False;
    one_hop = [edge[1] for edge in topology if edge[0] == node]
    for n in one_hop:
        for elem in schedule:
            #print(repr(node)+repr(elem))
            if elem == (timeslot, n, activity):
                conflict = True;
                print('Conflict Detected! TX-RX conflict between node ' + repr(node) + ' and node ' + repr(n))
    return conflict;

# option A iteration
# TODO sort streams according to chosen metric
def scheduler(topology, stream_list, data_slots):
    for stream in stream_list:
        for timeslot in range(data_slots):
            print('Checking stream ' + repr(stream) + ' on timeslot ' + repr(timeslot))
            
            # Stream tuple unpacking
            src, dst = stream;
            
            err_conflict = False;
            err_unreachable = False;
            
            ## Connectivity check: edge between src and dst nodes
            if (src,dst) not in topology:
                err_unreachable = True;
                print('Nodes are not reachable in topology, cannot schedule stream ' + repr(stream))
                break;    #Cannot schedule transmission
                
            ## Conflict checks
            # Unicity check: no TX and RX for same node,timeslot
            err_conflict |= check_unicity_conflict(schedule, timeslot, stream)        
            # Interference check: no TX and RX for nodes at 1-hop distance in the same timeslot
            # Check TX node for RX neighbors
            err_conflict |= check_interference_conflict(schedule, topology, timeslot, src, 'RX')            
            # Check RX node for TX neighbors
            err_conflict |= check_interference_conflict(schedule, topology, timeslot, dst, 'TX')
                                            
            # Checks evaluation
            if err_conflict:
                print('Cannot schedule stream ' + repr(stream) + ' on timeslot ' + repr(timeslot))
                continue;  #Try to schedule in next timeslot
            else:
                # Adding stream to schedule
                schedule.append((timeslot, src, 'TX'));
                schedule.append((timeslot, dst, 'RX'));
                print('Scheduled stream ' + repr(stream) + ' on timeslot ' + repr(timeslot))
                break;     #Successfully scheduled transmission, break timeslot cycle

    ### Print resulting Schedule
    print('\nResulting schedule')
    print('Time, Node, Activity')
    for x in schedule:
    print(' {}     {}      {}'.format(x[0], x[1], x[2]))
    
    return schedule


if __name__ == '__main__':
    scheduler(topology_1, stream_list_1, data_slots_1)
    #scheduler(topology_1, stream_list_2, data_slots_1)
