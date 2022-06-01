/***************************************************************************
 *   Copyright (C) 2017-2019 by Polidori Paolo, Terraneo Federico,         *
 *   Federico Amedeo Izzo                                                  *
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

#include <cstdio>
#include <unistd.h>
#include <miosix.h>
#include <chrono>
#include "network_module/network_configuration.h"
#include "network_module/dynamic_tdmh.h"
#include "network_module/master_tdmh.h"
#include "network_module/downlink_phase/timesync/networktime.h"
#include "network_module/util/stackrange.h"
#include "network_module/util/stats.h"
#include "interfaces-impl/gpio_timer_corr.h"
#include "interfaces-impl/power_manager.h"

// For tests with a 50ms tile and 2ms slots
// #define SMALL_DATA

// For using callbacks API instead of the write/read one
#define USE_CALLBACKS

// For benchmarking end-to-end latency
//#define LATENCY_PROFILING

using namespace std;
using namespace mxnet;
using namespace miosix;
using namespace std::placeholders;

const int maxNodes = 16;
const int maxHops = 6;
const int txPower = 5; //dBm

#ifdef USE_CALLBACKS
// send callback    ~= 55000
// receive callback ~= 21000
const unsigned int callbacksExecTime = 500000;
#else
const unsigned int callbacksExecTime = 0;
#endif

FastMutex m;
MediumAccessController *tdmh = nullptr;

/* Arguments passed to the TDMH thread */
class Arg
{
public:
    Arg(unsigned char id, unsigned char hop=false) : id(id), hop(hop) {}
    const unsigned char id, hop;
};

class StreamThreadPar
{
public:
    StreamThreadPar(int stream) : stream(stream) {};
    int stream;
};

inline int guaranteedTopologies(int maxNumNodes, bool useWeakTopologies)
{
    /*
     * Dynamic Uplink allocation implemented
     * the parameter topologySMERatio configures the fraction of uplink packet
     * reserved for topology messages
     * UplinkMessagePkt   { hop, assignee, numTopology, numSME } : 4
     * NeighborTable      { bitmask }                            : bitmaskSize
     * TopologyElement(s) { nodeId, bitmask } * n                : (1+bitmaskSize) * n
     */
    int bitmaskSize = useWeakTopologies ? maxNumNodes/4 : maxNumNodes/8;
    const float topologySMERatio = 0.5;
    int packetCapacity = (125 - 4 - bitmaskSize) / (1 + bitmaskSize);
    return std::min<int>(packetCapacity * topologySMERatio, maxNumNodes - 2);
}

void printStatus(StreamStatus status) {
    switch(status){
    case StreamStatus::CONNECTING:
        printf("CONNECTING");
        break;
    case StreamStatus::CONNECT_FAILED:
        printf("CONNECT_FAILED");
        break;
    case StreamStatus::ACCEPT_WAIT:
        printf("ACCEPT_WAIT");
        break;
    case StreamStatus::ESTABLISHED:
        printf("ESTABLISHED");
        break;
    case StreamStatus::LISTEN_WAIT:
        printf("LISTEN_WAIT");
        break;
    case StreamStatus::LISTEN_FAILED:
        printf("LISTEN_FAILED");
        break;
    case StreamStatus::LISTEN:
        printf("LISTEN");
        break;
    case StreamStatus::REMOTELY_CLOSED:
        printf("REMOTELY_CLOSED");
        break;
    case StreamStatus::REOPENED:
        printf("REOPENED");
        break;
    case StreamStatus::CLOSE_WAIT:
        printf("CLOSE_WAIT");
        break;
    case StreamStatus::UNINITIALIZED:
        printf("UNINITIALIZED");
        break;
    default:
        printf("INVALID!");
    }
    printf("\n");
}

