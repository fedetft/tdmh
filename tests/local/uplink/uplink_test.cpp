
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
    
    nt.receivedMessage(7,1,-99,false,RuntimeBitset(maxNodes,0));
    
    assert(nt.hasPredecessor());
    assert(nt.getBestPredecessor()==7);
    //printf("%s\n",string(nt.getMyTopologyElement().getNeighbors()).c_str());
    assert(nt.getMyTopologyElement().getNeighbors()==rts("0000000000000000"));

    nt.receivedMessage(7,1,-50,false,RuntimeBitset(maxNodes,0));
    
    assert(nt.hasPredecessor());
    assert(nt.getBestPredecessor()==7);
    assert(nt.getMyTopologyElement().getNeighbors()==rts("0000000100000000"));
    
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
}

int main()
{
    testNeighborTable();
    
    cout<<"ok"<<endl;
    return 0;
}
