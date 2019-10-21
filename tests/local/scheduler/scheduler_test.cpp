
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
    // Populate fake network topology
    topology.addEdge(0,1);
    topology.addEdge(0,3);
    topology.addEdge(0,5);
    topology.addEdge(0,13);
    topology.addEdge(0,14);
    topology.addEdge(1,3);
    topology.addEdge(1,5);
    topology.addEdge(1,13);
    topology.addEdge(1,14);
    topology.addEdge(2,4);
    topology.addEdge(2,6);
    topology.addEdge(2,8);
    topology.addEdge(3,5);
    topology.addEdge(4,6);
    topology.addEdge(4,8);
    topology.addEdge(4,9);
    topology.addEdge(4,14);
    topology.addEdge(5,13);
    topology.addEdge(5,14);
    topology.addEdge(8,14);
    topology.addEdge(9,10);
    topology.addEdge(9,11);
    topology.addEdge(9,14);
    topology.addEdge(10,11);
    topology.addEdge(10,14);
    topology.addEdge(11,12);
    topology.addEdge(12,13);
    // Populate fake servers and streams
    {
        UpdatableQueue<SMEKey, StreamManagementElement> smes;
        StreamParameters params = StreamParameters(4,4,10,0);
        StreamStatus conn = StreamStatus::CONNECTING;
        StreamManagementElement sme(
            StreamInfo(StreamId(0,0,0,1), params, StreamStatus::LISTEN_WAIT),
            SMEType::LISTEN
        );
        StreamManagementElement sme2(StreamInfo(StreamId(1,0,0,1), params, conn),
                                     SMEType::CONNECT);
        StreamManagementElement sme3(StreamInfo(StreamId(3,0,0,1), params, conn),
                                     SMEType::CONNECT);
        StreamManagementElement sme4(StreamInfo(StreamId(4,0,0,1), params, conn),
                                     SMEType::CONNECT);
        StreamManagementElement sme5(StreamInfo(StreamId(5,0,0,1), params, conn),
                                     SMEType::CONNECT);
        StreamManagementElement sme6(StreamInfo(StreamId(8,0,0,1), params, conn),
                                     SMEType::CONNECT);
        StreamManagementElement sme7(StreamInfo(StreamId(9,0,0,1), params, conn),
                                     SMEType::CONNECT);
        StreamManagementElement sme8(StreamInfo(StreamId(10,0,0,1), params, conn),
                                     SMEType::CONNECT);
        StreamManagementElement sme9(StreamInfo(StreamId(11,0,0,1), params, conn),
                                     SMEType::CONNECT);
        StreamManagementElement sme10(StreamInfo(StreamId(12,0,0,1), params, conn),
                                     SMEType::CONNECT);
        StreamManagementElement sme11(StreamInfo(StreamId(13,0,0,1), params, conn),
                                     SMEType::CONNECT);
        StreamManagementElement sme12(StreamInfo(StreamId(14,0,0,1), params, conn),
                                     SMEType::CONNECT);

        smes.enqueue(sme.getKey(),sme);
        smes.enqueue(sme2.getKey(),sme2);
        smes.enqueue(sme3.getKey(),sme3);
        smes.enqueue(sme4.getKey(),sme4);
        smes.enqueue(sme5.getKey(),sme5);
        smes.enqueue(sme6.getKey(),sme6);
        smes.enqueue(sme7.getKey(),sme7);
        smes.enqueue(sme8.getKey(),sme8);
        smes.enqueue(sme9.getKey(),sme9);
        smes.enqueue(sme10.getKey(),sme10);
        smes.enqueue(sme11.getKey(),sme11);
        smes.enqueue(sme12.getKey(),sme12);
        streamCollection.receiveSMEs(smes);
    }
    scheduler.startThread();
    scheduler.sync();   
    scheduler.beginScheduling();
    scheduler.sync();
    
    exit(1);
}