void masterNode(void*)
{
    if (ENABLE_STACK_STATS_DBG) {
        printStackRange("MAC (master)");
    }

    try {
        printf("Master node\n");
        bool useWeakTopologies=true;
        const NetworkConfiguration config(
            maxHops,       //maxHops
            maxNodes,      //maxNodes
            0,             //networkId
            false,         //staticHop
            6,             //panId
            txPower,       //txPower
            2450,          //baseFrequency
            10000000000,   //clockSyncPeriod
            guaranteedTopologies(maxNodes,useWeakTopologies), //guaranteedTopologies
            1,             //numUplinkPackets
            100000000,     //tileDuration
            150000,        //maxAdmittedRcvWindow
            callbacksExecTime, //callbacksExecutionTime
            8,             //maxRoundsUnavailableBecomesDead
            128,           //maxRoundsWeakLinkBecomesDead
            -75,           //minNeighborRSSI
            -95,           //minWeakNeighborRSSI
            4,             //maxMissedTimesyncs
            true,          //channelSpatialReuse
            useWeakTopologies          //useWeakTopologies
#ifdef CRYPTO
            ,
            true,          //authenticateControlMessages
            false,         //encryptControlMessages
            true,          //authenticateDataMessages
            true,          //encryptDataMessages
            true,          //doMasterChallengeAuthentication
            500, //3000,          //masterChallengeAuthenticationTimeout
            3000           //rekeyingPeriod
#endif
        );
        printf("Starting TDMH with guaranteedTopologies=%d\n", guaranteedTopologies(maxNodes,useWeakTopologies));
        MasterMediumAccessController controller(Transceiver::instance(), config);
        tdmh = &controller;
        controller.run();
    } catch(exception& e) {
        for(;;)
        {
            printf("exception %s\n",e.what());
            Thread::sleep(1000);
        }
    }
}

void dynamicNode(void* argv)
{
    if (ENABLE_STACK_STATS_DBG) {
        printStackRange("MAC (dynamic)");
    }

    try {
        auto arg = reinterpret_cast<Arg*>(argv);
        printf("Dynamic node %d",arg->id);
        if(arg->hop) printf(" forced hop %d",arg->hop);
        printf("\n");
        bool useWeakTopologies=true;
        const NetworkConfiguration config(
            maxHops,       //maxHops
            maxNodes,      //maxNodes
            arg->id,       //networkId
            arg->hop,      //staticHop
            6,             //panId
            txPower,       //txPower
            2450,          //baseFrequency
            10000000000,   //clockSyncPeriod
            guaranteedTopologies(maxNodes,useWeakTopologies), //guaranteedTopologies
            1,             //numUplinkPackets
            100000000,     //tileDuration
            150000,        //maxAdmittedRcvWindow
            callbacksExecTime, //callbacksExecutionTime
            8,             //maxRoundsUnavailableBecomesDead
            128,           //maxRoundsWeakLinkBecomesDead
            -75,           //minNeighborRSSI
            -95,           //minWeakNeighborRSSI
            4,             //maxMissedTimesyncs
            true,          //channelSpatialReuse
            useWeakTopologies          //useWeakTopologies
#ifdef CRYPTO
            ,
            true,          //authenticateControlMessages
            false,         //encryptControlMessages
            true,          //authenticateDataMessages
            true,          //encryptDataMessages
            true,          //doMasterChallengeAuthentication
            500, //3000,          //masterChallengeAuthenticationTimeout
            3000           //rekeyingPeriod
#endif
        );
        printf("Starting TDMH with guaranteedTopologies=%d\n", guaranteedTopologies(maxNodes,useWeakTopologies));
        DynamicMediumAccessController controller(Transceiver::instance(), config);
        {
            Lock<FastMutex> l(m);
            tdmh = &controller;
        }
        controller.run();
    } catch(exception& e) {
        for(;;)
        {
            printf("exception %s\n",e.what());
            Thread::sleep(1000);
        }
    }
}

void statThread(void *)
{
    if (ENABLE_STACK_STATS_DBG) {
        printStackRange("stat");
    }

    for(;;)
    {
        print_dbg("[H] Time=%lld MinHeap=%u Heap=%u\n", miosix::getTime(),
               miosix::MemoryProfiling::getAbsoluteFreeHeap(),
               miosix::MemoryProfiling::getCurrentFreeHeap());
        Thread::sleep(2000);
    }
}

#ifndef SMALL_DATA

struct Data
{
    Data() {}
    Data(int id, unsigned int counter) : id(id), time(miosix::getTime()), 
                   netTime(NetworkTime::now().get()),
                   minHeap(MemoryProfiling::getAbsoluteFreeHeap()),
                   heap(MemoryProfiling::getCurrentFreeHeap()),
                   counter(counter) {}
                   
