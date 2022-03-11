/***************************************************************************
 *   Copyright (C) 2022 by Luca Conterio                                   *
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

#include <miosix.h>
#include <network_module/tdmh.h>
#include <experiments/control_demo/src/utils.h>

using namespace std;
using namespace mxnet;
using namespace miosix;

class ActuatorNode {

public:
    ActuatorNode(MACContext* c) : ctx(c) {}

    void run() {
        while(!ctx->isReady()) {
            Thread::sleep(1000);
        }

        while(true) {
            int server = openServer(PORT1);
            if(server < 0) {                
                //printf("[A] Server opening failed! error=%d\n", server);
                Thread::sleep(2000);
                continue;
            }

            try {
                while(getInfo(server).getStatus() == StreamStatus::LISTEN) {
                    int stream = accept(server);
                    Thread::create(&ActuatorNode::processThreadLauncher, 1536, 
                                    MAIN_PRIORITY+1, new StreamThreadPar(this, stream));
                }
            } catch(exception& e) {
                //printf("Unexpected exception while server accept: %s\n",e.what());
            } catch(...) {
                //printf("Unexpected unknown exception while server accept\n");
            }
            //printf("[A] Server on port %d closed, status=", port);
            //printStatus(getInfo(server).getStatus());
            mxnet::close(server);
        }
    }

    void process(StreamThreadPar* arg) {
        auto *s = reinterpret_cast<StreamThreadPar*>(arg);
        int stream = s->stream;
        StreamInfo info = getInfo(stream);
        //StreamId id = info.getStreamId();
        //print_dbg("[A] Stream (%d,%d) accepted\n", id.src, id.dst);

        Thread::sleep(streamOpeningDelay);
        
        // open stream to ActuatorNode
        int outStream = -1;
        if (first) {
            bool outStreamOpened = false;
            while(!outStreamOpened) {
                outStream = openStream(controllerNodeID, PORT1);
                if(outStream < 0) {                
                    //printf("[A] Stream opening failed! error=%d\n", outStream);
                    Thread::sleep(1000);
                }
                else {
                    outStreamOpened = true;
                }
            }
        }
        first = false;

        while(getInfo(stream).getStatus() == StreamStatus::ESTABLISHED &&
                getInfo(outStream).getStatus() == StreamStatus::ESTABLISHED) {

            Data data;
            int len = mxnet::read(stream, &data, sizeof(data));

            long long now = NetworkTime::now().get();
            bool miss = false;

            if(len > 0) {
                if(len == sizeof(data))
                {   
                    controllerActuatorLatency = now - data.getNetworkTime();
                    //printf("[A] R (%d,%d) ID=%d NT=%lld C=%u\n",
                    //      id.src, id.dst, data.getId(), data.getNetworkTime(), data.getCounter());
                    sensorDataTimestamp = data.getSampleTime();
                    controlAction = data.getValue();
                    //printf("[A] (%d,%d): L=%lld\n", id.src, id.dst, sensorActuatorLatency);
                } else {
                    //printf("[E] W (%d,%d) %d\n", id.src, id.dst, len);
                }
            }
            else if(len == -1) { // miss
                miss = true;
                //print_dbg("[E] M (%d,%d)\n", id.src, id.dst);
            }
            else {
                //print_dbg("[E] M (%d,%d) Read returned %d\n", id.src, id.dst, len);
            }

            // wait two seconds after the sensor sampling time
            // but leave a couple of 2ms to send the control action over serial (printf)
            long long when = NetworkTime::fromNetworkTime(sensorDataTimestamp + 2000000000LL - 2500000LL).toLocalTime();
            ctx->sleepUntil(when);

            if (!miss) {
                actuate(controlAction);
                now = mxnet::NetworkTime::now().get();
                sensorActuatorLatency = now - sensorDataTimestamp;

                send(outStream, data.getCounter());
            }
        }
        //printf("[A] Stream (%d,%d) has been closed, status=", id.src, id.dst);
        //printStatus(getInfo(stream).getStatus());
        // NOTE: Remember to call close() after the stream has been closed remotely
        mxnet::close(stream);
        delete s;
        mxnet::close(outStream);
        first = true;
    }

private:
    MACContext *ctx;
    unsigned int counter = 0;
    unsigned int streamOpeningDelay = 15000;
    long long sensorDataTimestamp = 0;
    long long sensorActuatorLatency = 0;
    long long controllerActuatorLatency = 0;
    int controlAction = 0;

    bool first = true;

    int openServer(unsigned char port) {
        //printf("[A] N=%d Waiting to authenticate master node\n", ctx->getNetworkId());
        while(waitForMasterTrusted()) {
            ////printf("[A] N=%d StreamManager not present! \n", ctx->getNetworkId());
        }

        //printf("[A] Opening server on port %d\n", port);
        /* Open a Server to listen for incoming streams */
        int server = listen(port,          // Destination port
                            serverParams); // Server parameters

        return server;
    }

    int openStream(unsigned char dest, unsigned char port) {
        //printf("[A] N=%d Waiting to authenticate master node\n", ctx->getNetworkId());
        while(waitForMasterTrusted()) {
            ////printf("[A] N=%d StreamManager not present! \n", ctx->getNetworkId());
        }
        // TODO può lanciare eccezioni?
        try {
            /* Open a Stream to another node */
            //printf("[A] Opening stream to node %d\n", dest);
            int stream = connect(dest,                         // Destination node
                                port,                          // Destination port
                                clientParams);                 // Stream parameters 
                                //2*ctx->getDataSlotDuration()); // Wakeup advance (max 1 tile)

            return stream;

        } catch(exception& e) {
            //printf("exception %s\n",e.what());
            return -1;
        }
    }

    void send(int stream, unsigned int c) {
        // Put last sensor data timestamp as a network time, so that
        // the actuator will be able to compute the end-to-end sensor-actuator latency.
        // Reuse the network time field to send back the controller-actuator latency
        Data data(ctx->getNetworkId(), c, controllerActuatorLatency, sensorActuatorLatency);

        int ret = mxnet::write(stream, &data, sizeof(data));
        // if(ret >= 0) {
        //     printf("[A] Sent ID=%d Time=%lld NetTime=%lld Counter=%u\n",
        //            data.getId(), data.getTime(), data.getNetworkTime(), data.getCounter());
        // }
        // else
        //     printf("[E] Error sending data, result=%d\n", ret);
    }

    void actuate(int cv) {
        ////printf("[A] Actuate: cv=%d\n", cv);
        printf("%d\n", cv);
    }

    static void processThreadLauncher(void *arg) {
        auto* s = reinterpret_cast<StreamThreadPar*>(arg);
        auto* a = reinterpret_cast<ActuatorNode*>(s->obj);
        a->process(s);
    }
};