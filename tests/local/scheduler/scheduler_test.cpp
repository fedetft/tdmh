
#include <iostream>
#include <thread>
#include <chrono>
#include "scheduler/schedule_computation.h"

using namespace std;
using namespace std::chrono;
using namespace mxnet;

inline int maxForwardedTopologiesFromMaxNumNodes(int maxNumNodes)
{
    /*
     * Dynamic Uplink allocation implemented
     * the parameter topologySMERatio configures the fraction of uplink packet
     * reserved for topology messages
     * UplinkMessagePkt                 { hop, assignee }   2
     * NeighborTable                    { bitmask }         maxNumNodes/8
     * number of forwarded topologies (NeighborMessage::serialize) 1
     * vector<ForwardedNeighborMessage> { nodeId, bitmask } maxForwardedTopologies*(1+maxNumNodes/8)
     */
    const float topologySMERatio = 0.5;
    int packetCapacity = (125 - 2 - maxNumNodes/8 - 1) / (1 + maxNumNodes/8);
    return std::min<int>(packetCapacity * topologySMERatio, maxNumNodes - 2);
}

int main()
{
    const int maxNodes = 16;
    const int maxHops = 6;
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
        15  //dataslotsPerUplinkTile
    );
    
    auto& streamCollection=*scheduler.getStreamCollection();
    NetworkTopology topology(config);
    scheduler.setTopology(&topology);
    scheduler.startThread();
    
    this_thread::sleep_for(milliseconds(10)); //TODO find a way to sync with the scheduler
    
    {
        UpdatableQueue<SMEKey, StreamManagementElement> smes;
        StreamManagementElement sme(
            StreamInfo(
                StreamId(0,0,0,1),
                StreamParameters(3,10,10,0),
                StreamStatus::LISTEN_WAIT
            ),
            SMEType::LISTEN
        );
        smes.enqueue(sme.getKey(),sme);
        streamCollection.receiveSMEs(smes);
    }
    
    scheduler.beginScheduling();
    
    this_thread::sleep_for(seconds(1)); //TODO find a way to sync with the scheduler
    
    
}