    unsigned char getId()          const { return id; }
    long long     getTime()        const { return time; }
    long long     getNetworkTime() const { return netTime; }
    int           getMinHeap()     const { return minHeap; }
    int           getHeap()        const { return heap; }
    unsigned int  getCounter()     const { return counter; }
    
    unsigned char id;
    long long time;
    long long netTime;
    int minHeap;
    int heap;
    unsigned int counter;
}__attribute__((packed));

#else //SMALL_DATA

struct Data
{
    Data() {}
    Data(int id, unsigned int counter): id(id),
        minHeap(MemoryProfiling::getAbsoluteFreeHeap()), counter(counter) {}
    
    unsigned char getId()          const { return id; }
    long long     getTime()        const { return -1; }
    long long     getNetworkTime() const { return -1; }
    int           getMinHeap()     const { return minHeap; }
    int           getHeap()        const { return -1; }
    unsigned int  getCounter()     const { return counter; }
    
    unsigned char id;
    int minHeap;
    unsigned int counter;
}__attribute__((packed));

#endif //SMALL_DATA

#ifdef USE_CALLBACKS
int networkId;

// SEND CALLBACK
unsigned int sendMsgCounter = 0;
void sendCallback(void* data, unsigned int* size, StreamStatus status)
{
    if (status == StreamStatus::ESTABLISHED) {
        sendMsgCounter++;
        Data d(networkId, sendMsgCounter);
        *size = sizeof(Data);
        memcpy(data, &d, *size);
    }
    else {
        printf("[A] Send callback : stream status=");
        printStatus(status);
    }
}

// RECEIVE CALLBACK
struct CallbackData
{
    bool received;
    int receivedBytes;
    StreamStatus streamStatus;
    Data receivedData;
    long long receiveTime;
    std::mutex receiveMutex;
    std::condition_variable receiveCv;

    CallbackData() : received(false), receivedBytes(-1), streamStatus(StreamStatus::UNINITIALIZED),
                        receivedData(Data()), receiveTime(0) {}
    ~CallbackData() {}

    void recvCallback(void* data, unsigned int* size, StreamStatus status)
    {
        std::unique_lock<std::mutex> lck(receiveMutex);

        streamStatus = status;

        if (*size != 0) { // && streamStatus == StreamStatus::ESTABLISHED) {
            receivedBytes = *size;
            memcpy(&receivedData, data, receivedBytes);
        }
        else {
            receivedBytes = -1;
            receivedData = Data{-1, 0};
        }
        // take timestamp after the data has been
        // really made available to the application
        receiveTime = NetworkTime::now().get();
        received = true;
        receiveCv.notify_one();
    }
};

