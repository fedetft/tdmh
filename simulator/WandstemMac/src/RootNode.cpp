//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU Lesser General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU Lesser General Public License for more details.
//
// You should have received a copy of the GNU Lesser General Public License
// along with this program.  If not, see http://www.gnu.org/licenses/.
//

#include "RootNode.h"

#include "network_module/master_tdmh.h"
#include "network_module/network_configuration.h"
#include "network_module/util/stats.h"
#include <chrono>
#include <functional>
#include <iostream>
#include <list>
#include <miosix.h>
#include <stdexcept>

Define_Module(RootNode);

using namespace std;
using namespace mxnet;
using namespace std::placeholders;

struct Data {
  Data() {}
  Data(int id, unsigned int counter, long long timestamp)
      : id(id), counter(counter), timestamp(timestamp) {}
  unsigned char id;
  unsigned int counter;
  long long timestamp;

} __attribute__((packed));

class StreamThreadPar {
public:
  StreamThreadPar(int stream, StreamManager *mgr)
      : stream(stream), mgr(mgr) {};
  int stream;
  StreamManager *mgr;
};

void RootNode::activity() {
  using namespace miosix;
  print_dbg("Master node\n");
  bool useWeakTopologies = true;
  const NetworkConfiguration config(
      hops,                                           // maxHops
      nodes,                                          // maxNodes
      address,                                        // networkId
      false,                                          // staticHop
      6,                                              // panId
      5,                                              // txPower
      2450,                                           // baseFrequency
      10000000000,                                    // clockSyncPeriod
      guaranteedTopologies(nodes, useWeakTopologies), // guaranteedTopologies
      1,                                              // numUplinkPackets
      100000000,                                      // tileDuration
      150000,                                         // maxAdmittedRcvWindow
      0,                                              //callbacksExecutionTime
      3,                // maxRoundsUnavailableBecomesDead
      16,               // maxRoundsWeakLinkBecomesDead
      -75,              // minNeighborRSSI
      -90,              // minWeakNeighborRSSI
      3,                // maxMissedTimesyncs
      true,             // channelSpatialReuse
      useWeakTopologies // useWeakTopologies
#ifdef CRYPTO
      ,
      true,  // authenticateControlMessages
      false, // encryptControlMessages
      true,  // authenticateDataMessages
      true,  // encryptDataMessages
      true,  // doMasterChallengeAuthentication
      3000,  // masterChallengeAuthenticationTimeout
      3000   // rekeyingPeriod
#endif
  );
  MasterMediumAccessController controller(Transceiver::instance(), config);

  tdmh = &controller;
  thread *t = nullptr;
  try {
    if (openStream)
      t = new thread(&RootNode::application, this);
    controller.run();
    quit.store(true);
    if (t)
      t->join();
  } catch (exception &e) {
    // Note: omnet++ seems to terminate coroutines with an exception
    // of type cStackCleanupException. Squelch these
    if (string(typeid(e).name()).find("cStackCleanupException") == string::npos)
      cerr << "\nException thrown: " << e.what() << endl;
    // TODO: perform clean shutdown when simulation stops
    exit(0);
    quit.store(true);
    if (t)
      t->join();
    throw;
  } catch (...) {
    quit.store(true);
    if (t)
      t->join();
    cerr << "\nUnnamed exception thrown" << endl;
    throw;
  }
}

void RootNode::initialize() {
  NodeBase::initialize();
  nodesNum = par("nodes").intValue();
}

void RootNode::application() {
  /* Wait for TDMH to become ready */
  MACContext *ctx = tdmh->getMACContext();
  while (!ctx->isReady()) {
  }
  openServer(ctx, 1, Period::P1, Redundancy::TRIPLE_SPATIAL);
}

void RootNode::openServer(MACContext *ctx, unsigned char port, Period period,
                          Redundancy redundancy) {
  try {
    // NOTE: we can't use Stream API functions in simulator
    // so we have to get a pointer to StreamManager
    StreamManager *mgr = ctx->getStreamManager();
    auto params = StreamParameters(redundancy,     // Redundancy
                                   period,         // Period
                                   1,              // Payload size
                                   Direction::TX); // Direction

    printf("[A] Opening server on port %d\n", port);
    /* Open a Server to listen for incoming streams */
    int server = mgr->listen(port,    // Destination port
                             params); // Server parameters

    if (server < 0) {
      printf("[A] Server opening failed! error=%d\n", server);
      return;
    }

    while (mgr->getInfo(server).getStatus() == StreamStatus::LISTEN) {
      int stream = mgr->accept(server);

      // pair<int, StreamManager*> arg = make_pair(stream, mgr);
      thread t1(&RootNode::streamThread, this,
                new StreamThreadPar(stream, mgr));
      t1.detach();
    }
  } catch (exception &e) {
    // Note: omnet++ seems to terminate coroutines with an exception
    // of type cStackCleanupException. Squelch these
    if (string(typeid(e).name()).find("cStackCleanupException") == string::npos)
      cerr << "\nException thrown: " << e.what() << endl;
  }
}

void RootNode::streamThread(void *arg) {
    try{
        auto *s = reinterpret_cast<StreamThreadPar*>(arg);
        int stream = s->stream;
        StreamManager* mgr = s->mgr;
        StreamId id = mgr->getInfo(stream).getStreamId();
        unsigned int receivedCounter = 0;
        printf("[A] Master node: Stream (%d,%d) accepted\n", id.src, id.dst);
        // Receive data until the stream is closed
        while(mgr->getInfo(stream).getStatus() == StreamStatus::ESTABLISHED) {
            Data data;
            int len = mgr->read(stream, &data, sizeof(data));
            if(len >= 0) {
                if(len == sizeof(data))
                {   
                    receivedCounter++;
                    printf("[A] Received data from (%d,%d): ID=%d MinHeap=0 Heap=0 Time=%llu Counter=%u Received=%u\n",
                            id.src, id.dst, data.id, miosix::getTime(), data.counter, receivedCounter);
#ifdef LATENCY_PROFILING
                    long long latency = miosix::getTime() - data.timestamp;
                    printf("[A] (%d,%d): L=%lld\n", id.src, id.dst, latency);
#endif
                }
                else
                    printf("[E] Received wrong size data from Stream (%d,%d): %d\n",
                            id.src, id.dst, len);
            }
            else if(len == -1) {
                // No data received
                printf("[E] No data received from Stream (%d,%d)\n", id.src, id.dst);
            }
        }
        printf("[A] Stream (%d,%d) was closed\n", id.src, id.dst);
    }catch(...){
        printf("Exception thrown in streamThread\n");
    }
}
