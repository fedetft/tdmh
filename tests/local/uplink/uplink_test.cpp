
#include <iostream>
#include <thread>
#include <chrono>
#include "uplink_phase/topology/neighbor_table.h"

using namespace std;
using namespace std::chrono;
using namespace mxnet;

// Convert string to RuntimeBitset
RuntimeBitset rts(const string& s)
{
    RuntimeBitset result(s.size(),0);
    for(int i=0;i<s.size();i++) if(s.at(i)=='1') result[i]=1;
    return result;
}

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

void testNeighborTable()
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
    
    NeighborTable nt(
        config,
        5, //myId
        2  //hop
    );
    
    //Add link with lower hop, sub-threshold: should be the predecessor but not in the topology
    nt.receivedMessage(7,1,-99,false,RuntimeBitset(maxNodes,0));
    
    assert(nt.hasPredecessor());
    assert(nt.getBestPredecessor()==7);
    //printf("%s\n",string(nt.getMyTopologyElement().getNeighbors()).c_str());
    assert(nt.getMyTopologyElement().getNeighbors()==rts("0000000000000000"));

    //Add same link, above threshold: should be also in the topology
    nt.receivedMessage(7,1,-50,false,RuntimeBitset(maxNodes,0));
    
    assert(nt.hasPredecessor());
    assert(nt.getBestPredecessor()==7);
    assert(nt.getMyTopologyElement().getNeighbors()==rts("0000000100000000"));
    
    //After three misses should disappear from both
    nt.missedMessage(7);
    
    assert(nt.hasPredecessor());
    assert(nt.getBestPredecessor()==7);
    assert(nt.getMyTopologyElement().getNeighbors()==rts("0000000100000000"));
    
    nt.missedMessage(7);
    
    assert(nt.hasPredecessor());
    assert(nt.getBestPredecessor()==7);
    assert(nt.getMyTopologyElement().getNeighbors()==rts("0000000100000000"));
    
    nt.missedMessage(7);
    
    assert(nt.hasPredecessor()==false);
    assert(nt.getMyTopologyElement().getNeighbors()==rts("0000000000000000"));
    
    //Add bad link with higher rssi and good link with low rssi
    //The bad but high should be in topology, the good but low rssi the predecessor
    nt.receivedMessage(8,1,-50,true,RuntimeBitset(maxNodes,0));
    nt.receivedMessage(5,1,-80,false,RuntimeBitset(maxNodes,0));
    
    assert(nt.hasPredecessor());
    assert(nt.getBestPredecessor()==5);
    assert(nt.getMyTopologyElement().getNeighbors()==rts("0000000010000000"));
    
    //After three misses should disappear from both
    nt.missedMessage(8);
    nt.missedMessage(5);
    
    assert(nt.hasPredecessor());
    assert(nt.getBestPredecessor()==5);
    assert(nt.getMyTopologyElement().getNeighbors()==rts("0000000010000000"));
    
    nt.missedMessage(8);
    nt.missedMessage(5);
    
    assert(nt.hasPredecessor());
    assert(nt.getBestPredecessor()==5);
    assert(nt.getMyTopologyElement().getNeighbors()==rts("0000000010000000"));
    
    nt.missedMessage(8);
    nt.missedMessage(5);
    
    assert(nt.hasPredecessor()==false);
    assert(nt.getMyTopologyElement().getNeighbors()==rts("0000000000000000"));
    
    //Add three good links, make sure it picks the one with the higest rssi as predecessor
    nt.receivedMessage(7,1,-82,false,RuntimeBitset(maxNodes,0));
    nt.receivedMessage(15,1,-70,false,RuntimeBitset(maxNodes,0));
    nt.receivedMessage(2,1,-5,false,RuntimeBitset(maxNodes,0));
    
    assert(nt.hasPredecessor());
    assert(nt.getBestPredecessor()==2);
    assert(nt.getMyTopologyElement().getNeighbors()==rts("0010000000000001"));
}

int main()
{
    testNeighborTable();
    
    cout<<"ok"<<endl;
    return 0;
}