void streamThread(void *arg)
{
    if (ENABLE_STACK_STATS_DBG) {
        printStackRange("stream");
    }
    
    auto *s = reinterpret_cast<StreamThreadPar*>(arg);
    int stream = s->stream;

    StreamInfo info = getInfo(stream);
    StreamId id = info.getStreamId();
    printf("[A] Master node: Stream (%d,%d) accepted\n", id.src, id.dst);

    CallbackData c_data;
    std::function<void(void*,unsigned int*,StreamStatus)> recvCallback = 
                                std::bind(&CallbackData::recvCallback, &c_data, _1, _2, _3);
    printf("[A] Set receive callback for stream (%d,%d) \n", info.getSrc(), info.getDst());
    if (!setReceiveCallback(stream, recvCallback)) {
        printf("[A] Error : failed to set receive callback for stream (%d,%d) \n", info.getSrc(), info.getDst());
        return;
    }

    while(true) { // getInfo(stream).getStatus() == StreamStatus::ESTABLISHED
        Data data;
        int len = -1;
        long long now = 0;
        StreamStatus streamStatus;

        {
            std::unique_lock<std::mutex> lck(c_data.receiveMutex);

            while(!c_data.received) {
                c_data.receiveCv.wait(lck);
            }

            c_data.received = false;
            now = c_data.receiveTime;
            data = c_data.receivedData;
            len = c_data.receivedBytes;
            streamStatus = c_data.streamStatus;
        }

        if (c_data.streamStatus != StreamStatus::ESTABLISHED) {
            printf("[A] Stream status != ESTABLISHED, closing stream\n");
            break;
        }

        if(len >= 0) {
            if(len == sizeof(Data))
            {
                if(COMPRESSED_DBG==false)
                {
                    printf("[A] Received data from (%d,%d): ID=%d Time=%lld MinHeap=%u Heap=%u Counter=%u\n",
                       id.src, id.dst, data.getId(), data.getNetworkTime(), data.getMinHeap(), data.getHeap(), data.getCounter());
                } else {
                    printf("[A] R (%d,%d) ID=%d T=%lld MH=%u C=%u\n",
                       id.src, id.dst, data.getId(), data.getTime(), data.getMinHeap(), data.getCounter());
#ifdef LATENCY_PROFILING
                    long long latency = now - data.getNetworkTime();
                    printf("[A] (%d,%d): L=%lld\n", id.src, id.dst, latency);
#endif
                }
            } else {
                if(COMPRESSED_DBG==false)
                    printf("[E] Received wrong size data from Stream (%d,%d): %d\n",
                       id.src, id.dst, len);
                else
                    printf("[E] W (%d,%d) %d\n", id.src, id.dst, len);
            }
        }
        else {
            if(COMPRESSED_DBG==false)
                printf("[E] No data received from Stream (%d,%d): %d\n",
                id.src, id.dst, len);
            else {
                printf("[E] M (%d,%d)\n", id.src, id.dst);
            }
        }
    }

    printf("[A] Stream (%d,%d) has been closed, status=", id.src, id.dst);
    printStatus(getInfo(stream).getStatus());
    // NOTE: Remember to call close() after the stream has been closed remotely
    mxnet::close(stream);
    delete s;
}

#else // !defined USE_CALLBACKS

void streamThread(void *arg)
{
    if (ENABLE_STACK_STATS_DBG) {
        printStackRange("stream");
    }

    auto *s = reinterpret_cast<StreamThreadPar*>(arg);
    int stream = s->stream;
    StreamInfo info = getInfo(stream);
    StreamId id = info.getStreamId();
    print_dbg("[A] Master node: Stream (%d,%d) accepted\n", id.src, id.dst);
    int cnt = 0;

    while(getInfo(stream).getStatus() == StreamStatus::ESTABLISHED) {
        Data data;
        int len = mxnet::read(stream, &data, sizeof(data));
        
#ifdef LATENCY_PROFILING
        long long now = mxnet::NetworkTime::now().get();
#endif

        if(++cnt>300) {
            cnt = 0;
#ifdef _MIOSIX
            if (ENABLE_STACK_STATS_DBG) {
                unsigned int stackSize = MemoryProfiling::getStackSize();
                unsigned int absFreeStack = MemoryProfiling::getAbsoluteFreeStack();
                print_dbg("[H] Stream thread stack %d/%d\n", stackSize - absFreeStack, stackSize);
            }
#endif
        }

        if(len > 0) {
            if(len == sizeof(data))
            {
                if(COMPRESSED_DBG==false)
                {
                    printf("[A] Received data from (%d,%d): ID=%d Time=%lld MinHeap=%u Heap=%u Counter=%u\n",
                       id.src, id.dst, data.getId(), data.getNetworkTime(), data.getMinHeap(), data.getHeap(), data.getCounter());
                } else {
                    printf("[A] R (%d,%d) ID=%d T=%lld MH=%u C=%u\n",
                       id.src, id.dst, data.getId(), data.getTime(), data.getMinHeap(), data.getCounter());

#ifdef LATENCY_PROFILING
                    long long latency = now - data.getNetworkTime();
                    printf("[A] (%d,%d): L=%lld\n", id.src, id.dst, latency);
#endif
                }
            } else {
                if(COMPRESSED_DBG==false)
                    printf("[E] Received wrong size data from Stream (%d,%d): %d\n",
                       id.src, id.dst, len);
                else
                    printf("[E] W (%d,%d) %d\n", id.src, id.dst, len);
            }
        }
        else if(len == -1) {
            if(COMPRESSED_DBG==false)
                printf("[E] No data received from Stream (%d,%d): %d\n",
                   id.src, id.dst, len);
            else {
                print_dbg("[E] M (%d,%d)\n", id.src, id.dst);
            }
        }
        else {
            print_dbg("[E] M (%d,%d) Read returned %d\n", id.src, id.dst, len);
        }
    }
    printf("[A] Stream (%d,%d) has been closed, status=", id.src, id.dst);
    printStatus(getInfo(stream).getStatus());
    // NOTE: Remember to call close() after the stream has been closed remotely
    mxnet::close(stream);
    delete s;
}
#endif

