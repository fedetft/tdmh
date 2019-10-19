
#include "scheduler/schedule_computation.h"

int main()
{
    const NetworkConfiguration config(
        maxHops,       //maxHops
        maxNodes,      //maxNodes
        0,             //networkId
        false,         //staticHop
        6,             //panId
        5,             //txPower
        2450,          //baseFrequency
        10000000000,   //clockSyncPeriod
        maxForwardedTopologiesFromMaxNumNodes(maxNodes), //maxForwardedTopologies
        1,             //numUplinkPackets
        100000000,     //tileDuration
        150000,        //maxAdmittedRcvWindow
        3,             //maxRoundsUnavailableBecomesDead
        -75,           //minNeighborRSSI
        4,             //maxMissedTimesyncs
        false          //channelSpatialReuse
    );
    
    
    ScheduleComputation scheduler(
        config,
        16, //slotsPerTile
        10, //dataslotsPerDownlinkTile
        15, //dataslotsPerUplinkTile
    );
    
    
}