void openServer(unsigned char port, StreamParameters params) {
    /* Wait for TDMH to become ready */
    MACContext* ctx = tdmh->getMACContext();
    while(!ctx->isReady()) {
        Thread::sleep(1000);
    }

#ifdef USE_CALLBACKS
    networkId = ctx->getNetworkId();
#endif

    printf("[A] Opening server on port %d\n", port);
    /* Open a Server to listen for incoming streams */
    int server = listen(port,              // Destination port
                        params);           // Server parameters
    if(server < 0) {                
        printf("[A] Server opening failed! error=%d\n", server);
        return;
    }

    try {
        while(getInfo(server).getStatus() == StreamStatus::LISTEN) {
            int stream = accept(server);
            Thread::create(streamThread, 1536, MAIN_PRIORITY+1, new StreamThreadPar(stream));
        }
    } catch(exception& e) {
        printf("Unexpected exception while server accept: %s\n",e.what());
    } catch(...) {
        printf("Unexpected unknown exception while server accept\n");
    }
    printf("[A] Server on port %d closed, status=", port);
    printStatus(getInfo(server).getStatus());
    mxnet::close(server);
}

void openStream(unsigned char dest, unsigned char port, StreamParameters params) {
    /* Wait for TDMH to become ready */
    MACContext* ctx = tdmh->getMACContext();
    while(!ctx->isReady()) {
        Thread::sleep(1000);
    }
    /* Delay the Stream opening so it gets opened after the StreamServer */
    Thread::sleep(1000);

#ifdef USE_CALLBACKS
    networkId = ctx->getNetworkId();
#endif

    /* Open Stream from node */
    while(true){
        try{
            printf("[A] N=%d Waiting to authenticate master node\n", ctx->getNetworkId());
            if(waitForMasterTrusted()) {
                printf("[A] N=%d StreamManager not present! \n", ctx->getNetworkId());
                continue;
            }
            /* Open a Stream to another node */
            printf("[A] Opening stream to node %d\n", dest);
#ifdef USE_CALLBACKS
            int stream = connect(dest,          // Destination node
                                 port,          // Destination port
                                 params);       // Stream parameters
#else
            int stream = connect(dest,          // Destination node
                                 port,          // Destination port
                                 params,        // Stream parameters 
                                 2*ctx->getDataSlotDuration()); // Wakeup advance (max 1 tile)
#endif
            if(stream < 0) {                
                printf("[A] Stream opening failed! error=%d\n", stream);
                continue;
            }
#ifdef USE_CALLBACKS
            else {
                printf("[A] Set send callback \n");
                if (!setSendCallback(stream, &sendCallback)) {
                    printf("[A] Error : failed to set send callback! \n");
                }
            }
#endif
            unsigned int counter = 0;
            bool first = true;
            while(getInfo(stream).getStatus() == StreamStatus::ESTABLISHED) {
                if(first) {
                    first=false;
                    printf("[A] Stream opened \n");
                }

#ifdef USE_CALLBACKS
                // avoid loop to continuously spin
                Thread::sleep(1000);
#else
                //printf("\n[A] Stream wait! NT = %llu\n\n", NetworkTime::now().get());
                int res = mxnet::wait(stream);
                if (res != 0) {
                    printf("[A] Stream wait error\n");
                }

                counter++;

                //printf("\n[A] Stream wakeup! NT=%llu C=%u\n", NetworkTime::now().get(), counter);
                Data data(ctx->getNetworkId(), counter);
                int ret = mxnet::write(stream, &data, sizeof(data));      
                if(ret >= 0) {
                    printf("[A] Sent ID=%d Time=%lld NetTime=%lld MinHeap=%u Heap=%u Counter=%u\n",
                              data.getId(), data.getTime(), data.getNetworkTime(), data.getMinHeap(), data.getHeap(), data.getCounter());
                }
                else
                    printf("[E] Error sending data, result=%d\n", ret);
#endif
            }

            printf("[A] Stream (%d,%d) closed, status=", ctx->getNetworkId(), dest);
            printStatus(getInfo(stream).getStatus());
            // NOTE: Remember to call close() after the stream has been closed
            mxnet::close(stream);
        
        } catch(exception& e) {
            printf("exception %s\n",e.what());
        }
    }
}

unsigned long long getUniqueID() {
    return *reinterpret_cast<unsigned long long*>(0x0FE081F0);
}

void idle() {
    // Remove this sleep loop when enabling stream code
    for(;;) {            
        Thread::sleep(5000);
    }
}

void periodicGPIOThread(void* param)
{
    //enable power to optical fiber transeiver
    expansion::gpio16::mode(Mode::OUTPUT);
    expansion::gpio16::high();
    auto& timestamp=GPIOtimerCorr::instance();
    //MACContext *ctx = tdmh->getMACContext();

    /*while(!ctx->isReady())
        Thread::sleep(1000);*/

    for(;;)
    {

        auto period=NetworkTime::fromNetworkTime(1000000000);
        auto now=NetworkTime::fromNetworkTime(((NetworkTime::now().get()/period.get())+2)*period.get());
        printf("--- %lld %lld %lld\n",now.get(),now.toLocalTime(),period.get());
        for(auto offset=NetworkTime::fromLocalTime(0).get(); offset == NetworkTime::fromLocalTime(0).get(); now += period) 
        {
            //ctx->sleepUntil(now.toLocalTime() - 100000000);
            //expansion::gpio16::high();
            timestamp.absoluteWaitTrigger(timestamp.ns2tick(now.toLocalTime()));
            //ctx->sleepUntil(now.toLocalTime() + 100000000);
            //expansion::gpio16::low();
            printf("[TIMESTAMP] %lld %lld %lld\n",now.get(),now.toLocalTime(), NetworkTime::now().get());
            // MemoryProfiling::print();
        }
    } 
}

int main()
{
    if (ENABLE_STACK_STATS_DBG) {
        printStackRange("main");
    }
    
    unsigned long long nodeID = getUniqueID();
    printf("### TDMH Test code ###\nNodeID=%llx\n", nodeID);
    const int macThreadStack = 4096;
    /* Start TDMH thread mapping node unique ID to network ID */
    switch(nodeID) {
    //case 0x243537035155c338:
      //  Thread::create(masterNode, macThreadStack, PRIORITY_MAX-1, nullptr, Thread::JOINABLE);
      //  break;
    case 0x243537005155c356:
        Thread::create(dynamicNode, macThreadStack, PRIORITY_MAX-1, new Arg(1), Thread::JOINABLE);
        break;
    case 0x243537005155c346:
        Thread::create(dynamicNode, macThreadStack, PRIORITY_MAX-1, new Arg(2), Thread::JOINABLE);
        break;
    case 0x243537025155c346:
        Thread::create(dynamicNode, macThreadStack, PRIORITY_MAX-1, new Arg(3), Thread::JOINABLE);
        break;
    case 0x243537035155c356:
        Thread::create(dynamicNode, macThreadStack, PRIORITY_MAX-1, new Arg(4), Thread::JOINABLE);
        break;
    case 0x243537035155bdca:
        Thread::create(dynamicNode, macThreadStack, PRIORITY_MAX-1, new Arg(5), Thread::JOINABLE);
        break;
    case 0x243537015155bdab:
        Thread::create(dynamicNode, macThreadStack, PRIORITY_MAX-1, new Arg(6), Thread::JOINABLE);
        break;
    case 0x243537015155c9bf:
        Thread::create(dynamicNode, macThreadStack, PRIORITY_MAX-1, new Arg(7), Thread::JOINABLE);
        break;
    case 0x2435370352c6aa9a:
        Thread::create(dynamicNode, macThreadStack, PRIORITY_MAX-1, new Arg(8), Thread::JOINABLE);
        break;
    case 0x243537015155bdba:
        Thread::create(dynamicNode, macThreadStack, PRIORITY_MAX-1, new Arg(9), Thread::JOINABLE);
        break;
    case 0x243537025155bdba:        
        Thread::create(dynamicNode, macThreadStack, PRIORITY_MAX-1, new Arg(10), Thread::JOINABLE);
        break;
    case 0x243537025155bdca:
        Thread::create(dynamicNode, macThreadStack, PRIORITY_MAX-1, new Arg(11), Thread::JOINABLE);
        break;
    case 0x243537005155bdba:
        Thread::create(dynamicNode, macThreadStack, PRIORITY_MAX-1, new Arg(12), Thread::JOINABLE);
        break;
    case 0x243537035155bdba:
        //Thread::create(dynamicNode, macThreadStack, PRIORITY_MAX-1, new Arg(13), Thread::JOINABLE);
    Thread::create(masterNode, macThreadStack, PRIORITY_MAX-1, nullptr, Thread::JOINABLE);
        break;
    case 0x243537005155bdab:
        Thread::create(dynamicNode, macThreadStack, PRIORITY_MAX-1, new Arg(14), Thread::JOINABLE);
        break;
    default:     
        printf("ERROR: nodeID is not mapped to any node, halting!\n");
    }


    /* Wait for tdmh pointer to become valid */
    for(;;)
    {
        Lock<FastMutex> l(m);
        if(tdmh) break;
    }
    Thread::sleep(5000);

    MACContext* ctx = tdmh->getMACContext();
    unsigned char netID = ctx->getNetworkId();

    Thread::create(periodicGPIOThread, macThreadStack, PRIORITY_MAX-1, nullptr, Thread::JOINABLE);
    // NOTE: the server parameters represent the maximum values accepted
    // by the server, the actual stream parameters are negotiated with
    // the value required by the client.
    // You don't need to change the server parameters, but just the
    // ones of the client
    StreamParameters serverParams(Redundancy::TRIPLE_SPATIAL,
                                  Period::P1,
                                  1,     // payload size
                                  Direction::TX);
    StreamParameters clientParams(Redundancy::TRIPLE_SPATIAL,
                                  Period::P10,
                                  1,     // payload size
                                  Direction::TX);
    unsigned char port = 1;
    /* Perform actions depending on your network ID */
    switch(netID) {
    case 0:
        // Master node code
        if (ENABLE_STACK_STATS_DBG) {
            Thread::create(statThread, 2048, MAIN_PRIORITY);
        }
        for(;;) openServer(port, serverParams);
        break;
    case 1:
        // NOTE: decopmment the idle() function when openStream is commented
        //idle();
        openStream(0, port, clientParams);
        break;
    case 2:
        // NOTE: decopmment the idle() function when openStream is commented
        //idle();
        openStream(0, port, clientParams);
        break;
    case 3:
        // NOTE: decopmment the idle() function when openStream is commented
        //idle();
        openStream(0, port, clientParams);
        break;
    case 4:
        // NOTE: decopmment the idle() function when openStream is commented
        //idle();
        openStream(0, port, clientParams);
        break;
    case 5:
        // NOTE: decopmment the idle() function when openStream is commented
        //idle();
        openStream(0, port, clientParams);
        break;
    case 6:
        // NOTE: decopmment the idle() function when openStream is commented
        //idle();
        openStream(0, port, clientParams);
        break;
    case 7:
        // NOTE: decopmment the idle() function when openStream is commented
        //idle();
        openStream(0, port, clientParams);
        break;
    case 8:
        // NOTE: decopmment the idle() function when openStream is commented
        //idle();
        openStream(0, port, clientParams);
        break;
    case 9:
        // NOTE: decopmment the idle() function when openStream is commented
        //idle();
        openStream(0, port, clientParams);
        break;
    case 10:
        // NOTE: decopmment the idle() function when openStream is commented
        //idle();
        openStream(0, port, clientParams);
        break;
    case 11:
        // NOTE: decopmment the idle() function when openStream is commented
        //idle();
        openStream(0, port, clientParams);
        break;
    case 12:
        // NOTE: decopmment the idle() function when openStream is commented
        //idle();
        openStream(0, port, clientParams);
        break;
    case 13:
        // NOTE: decopmment the idle() function when openStream is commented
        //idle();
        openStream(0, port, clientParams);
        break;
    case 14:
        // NOTE: decopmment the idle() function when openStream is commented
        //idle();
        openStream(0, port, clientParams);
        break;
    default:
        idle();
    }
 
}
